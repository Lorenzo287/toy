#include "toy.h"

#include <stdio.h>
#include <string.h>

#ifdef STB_LEAKCHECK
#include "tf_alloc.h"
#endif

#define CHECK(condition, message)                                             \
    do {                                                                      \
        if (!(condition)) {                                                   \
            fprintf(stderr, "embed API check failed: %s\n", message);       \
            return 1;                                                         \
        }                                                                     \
    } while (0)

static toy_status host_double(toy_state *state) {
    int64_t value = 0;
    if (!toy_get_int(state, 0, &value)) {
        return toy_error(state, "host::double expected an integer");
    }
    if (!toy_pop(state, 1)) return toy_error(state, "host::double pop failed");
    return toy_push_int(state, value * 2);
}

static const toy_native_word host_words[] = {
    {"double", host_double},
};

static const toy_native_module host_module = {
    "host",
    host_words,
    sizeof(host_words) / sizeof(host_words[0]),
};

static const toy_native_word host_tools_words[] = {
    {"double", host_double},
};

static const toy_native_module host_tools_module = {
    "host::tools",
    host_tools_words,
    sizeof(host_tools_words) / sizeof(host_tools_words[0]),
};

static const toy_native_word invalid_atomic_words[] = {
    {"ok", host_double},
    {"bad/name", host_double},
};

static const toy_native_module invalid_atomic_module = {
    "atomic",
    invalid_atomic_words,
    sizeof(invalid_atomic_words) / sizeof(invalid_atomic_words[0]),
};

static const toy_native_word atomic_words[] = {
    {"ok", host_double},
};

static const toy_native_module atomic_module = {
    "atomic",
    atomic_words,
    sizeof(atomic_words) / sizeof(atomic_words[0]),
};

