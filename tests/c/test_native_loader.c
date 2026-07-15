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

static bool set_module_path(const char *path) {
#ifdef _WIN32
    return _putenv_s("TOY_MODULE_PATH", path) == 0;
#else
    return setenv("TOY_MODULE_PATH", path, 1) == 0;
#endif
}

int main(int argc, char **argv) {
    CHECK(argc == 2, "plugin directory argument");
    CHECK(set_module_path(argv[1]), "set module search path");

    toy_state_config config = {
        .diagnostic = discard_diagnostic,
    };
    toy_state *state = toy_state_new(&config);
    CHECK(state, "state creation");

    CHECK(toy_eval(state, "<native-loader>",
                   "\"test.plugin\" 'p require-as "
                   "21 p.double p.make-resource") == TOY_OK,
          "load and call shared native module");

    const char *resource_type = NULL;
    int64_t integer = 0;
    CHECK(toy_stack_size(state) == 2, "native result stack size");
    CHECK(toy_get_resource_type(state, 0, &resource_type) &&
              strcmp(resource_type, "test.plugin.resource") == 0,
          "shared module resource result");
    CHECK(toy_get_int(state, 1, &integer) && integer == 42,
          "shared module integer result");
    CHECK(toy_pop(state, 2), "release native results");

    CHECK(toy_eval(state, "<native-loader-repeat>",
                   "\"test.plugin\" require 20 test.plugin.double") ==
              TOY_OK,
          "require shared module once");
    CHECK(toy_get_int(state, 0, &integer) && integer == 40,
          "repeat shared module call");
    CHECK(toy_pop(state, 1), "release repeat result");

    CHECK(toy_eval(state, "<native-loader-error>",
                   "\"bad\" test.plugin.double") == TOY_ERROR,
          "shared native error");
    CHECK(toy_get_error(state) &&
              strcmp(toy_get_error(state),
                     "test.plugin.double expected an integer") == 0,
          "shared native diagnostic");
    CHECK(toy_pop(state, 1), "release rejected native input");

    CHECK(toy_eval(state, "<native-loader-abi>",
                   "\"test.bad\" require") == TOY_ERROR,
          "reject incompatible shared module ABI");
    CHECK(toy_get_error(state) &&
              strstr(toy_get_error(state), "incompatible descriptor"),
          "incompatible ABI diagnostic");

    CHECK(toy_eval(state, "<native-loader-shutdown>",
                   "test.plugin.make-resource") == TOY_OK,
          "create shutdown resource");
    toy_state_free(state);

#ifdef STB_LEAKCHECK
    stb_leakcheck_dumpmem();
#endif
    return 0;
}
