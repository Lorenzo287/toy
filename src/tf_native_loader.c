#include "tf_native_loader.h"

#include "tf_alloc.h"
#include "toy_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef TOY_SHARED_SUFFIX
#define TOY_SHARED_SUFFIX ".dll"
#endif
#define TOY_PATH_SEPARATOR ';'

static void *library_open(const char *path) {
    return (void *)LoadLibraryA(path);
}

static toy_module_entry library_entry(void *handle) {
    FARPROC symbol = GetProcAddress((HMODULE)handle, TOY_MODULE_ENTRY_SYMBOL);
    toy_module_entry entry = NULL;
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
#ifndef TOY_SHARED_SUFFIX
#ifdef __APPLE__
#define TOY_SHARED_SUFFIX ".dylib"
#else
#define TOY_SHARED_SUFFIX ".so"
#endif
#endif
#define TOY_PATH_SEPARATOR ':'

static void *library_open(const char *path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static toy_module_entry library_entry(void *handle) {
    dlerror();
    void *symbol = dlsym(handle, TOY_MODULE_ENTRY_SYMBOL);
    toy_module_entry entry = NULL;
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

static const toy_module_api module_api = {
    .struct_size = sizeof(toy_module_api),
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

static bool file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

static char *join_path(const char *directory, const char *filename) {
    size_t directory_len = strlen(directory);
    size_t filename_len = strlen(filename);
    bool needs_separator = directory_len > 0 &&
                           directory[directory_len - 1] != '/' &&
                           directory[directory_len - 1] != '\\';
    char *path = tf_xmalloc(directory_len + (needs_separator ? 1 : 0) +
                            filename_len + 1);
    memcpy(path, directory, directory_len);
    size_t out = directory_len;
    if (needs_separator) path[out++] = '/';
    memcpy(path + out, filename, filename_len + 1);
    return path;
}

static char *module_filename(const char *name, size_t name_len) {
    static const char prefix[] = "toy_";
    size_t prefix_len = sizeof(prefix) - 1;
    size_t suffix_len = sizeof(TOY_SHARED_SUFFIX) - 1;
    char *filename = tf_xmalloc(prefix_len + name_len + suffix_len + 1);
    memcpy(filename, prefix, prefix_len);
    memcpy(filename + prefix_len, name, name_len);
    memcpy(filename + prefix_len + name_len, TOY_SHARED_SUFFIX,
           suffix_len + 1);
    return filename;
}

static void remember_library(tf_ctx *ctx, void *handle) {
    tf_native_library_table *libraries = &ctx->native_libraries;
    if (libraries->len >= libraries->cap) {
        libraries->cap = libraries->cap ? libraries->cap * 2 : 4;
        libraries->handles = tf_xrealloc(
            libraries->handles, sizeof(void *) * libraries->cap);
    }
    libraries->handles[libraries->len++] = handle;
}

static tf_native_module_status load_candidate(tf_ctx *ctx, const char *name,
                                              const char *path) {
    if (!file_exists(path)) return TF_NATIVE_MODULE_NOT_FOUND;

    void *handle = library_open(path);
    if (!handle) {
        tf_ctx_runtime_errorf(ctx, "failed to open native module '%s': %s\n",
                              path, library_error());
        return TF_NATIVE_MODULE_ERROR;
    }

    toy_module_entry entry = library_entry(handle);
    if (!entry) {
        tf_ctx_runtime_errorf(
            ctx, "native module '%s' does not export %s: %s\n", path,
            TOY_MODULE_ENTRY_SYMBOL, library_error());
        library_close(handle);
        return TF_NATIVE_MODULE_ERROR;
    }

    const toy_module_export *exported =
        entry(TOY_MODULE_ABI_VERSION, &module_api);
    if (!exported) {
        tf_ctx_runtime_errorf(ctx,
                              "native module '%s' rejected the host ABI\n",
                              path);
        library_close(handle);
        return TF_NATIVE_MODULE_ERROR;
    }
    if (exported->struct_size != sizeof(toy_module_export)) {
        tf_ctx_runtime_errorf(ctx,
                              "native module '%s' has an incompatible "
                              "descriptor\n",
                              path);
        library_close(handle);
        return TF_NATIVE_MODULE_ERROR;
    }
    if (!exported->name || strcmp(exported->name, name) != 0) {
        tf_ctx_runtime_errorf(
            ctx, "native module '%s' exports '%s', expected '%s'\n", path,
            exported->name ? exported->name : "<missing>", name);
        library_close(handle);
        return TF_NATIVE_MODULE_ERROR;
    }

    toy_native_module module = {
        exported->name,
        exported->words,
        exported->word_count,
    };
    if (tf_register_module(ctx, &module) != TOY_OK) {
        library_close(handle);
        return TF_NATIVE_MODULE_ERROR;
    }

    remember_library(ctx, handle);
    return TF_NATIVE_MODULE_LOADED;
}

static tf_native_module_status load_from_directory(tf_ctx *ctx,
                                                   const char *name,
                                                   const char *directory,
                                                   const char *filename) {
    if (!directory || directory[0] == '\0') {
        return TF_NATIVE_MODULE_NOT_FOUND;
    }
    char *path = join_path(directory, filename);
    tf_native_module_status status = load_candidate(ctx, name, path);
    free(path);
    return status;
}

tf_native_module_status tf_native_module_load(tf_ctx *ctx, const char *name,
                                              size_t name_len,
                                              const char *source_directory) {
    char *filename = module_filename(name, name_len);
    tf_native_module_status status = load_from_directory(
        ctx, name, source_directory, filename);
    if (status != TF_NATIVE_MODULE_NOT_FOUND) {
        free(filename);
        return status;
    }

    const char *search_path = getenv("TOY_MODULE_PATH");
    const char *segment = search_path;
    while (segment && *segment) {
        const char *end = strchr(segment, TOY_PATH_SEPARATOR);
        size_t length = end ? (size_t)(end - segment) : strlen(segment);
        if (length > 0) {
            char *directory = tf_xmalloc(length + 1);
            memcpy(directory, segment, length);
            directory[length] = '\0';
            status = load_from_directory(ctx, name, directory, filename);
            free(directory);
            if (status != TF_NATIVE_MODULE_NOT_FOUND) {
                free(filename);
                return status;
            }
        }
        segment = end ? end + 1 : NULL;
    }

    status = load_from_directory(ctx, name, ".", filename);
    free(filename);
    return status;
}

void tf_native_modules_close(tf_ctx *ctx) {
    if (!ctx) return;
    tf_native_library_table *libraries = &ctx->native_libraries;
    while (libraries->len > 0) {
        library_close(libraries->handles[--libraries->len]);
    }
    free(libraries->handles);
    libraries->handles = NULL;
    libraries->cap = 0;
}
