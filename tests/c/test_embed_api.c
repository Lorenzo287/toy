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
        return toy_error(state, "host/double expected an integer");
    }
    if (!toy_pop(state, 1)) return toy_error(state, "host/double pop failed");
    return toy_push_int(state, value * 2);
}

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

    CHECK(toy_register_native(first, "host/double", host_double) == TOY_OK,
          "register native word");
    CHECK(toy_eval(first, "<native>", "21 host/double") == TOY_OK,
          "call native word");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "native word result");
    CHECK(toy_pop(first, 1), "pop native result");
    CHECK(toy_eval(first, "<native-error>", "\"bad\" host/double") ==
              TOY_ERROR,
          "native word error status");
    CHECK(toy_last_error(first) &&
              strcmp(toy_last_error(first),
                     "host/double expected an integer") == 0,
          "native word diagnostic");
    CHECK(toy_pop(first, 1), "pop rejected native input");

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
