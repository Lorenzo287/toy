#ifdef _WIN32
#include <stdlib.h>
#else
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#endif

#include "toy.h"

#include <stdio.h>
#include <string.h>

#ifdef STB_LEAKCHECK
#include "tf_alloc.h"
#endif

#define CHECK(condition, message)                                         \
    do {                                                                  \
        if (!(condition)) {                                               \
            fprintf(stderr, "native loader check failed: %s\n", message); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

static void discard_diagnostic(void *userdata, const char *data,
                               size_t length) {
    (void)userdata;
    (void)data;
    (void)length;
}

static bool package_path(char *output, size_t capacity, const char *parent,
                         const char *name) {
    int written = snprintf(output, capacity, "%s/%s", parent, name);
    return written > 0 && (size_t)written < capacity;
}

int main(int argc, char **argv) {
    CHECK(argc == 2, "package parent directory argument");
    char plugin_path[1024];
    char bad_path[1024];
    CHECK(package_path(plugin_path, sizeof(plugin_path), argv[1], "plugin"),
          "construct plugin package path");
    CHECK(package_path(bad_path, sizeof(bad_path), argv[1], "bad"),
          "construct bad package path");

    toy_state_config config = {
        .diagnostic = discard_diagnostic,
    };
    toy_state *state = toy_state_new(&config);
    CHECK(state, "state creation");
    CHECK(toy_import_package(state, plugin_path, "p") == TOY_OK,
          "import shared native package");

    CHECK(toy_eval(state, "<native-loader>",
                   "21 p.double p.make-resource "
                   "[ 1 2 3 ] p.sequence-size "
                   "p.make-pair [ 7 9 ] ==") == TOY_OK,
          "load and call shared native package");

    const char *resource_type = NULL;
    int64_t integer = 0;
    bool boolean = false;
    CHECK(toy_stack_size(state) == 4, "native result stack size");
    CHECK(toy_get_bool(state, 0, &boolean) && boolean,
          "shared package collection construction");
    CHECK(toy_get_int(state, 1, &integer) && integer == 3,
          "shared package retained sequence access");
    CHECK(toy_get_resource_type(state, 2, &resource_type) &&
              strcmp(resource_type, "test.plugin.resource") == 0,
          "shared package resource result");
    CHECK(toy_get_int(state, 3, &integer) && integer == 42,
          "shared package integer result");
    CHECK(toy_pop(state, 4), "release native results");

    CHECK(toy_import_package(state, plugin_path, NULL) == TOY_OK,
          "import the same package under its declared name");
    CHECK(toy_eval(state, "<native-loader-repeat>",
                   "20 plugin.double") == TOY_OK,
          "call shared package through its declared name");
    CHECK(toy_get_int(state, 0, &integer) && integer == 40,
          "repeat shared package call");
    CHECK(toy_pop(state, 1), "release repeat result");

    CHECK(toy_eval(state, "<native-loader-error>",
                   "\"bad\" plugin.double") == TOY_ERROR,
          "shared native error");
    CHECK(toy_get_error(state) &&
              strcmp(toy_get_error(state),
                     "plugin.double expected an integer") == 0,
          "shared native diagnostic");
    CHECK(toy_pop(state, 1), "release rejected native input");

    CHECK(toy_import_package(state, bad_path, NULL) == TOY_ERROR,
          "reject incompatible shared package ABI");
    CHECK(toy_get_error(state) &&
              strstr(toy_get_error(state), "incompatible descriptor"),
          "incompatible ABI diagnostic");

    CHECK(toy_eval(state, "<native-loader-shutdown>",
                   "plugin.make-resource") == TOY_OK,
          "create shutdown resource");
    toy_state_free(state);

#ifdef STB_LEAKCHECK
    stb_leakcheck_dumpmem();
#endif
    return 0;
}