int main(void) {
    toy_state *first = toy_state_new();
    toy_state *second = toy_state_new();
    CHECK(first && second, "state creation");

    CHECK(toy_eval(first, "<arithmetic>", "2 3 +") == TOY_OK,
          "evaluate arithmetic");
    int64_t integer = 0;
    CHECK(toy_stack_size(first) == 1, "arithmetic stack size");
    CHECK(toy_stack_type(first, 0) == TOY_TYPE_INT, "arithmetic result type");
    CHECK(toy_get_int(first, 0, &integer) && integer == 5,
          "arithmetic result value");
    CHECK(toy_pop(first, 1), "pop arithmetic result");

    CHECK(toy_register_native_module(first, &host_module) == TOY_OK,
          "register native module");
    CHECK(toy_eval(first, "<native>",
                   "\"host\" require 21 host::double") == TOY_OK,
          "require and call native module");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "native word result");
    CHECK(toy_pop(first, 1), "pop native result");
    CHECK(toy_eval(first, "<native-alias>",
                   "\"host\" 'h require-as 21 h::double") == TOY_OK,
          "alias native module");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "aliased native word result");
    CHECK(toy_pop(first, 1), "pop aliased native result");
    CHECK(toy_register_native_module(first, &host_tools_module) == TOY_OK,
          "register nested native module");
    CHECK(toy_eval(first, "<nested-native>",
                   "\"host::tools\" 'ht require-as 21 ht::double") == TOY_OK,
          "alias nested native module");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "nested native module result");
    CHECK(toy_pop(first, 1), "pop nested native module result");
    CHECK(toy_eval(first, "<native-error>", "\"bad\" host::double") ==
              TOY_ERROR,
          "native word error status");
    CHECK(toy_last_error(first) &&
              strcmp(toy_last_error(first),
                     "host::double expected an integer") == 0,
          "native word diagnostic");
    CHECK(toy_pop(first, 1), "pop rejected native input");

    CHECK(toy_register_native_module(first, &host_module) == TOY_ERROR,
          "reject duplicate native module");
    CHECK(toy_last_error(first) &&
              strstr(toy_last_error(first), "already registered"),
          "duplicate native module diagnostic");
    CHECK(toy_register_native(first, "host::extra", host_double) == TOY_ERROR,
          "standalone registration cannot enter a module namespace");
    CHECK(toy_last_error(first) &&
              strstr(toy_last_error(first), "belongs to registered module"),
          "native module namespace diagnostic");

    CHECK(toy_register_native_module(first, &invalid_atomic_module) ==
              TOY_ERROR,
          "reject invalid native module atomically");
    CHECK(toy_last_error(first) &&
              strstr(toy_last_error(first), "invalid native word name"),
          "invalid native word diagnostic");
    CHECK(toy_call(first, "atomic::ok") == TOY_ERROR,
          "invalid module left no partial word");
    CHECK(toy_register_native_module(first, &atomic_module) == TOY_OK,
          "invalid module left no registry entry");
    CHECK(toy_eval(first, "<atomic-native>",
                   "\"atomic\" require 21 atomic::ok") == TOY_OK,
          "call module after atomic retry");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "atomic retry result");
    CHECK(toy_pop(first, 1), "pop atomic retry result");

    char copied_module_name[] = "copied";
    char copied_word_name[] = "double";
    toy_native_word copied_words[] = {
        {copied_word_name, host_double},
    };
    toy_native_module copied_module = {
        copied_module_name,
        copied_words,
        sizeof(copied_words) / sizeof(copied_words[0]),
    };
    CHECK(toy_register_native_module(first, &copied_module) == TOY_OK,
          "register module with transient names");
    copied_module_name[0] = 'X';
    copied_word_name[0] = 'X';
    CHECK(toy_eval(first, "<copied-native>",
                   "\"copied\" require 21 copied::double") == TOY_OK,
          "native module copied descriptor names");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "copied native module result");
    CHECK(toy_pop(first, 1), "pop copied native module result");

    CHECK(toy_register_native(first, "legacy-double", host_double) == TOY_OK,
          "register standalone native word");
    CHECK(toy_eval(first, "<standalone-native>", "21 legacy-double") ==
              TOY_OK,
          "call standalone native word");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "standalone native word result");
    CHECK(toy_pop(first, 1), "pop standalone native result");

    CHECK(toy_eval(first, "<definition>", "'update [ 1 + ] def") == TOY_OK,
          "define Toy word");
    CHECK(toy_push_int(first, 41) == TOY_OK, "push host argument");
    CHECK(toy_call(first, "update") == TOY_OK, "call Toy word from host");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "host call result");
    CHECK(toy_pop(first, 1), "pop host call result");

    CHECK(toy_call(second, "update") == TOY_ERROR,
          "definitions remain state-local");
    CHECK(toy_last_error(second) &&
              strstr(toy_last_error(second), "undefined word 'update'"),
          "undefined-word diagnostic");
    CHECK(toy_call(second, "host::double") == TOY_ERROR,
          "native modules remain state-local");

    CHECK(toy_eval(first, "<failure>", "1 0 /") == TOY_ERROR,
          "runtime failure status");
    CHECK(toy_last_error(first) && strstr(toy_last_error(first), "divide by zero"),
          "runtime failure diagnostic");
    CHECK(toy_pop(first, 2), "clear failed operation inputs");
    CHECK(toy_eval(first, "<recovery>", "4 5 +") == TOY_OK,
          "state reuse after failure");
    CHECK(toy_get_int(first, 0, &integer) && integer == 9,
          "recovery result");
    CHECK(toy_pop(first, 1), "pop recovery result");

    CHECK(toy_eval(first, "<exit>", "exit") == TOY_EXIT_REQUESTED,
          "embedded exit request");
    CHECK(toy_eval(first, "<after-exit>", "6 7 +") == TOY_OK,
          "state survives exit request");
    CHECK(toy_get_int(first, 0, &integer) && integer == 13,
          "post-exit result");
    CHECK(toy_pop(first, 1), "pop post-exit result");

    toy_interrupt(first);
    CHECK(toy_eval(first, "<interrupt>", "99") == TOY_INTERRUPTED,
          "per-state interrupt request");
    CHECK(toy_stack_size(first) == 0, "interrupted source did not execute");
    CHECK(toy_eval(first, "<after-interrupt>", "8 9 +") == TOY_OK,
          "state survives interrupt request");
    CHECK(toy_get_int(first, 0, &integer) && integer == 17,
          "post-interrupt result");
    CHECK(toy_pop(first, 1), "pop post-interrupt result");

    CHECK(toy_eval(first, "<parse-error>", "[") == TOY_ERROR,
          "parse failure status");
    CHECK(toy_last_error(first) &&
              strcmp(toy_last_error(first), "source parsing failed") == 0,
          "parse failure diagnostic");

    CHECK(toy_push_bool(first, true) == TOY_OK, "push bool");
    bool boolean = false;
    CHECK(toy_get_bool(first, 0, &boolean) && boolean, "get bool");
    CHECK(toy_pop(first, 1), "pop bool");

    CHECK(toy_push_float(first, 2.5) == TOY_OK, "push float");
    double floating = 0.0;
    CHECK(toy_get_float(first, 0, &floating) && floating == 2.5, "get float");
    CHECK(toy_pop(first, 1), "pop float");

    CHECK(toy_push_string(first, "hello", 5) == TOY_OK, "push string");
    const char *text = NULL;
    size_t length = 0;
    CHECK(toy_get_string(first, 0, &text, &length) && length == 5 &&
              memcmp(text, "hello", 5) == 0,
          "get string");
    CHECK(toy_pop(first, 1), "pop string");

    toy_state_free(second);
    toy_state_free(first);
#ifdef STB_LEAKCHECK
    stb_leakcheck_dumpmem();
#endif
    return 0;
}
