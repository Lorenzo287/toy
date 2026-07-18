#include "tf_native_loader.h"

#include "tf_alloc.h"
#include "toy_package.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static void *library_open(const char *path) {
    return (void *)LoadLibraryA(path);
}

static toy_package_entry library_entry(void *handle) {
    FARPROC symbol = GetProcAddress((HMODULE)handle, TOY_PACKAGE_ENTRY_SYMBOL);
    toy_package_entry entry = NULL;
    _Static_assert(sizeof(entry) == sizeof(symbol),
                   "function pointer size mismatch");
    memcpy(&entry, &symbol, sizeof(entry));
    return entry;
}

static const char *library_error(void) {
    static char message[512];
    DWORD code = GetLastError();
    DWORD length = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code,
        0, message, (DWORD)sizeof(message), NULL);
    while (length > 0 &&
           (message[length - 1] == '\r' || message[length - 1] == '\n')) {
        message[--length] = '\0';
    }
    return length > 0 ? message : "unknown platform error";
}

static void library_close(void *handle) {
    FreeLibrary((HMODULE)handle);
}
#else
#include <dlfcn.h>
static void *library_open(const char *path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static toy_package_entry library_entry(void *handle) {
    dlerror();
    void *symbol = dlsym(handle, TOY_PACKAGE_ENTRY_SYMBOL);
    toy_package_entry entry = NULL;
    _Static_assert(sizeof(entry) == sizeof(symbol),
                   "function pointer size mismatch");
    memcpy(&entry, &symbol, sizeof(entry));
    return entry;
}

static const char *library_error(void) {
    const char *message = dlerror();
    return message ? message : "unknown platform error";
}

static void library_close(void *handle) {
    dlclose(handle);
}
#endif

static const toy_package_api package_api = {
    .struct_size = sizeof(toy_package_api),
    .stack_size = toy_stack_size,
    .stack_type = toy_stack_type,
    .get_bool = toy_get_bool,
    .get_int = toy_get_int,
    .get_float = toy_get_float,
    .get_string = toy_get_string,
    .get_resource = toy_get_resource,
    .get_resource_type = toy_get_resource_type,
    .pop = toy_pop,
    .push_bool = toy_push_bool,
    .push_int = toy_push_int,
    .push_float = toy_push_float,
    .push_string = toy_push_string,
    .push_resource = toy_push_resource,
    .get_error = toy_get_error,
    .clear_error = toy_clear_error,
    .fail = toy_fail,
    .interrupt = toy_interrupt,
    .value_retain = toy_value_retain,
    .value_release = toy_value_release,
    .value_type = toy_value_type,
    .value_get_bool = toy_value_get_bool,
    .value_get_int = toy_value_get_int,
    .value_get_float = toy_value_get_float,
    .value_get_string = toy_value_get_string,
    .value_get_resource = toy_value_get_resource,
    .value_get_resource_type = toy_value_get_resource_type,
    .push_value = toy_push_value,
    .sequence_size = toy_sequence_size,
    .sequence_get = toy_sequence_get,
    .map_size = toy_map_size,
    .map_entry = toy_map_entry,
    .make_vector = toy_make_vector,
    .make_map = toy_make_map,
    .call_value = toy_call_value,
};

static void remember_library(tf_ctx *ctx, void *handle) {
    tf_native_library_table *libraries = &ctx->native_libraries;
    if (libraries->len >= libraries->cap) {
        libraries->cap = libraries->cap ? libraries->cap * 2 : 4;
        libraries->handles = tf_xrealloc(
            libraries->handles, sizeof(void *) * libraries->cap);
    }
    libraries->handles[libraries->len++] = handle;
}

tf_ret tf_native_package_load(tf_ctx *ctx, size_t package_index,
                              const char *path) {
    void *handle = library_open(path);
    if (!handle) {
        tf_ctx_runtime_errorf(ctx, "failed to open native package '%s': %s\n",
                              path, library_error());
        return TF_ERR;
    }

    toy_package_entry entry = library_entry(handle);
    if (!entry) {
        tf_ctx_runtime_errorf(
            ctx, "native package '%s' does not export %s: %s\n", path,
            TOY_PACKAGE_ENTRY_SYMBOL, library_error());
        library_close(handle);
        return TF_ERR;
    }

    const toy_package_export *exported =
        entry(TOY_PACKAGE_ABI_VERSION, &package_api);
    if (!exported) {
        tf_ctx_runtime_errorf(ctx,
                              "native package '%s' rejected the host ABI\n",
                              path);
        library_close(handle);
        return TF_ERR;
    }
    if (exported->struct_size != sizeof(toy_package_export)) {
        tf_ctx_runtime_errorf(ctx,
                              "native package '%s' has an incompatible "
                              "descriptor\n",
                              path);
        library_close(handle);
        return TF_ERR;
    }

    toy_native_package package = {
        exported->name,
        exported->words,
        exported->word_count,
    };
    if (tf_install_native_package(ctx, package_index, &package) != TOY_OK) {
        library_close(handle);
        return TF_ERR;
    }

    remember_library(ctx, handle);
    return TF_OK;
}

void tf_native_packages_close(tf_ctx *ctx) {
    if (!ctx) return;
    tf_native_library_table *libraries = &ctx->native_libraries;
    while (libraries->len > 0) {
        library_close(libraries->handles[--libraries->len]);
    }
    free(libraries->handles);
    libraries->handles = NULL;
    libraries->cap = 0;
}
