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

#define CHECK(condition, message)                                          \
    do {                                                                   \
        if (!(condition)) {                                                \
            fprintf(stderr, "generated binding check failed: %s\n", message); \
            return 1;                                                      \
        }                                                                  \
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
    CHECK(argc == 2, "module directory argument");
    CHECK(set_module_path(argv[1]), "set module search path");

    toy_state_config config = {
        .diagnostic = discard_diagnostic,
    };
    toy_state *state = toy_state_new(&config);
    CHECK(state, "state creation");
    CHECK(toy_eval(state, "<bindgen-require>",
                   "\"test.bindgen\" 'b require-as") == TOY_OK,
          "load generated module");

    int64_t integer = 0;
    CHECK(toy_eval(state, "<bindgen-add>", "20 22 b.add") == TOY_OK,
          "call generated integer wrapper");
    CHECK(toy_get_int(state, 0, &integer) && integer == 42,
          "generated integer result");
    CHECK(toy_pop(state, 1), "pop generated integer result");

    CHECK(toy_eval(state, "<bindgen-narrow>", "b.negative-i8") == TOY_OK,
          "call generated narrow wrapper");
    CHECK(toy_get_int(state, 0, &integer) && integer == -7,
          "generated narrow result");
    CHECK(toy_pop(state, 1), "pop generated narrow result");

    CHECK(toy_eval(state, "<bindgen-float>", "2.5 4 b.scale") == TOY_OK,
          "call generated floating wrapper");
    double floating = 0.0;
    CHECK(toy_get_float(state, 0, &floating) && floating == 10.5,
          "generated floating result");
    CHECK(toy_pop(state, 1), "pop generated floating result");

    CHECK(toy_eval(state, "<bindgen-string-argument>",
                   "\"hello\" b.text-length") == TOY_OK,
          "call generated string wrapper");
    CHECK(toy_get_int(state, 0, &integer) && integer == 5,
          "generated string length result");
    CHECK(toy_pop(state, 1), "pop generated string length");

    CHECK(toy_eval(state, "<bindgen-string-result>", "b.greeting") == TOY_OK,
          "call generated string-return wrapper");
    const char *text = NULL;
    size_t text_length = 0;
    CHECK(toy_get_string(state, 0, &text, &text_length) &&
              text_length == strlen("hello from generated C") &&
              memcmp(text, "hello from generated C", text_length) == 0,
          "generated string result");
    CHECK(toy_pop(state, 1), "pop generated string result");

    CHECK(toy_eval(state, "<bindgen-bool>", "true b.not") == TOY_OK,
          "call generated bool wrapper");
    bool boolean = true;
    CHECK(toy_get_bool(state, 0, &boolean) && !boolean,
          "generated bool result");
    CHECK(toy_pop(state, 1), "pop generated bool result");

    CHECK(toy_eval(state, "<bindgen-void>", "7 b.ignore") == TOY_OK,
          "call generated void wrapper");
    CHECK(toy_stack_size(state) == 0, "generated void result");

    CHECK(toy_eval(state, "<bindgen-type-error>",
                   "\"bad\" 2 b.add") == TOY_ERROR,
          "generated wrapper rejects a bad argument");
    CHECK(toy_get_error(state) && strstr(toy_get_error(state),
                                         "argument 1 expected i32"),
          "generated argument diagnostic");
    CHECK(toy_pop(state, 2), "pop rejected generated inputs");

    CHECK(toy_eval(state, "<bindgen-return-range>", "b.too-large") ==
              TOY_ERROR,
          "generated wrapper rejects an unrepresentable result");
    CHECK(toy_get_error(state) &&
              strstr(toy_get_error(state), "outside Toy's range"),
          "generated return range diagnostic");
    CHECK(toy_stack_size(state) == 0, "generated return failure stack");

    CHECK(toy_eval(state, "<bindgen-resource-create>",
                   "42 b.make-box") == TOY_OK,
          "generated owned resource return");
    CHECK(toy_stack_type(state, 0) == TOY_TYPE_RESOURCE,
          "generated resource result type");
    const char *resource_type = NULL;
    CHECK(toy_get_resource_type(state, 0, &resource_type) &&
              strcmp(resource_type, "test.bindgen.box") == 0,
          "generated resource type name");
    CHECK(toy_eval(state, "<bindgen-resource-use>",
                   "dup b.box-value") == TOY_OK,
          "generated resource argument");
    CHECK(toy_get_int(state, 0, &integer) && integer == 42,
          "generated resource value");
    CHECK(toy_pop(state, 1), "pop generated resource value");

    CHECK(toy_eval(state, "<bindgen-resource-status>",
                   "dup 84 b.set-box dup b.box-value") == TOY_OK,
          "generated status return consumes resource argument");
    CHECK(toy_get_int(state, 0, &integer) && integer == 84,
          "generated status mutation result");
    CHECK(toy_pop(state, 1), "pop generated status result");
    CHECK(toy_eval(state, "<bindgen-borrowed-result>",
                   "b.box-label") == TOY_OK,
          "copy borrowed result before consuming resource");
    CHECK(toy_get_string(state, 0, &text, &text_length) &&
              text_length == strlen("box-84") &&
              memcmp(text, "box-84", text_length) == 0,
          "generated borrowed string result");
    CHECK(toy_pop(state, 1), "pop generated borrowed result");
    CHECK(toy_eval(state, "<bindgen-resource-destroyed>",
                   "b.box-destroy-count b.box-live-count") == TOY_OK,
          "generated resource destructor counters");
    CHECK(toy_get_int(state, 1, &integer) && integer == 1,
          "generated destructor called once");
    CHECK(toy_get_int(state, 0, &integer) && integer == 0,
          "generated resource no longer live");
    CHECK(toy_pop(state, 2), "pop generated resource counters");

    CHECK(toy_eval(state, "<bindgen-output-resource>",
                   "7 b.open-box b.box-label") == TOY_OK,
          "generated output resource");
    CHECK(toy_get_string(state, 0, &text, &text_length) &&
              text_length == strlen("box-7") &&
              memcmp(text, "box-7", text_length) == 0,
          "generated output resource value");
    CHECK(toy_pop(state, 1), "pop output resource result");
    CHECK(toy_eval(state, "<bindgen-output-destroyed>",
                   "b.box-destroy-count") == TOY_OK,
          "output resource destructor count");
    CHECK(toy_get_int(state, 0, &integer) && integer == 2,
          "output resource destroyed once");
    CHECK(toy_pop(state, 1), "pop output destructor count");

    CHECK(toy_eval(state, "<bindgen-alternate-success>",
                   "8 b.open-box-alternate b.box-value") == TOY_OK,
          "generated alternate success status");
    CHECK(toy_get_int(state, 0, &integer) && integer == 8,
          "alternate success output value");
    CHECK(toy_pop(state, 1), "pop alternate success value");

    CHECK(toy_eval(state, "<bindgen-output-status-failure>",
                   "9 b.open-box-fail") == TOY_ERROR,
          "generated output status failure");
    CHECK(toy_get_error(state) &&
              strstr(toy_get_error(state), "failed with status 17"),
          "generated status failure diagnostic");
    CHECK(toy_stack_size(state) == 0,
          "status failure consumes valid inputs");
    CHECK(toy_eval(state, "<bindgen-failed-output-cleanup>",
                   "b.box-destroy-count b.box-live-count") == TOY_OK,
          "failed output resource cleanup counters");
    CHECK(toy_get_int(state, 1, &integer) && integer == 4,
          "failed output resource destroyed");
    CHECK(toy_get_int(state, 0, &integer) && integer == 0,
          "failed output resource no longer live");
    CHECK(toy_pop(state, 2), "pop failed output counters");

    CHECK(toy_eval(state, "<bindgen-null-resource-return>",
                   "-1 b.make-box") == TOY_ERROR,
          "generated null owned return rejected");
    CHECK(toy_get_error(state) &&
              strstr(toy_get_error(state), "null test.bindgen.box"),
          "generated null owned return diagnostic");
    CHECK(toy_stack_size(state) == 0,
          "null owned return leaves no resource");
    CHECK(toy_eval(state, "<bindgen-null-output-resource>",
                   "b.open-box-empty") == TOY_ERROR,
          "generated null output rejected");
    CHECK(toy_get_error(state) &&
              strstr(toy_get_error(state), "null test.bindgen.box"),
          "generated null output diagnostic");

    int foreign_box = 0;
    CHECK(toy_push_resource(state, "host.box", &foreign_box, NULL, NULL) ==
              TOY_OK,
          "push mismatched resource");
    CHECK(toy_eval(state, "<bindgen-wrong-resource>",
                   "b.box-value") == TOY_ERROR,
          "generated exact resource type check");
    CHECK(toy_get_error(state) &&
              strstr(toy_get_error(state),
                     "argument 1 expected test.bindgen.box"),
          "generated resource argument diagnostic");
    CHECK(toy_pop(state, 1), "pop rejected resource");

    toy_state_free(state);
#ifdef STB_LEAKCHECK
    stb_leakcheck_dumpmem();
#endif
    return 0;
}
