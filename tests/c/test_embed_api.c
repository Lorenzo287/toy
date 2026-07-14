#include "toy.h"

#include <stdio.h>
#include <stdlib.h>
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

typedef struct {
    char data[4096];
    size_t length;
} capture_buffer;

static void capture_write(void *userdata, const char *data, size_t length) {
    capture_buffer *buffer = userdata;
    size_t available = sizeof(buffer->data) - buffer->length;
    if (length > available) length = available;
    memcpy(buffer->data + buffer->length, data, length);
    buffer->length += length;
}

static void capture_clear(capture_buffer *buffer) {
    buffer->length = 0;
}

static bool capture_equals(capture_buffer *buffer, const char *expected,
                           size_t length) {
    return buffer->length == length &&
           memcmp(buffer->data, expected, length) == 0;
}

static bool capture_contains(capture_buffer *buffer, const char *expected) {
    size_t expected_len = strlen(expected);
    if (expected_len > buffer->length) return false;
    for (size_t i = 0; i <= buffer->length - expected_len; i++) {
        if (memcmp(buffer->data + i, expected, expected_len) == 0) return true;
    }
    return false;
}

static toy_status host_double(toy_state *state) {
    int64_t value = 0;
    if (!toy_get_int(state, 0, &value)) {
        return toy_set_error(state, "host.double expected an integer");
    }
    if (!toy_pop(state, 1)) {
        return toy_set_error(state, "host.double pop failed");
    }
    return toy_push_int(state, value * 2);
}

static void destroy_test_resource(void *resource, void *userdata) {
    int *destroy_count = userdata;
    (*destroy_count)++;
    free(resource);
}

static int failed_resource_destroy_count = 0;

static toy_status host_fail_resource(toy_state *state) {
    int *value = malloc(sizeof(*value));
    if (!value) return toy_set_error(state, "resource allocation failed");
    *value = 99;
    toy_status status = toy_push_resource(
        state, "host.failed", value, destroy_test_resource,
        &failed_resource_destroy_count);
    if (status != TOY_OK) {
        free(value);
        return status;
    }
    return toy_set_error(state, "failure after creating a resource");
}

static const toy_native_word host_words[] = {
    {"double", host_double},
    {"fail-resource", host_fail_resource},
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
    "host.tools",
    host_tools_words,
    sizeof(host_tools_words) / sizeof(host_tools_words[0]),
};

static const toy_native_word invalid_atomic_words[] = {
    {"ok", host_double},
    {"bad.name", host_double},
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
    capture_buffer first_output = {0};
    capture_buffer first_diagnostic = {0};
    capture_buffer second_output = {0};
    capture_buffer second_diagnostic = {0};
    toy_state_config first_config = {
        .output = capture_write,
        .output_userdata = &first_output,
        .diagnostic = capture_write,
        .diagnostic_userdata = &first_diagnostic,
    };
    toy_state_config second_config = {
        .output = capture_write,
        .output_userdata = &second_output,
        .diagnostic = capture_write,
        .diagnostic_userdata = &second_diagnostic,
    };
    toy_state *first = toy_state_new(&first_config);
    toy_state *second = toy_state_new(&second_config);
    CHECK(first && second, "state creation");

    CHECK(toy_eval(first, "<output>",
                   "\"hello\" print 4 5 \"{}+{}\" printf "
                   "1 \"two\" .s .S") == TOY_OK,
          "capture language output");
    const char expected_output[] =
        "hello\n4+5<2> 1 two \n<2> 1 \"two\" \n";
    CHECK(capture_equals(&first_output, expected_output,
                         sizeof(expected_output) - 1),
          "captured print, printf, and stack output");
    CHECK(first_diagnostic.length == 0,
          "ordinary output did not use diagnostic callback");
    CHECK(second_output.length == 0, "output remains state-local");
    CHECK(toy_pop(first, 2), "clear output test stack");

    capture_clear(&first_output);
    CHECK(toy_eval(first, "<binary-output>", "\"a\\x00b\" print") ==
              TOY_OK,
          "capture binary-safe output");
    const char expected_binary[] = {'a', '\0', 'b', '\n'};
    CHECK(capture_equals(&first_output, expected_binary,
                         sizeof(expected_binary)),
          "output callback receives explicit byte lengths");
    capture_clear(&first_output);

    CHECK(toy_eval(first, "<arithmetic>", "2 3 +") == TOY_OK,
          "evaluate arithmetic");
    int64_t integer = 0;
    CHECK(toy_stack_size(first) == 1, "arithmetic stack size");
    CHECK(toy_stack_type(first, 0) == TOY_TYPE_INT, "arithmetic result type");
    CHECK(toy_get_int(first, 0, &integer) && integer == 5,
          "arithmetic result value");
    CHECK(toy_pop(first, 1), "pop arithmetic result");

    CHECK(toy_register_module(first, &host_module) == TOY_OK,
          "register native module");
    CHECK(toy_eval(first, "<native>",
                   "\"host\" require 21 host.double") == TOY_OK,
          "require and call native module");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "native word result");
    CHECK(toy_pop(first, 1), "pop native result");
    CHECK(toy_eval(first, "<native-alias>",
                   "\"host\" 'h require-as 21 h.double") == TOY_OK,
          "alias native module");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "aliased native word result");
    CHECK(toy_pop(first, 1), "pop aliased native result");
    CHECK(toy_register_module(first, &host_tools_module) == TOY_OK,
          "register nested native module");
    CHECK(toy_eval(first, "<nested-native>",
                   "\"host.tools\" 'ht require-as 21 ht.double") == TOY_OK,
          "alias nested native module");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "nested native module result");
    CHECK(toy_pop(first, 1), "pop nested native module result");
    CHECK(toy_eval(first, "<native-error>", "\"bad\" host.double") ==
              TOY_ERROR,
          "native word error status");
    CHECK(toy_get_error(first) &&
              strcmp(toy_get_error(first),
                     "host.double expected an integer") == 0,
          "native word diagnostic");
    CHECK(toy_pop(first, 1), "pop rejected native input");

    CHECK(toy_register_module(first, &host_module) == TOY_ERROR,
          "reject duplicate native module");
    CHECK(toy_get_error(first) &&
              strstr(toy_get_error(first), "already registered"),
          "duplicate native module diagnostic");
    CHECK(toy_register_word(first, "host.extra", host_double) == TOY_ERROR,
          "standalone registration cannot enter a module namespace");
    CHECK(toy_get_error(first) &&
              strstr(toy_get_error(first), "belongs to registered module"),
          "native module namespace diagnostic");

    CHECK(toy_register_module(first, &invalid_atomic_module) == TOY_ERROR,
          "reject invalid native module atomically");
    CHECK(toy_get_error(first) &&
              strstr(toy_get_error(first), "invalid native word name"),
          "invalid native word diagnostic");
    CHECK(toy_call(first, "atomic.ok") == TOY_ERROR,
          "invalid module left no partial word");
    CHECK(toy_register_module(first, &atomic_module) == TOY_OK,
          "invalid module left no registry entry");
    CHECK(toy_eval(first, "<atomic-native>",
                   "\"atomic\" require 21 atomic.ok") == TOY_OK,
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
    CHECK(toy_register_module(first, &copied_module) == TOY_OK,
          "register module with transient names");
    copied_module_name[0] = 'X';
    copied_word_name[0] = 'X';
    CHECK(toy_eval(first, "<copied-native>",
                   "\"copied\" require 21 copied.double") == TOY_OK,
          "native module copied descriptor names");
    CHECK(toy_get_int(first, 0, &integer) && integer == 42,
          "copied native module result");
    CHECK(toy_pop(first, 1), "pop copied native module result");

    CHECK(toy_register_word(first, "legacy-double", host_double) == TOY_OK,
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
    CHECK(toy_get_error(second) &&
              strstr(toy_get_error(second), "undefined word 'update'"),
          "undefined-word diagnostic");
    CHECK(toy_call(second, "host.double") == TOY_ERROR,
          "native modules remain state-local");
    CHECK(second_diagnostic.length > 0,
          "second state receives its own diagnostics");

    CHECK(toy_eval(first, "<failure>", "1 0 /") == TOY_ERROR,
          "runtime failure status");
    CHECK(toy_get_error(first) && strstr(toy_get_error(first), "divide by zero"),
          "runtime failure diagnostic");
    CHECK(capture_contains(&first_diagnostic, "runtime error:") &&
              capture_contains(&first_diagnostic, "<failure>:1:5"),
          "runtime diagnostic callback");
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

    capture_clear(&first_diagnostic);
    CHECK(toy_eval(first, "<parse-error>", "[") == TOY_ERROR,
          "parse failure status");
    CHECK(toy_get_error(first) &&
              strcmp(toy_get_error(first),
                     "expected ']' but reached end of program at "
                     "<parse-error>:1:2") == 0,
          "parse failure diagnostic");
    const char expected_parse_diagnostic[] =
        "parsing error: expected ']' but reached end of program\n"
        "  at <parse-error>:1:2\n";
    CHECK(capture_equals(&first_diagnostic, expected_parse_diagnostic,
                         sizeof(expected_parse_diagnostic) - 1),
          "detailed parser diagnostic callback");

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

    size_t stack_before_resource = toy_stack_size(first);
    int rejected_resource = 0;
    CHECK(toy_push_resource(first, "", &rejected_resource,
                            destroy_test_resource, NULL) == TOY_ERROR,
          "reject empty resource type");
    CHECK(toy_push_resource(first, "bad type", &rejected_resource,
                            destroy_test_resource, NULL) == TOY_ERROR,
          "reject invalid resource type");
    CHECK(toy_push_resource(first, "host.widget", NULL,
                            destroy_test_resource, NULL) == TOY_ERROR,
          "reject null resource pointer");
    CHECK(toy_stack_size(first) == stack_before_resource,
          "rejected resources do not change the stack");

    int resource_destroy_count = 0;
    int *widget = malloc(sizeof(*widget));
    CHECK(widget, "allocate test resource");
    *widget = 42;
    char mutable_type[] = "host.widget";
    CHECK(toy_push_resource(first, mutable_type, widget,
                            destroy_test_resource,
                            &resource_destroy_count) == TOY_OK,
          "push resource");
    mutable_type[0] = 'X';
    CHECK(toy_stack_type(first, 0) == TOY_TYPE_RESOURCE,
          "resource stack type");

    void *borrowed_resource = NULL;
    const char *resource_type = NULL;
    CHECK(toy_get_resource(first, 0, "host.widget", &borrowed_resource) &&
              borrowed_resource == widget && *(int *)borrowed_resource == 42,
          "typed resource lookup");
    CHECK(!toy_get_resource(first, 0, "host.other", &borrowed_resource),
          "resource type mismatch");
    CHECK(toy_get_resource_type(first, 0, &resource_type) &&
              strcmp(resource_type, "host.widget") == 0,
          "resource type name is copied");

    CHECK(toy_eval(first, "<resource-introspection>",
                   "dup resource? dup type-of") == TOY_OK,
          "resource introspection");
    CHECK(toy_get_string(first, 0, &text, &length) && length == 4 &&
              memcmp(text, "bool", 4) == 0,
          "resource predicate result type");
    CHECK(toy_pop(first, 1), "pop resource predicate type");
    bool resource_predicate = false;
    CHECK(toy_get_bool(first, 0, &resource_predicate) && resource_predicate,
          "resource predicate result");
    CHECK(toy_pop(first, 1), "pop resource predicate");

    CHECK(toy_eval(first, "<resource-repr>", "dup repr") == TOY_OK,
          "resource representation");
    CHECK(toy_get_string(first, 0, &text, &length) &&
              length == strlen("<resource:host.widget>") &&
              memcmp(text, "<resource:host.widget>", length) == 0,
          "resource representation includes its type");
    CHECK(toy_pop(first, 1), "pop resource representation");

    CHECK(toy_eval(first, "<resource-identity>", "dup dup ==") == TOY_OK,
          "resource identity equality");
    CHECK(toy_get_bool(first, 0, &resource_predicate) && resource_predicate,
          "same resource wrapper compares equal");
    CHECK(toy_pop(first, 1), "pop resource equality result");

    CHECK(toy_eval(first, "<resource-unhashable>",
                   "#{ } over insert") == TOY_ERROR,
          "resource cannot be inserted into a set");
    CHECK(toy_get_error(first) &&
              strstr(toy_get_error(first),
                     "expected hashable set item, found resource"),
          "resource unhashable diagnostic");
    CHECK(toy_pop(first, 2), "clear rejected set insertion inputs");
    CHECK(resource_destroy_count == 0,
          "rejected set insertion preserves original resource");

    CHECK(toy_eval(first, "<resource-collection>",
                   "[ ] swap push-back") == TOY_OK,
          "store resource in a vector");
    CHECK(resource_destroy_count == 0,
          "collection retains resource ownership");
    CHECK(toy_pop(first, 1), "release resource collection");
    CHECK(resource_destroy_count == 1,
          "resource destructor runs once after final release");

    CHECK(toy_eval(first, "<resource-error>", "host.fail-resource") ==
              TOY_ERROR,
          "native error after creating resource");
    CHECK(failed_resource_destroy_count == 0 &&
              toy_stack_type(first, 0) == TOY_TYPE_RESOURCE,
          "resource survives non-transactional error stack effects");
    CHECK(toy_pop(first, 1), "release failed-call resource");
    CHECK(failed_resource_destroy_count == 1,
          "failed-call resource is destroyed once");

    int shutdown_destroy_count = 0;
    int *shutdown_resource = malloc(sizeof(*shutdown_resource));
    CHECK(shutdown_resource, "allocate shutdown resource");
    *shutdown_resource = 7;
    CHECK(toy_push_resource(second, "host.shutdown", shutdown_resource,
                            destroy_test_resource,
                            &shutdown_destroy_count) == TOY_OK,
          "push state-owned shutdown resource");

    toy_state_free(second);
    CHECK(shutdown_destroy_count == 1,
          "state shutdown destroys its remaining resources");
    toy_state_free(first);
#ifdef STB_LEAKCHECK
    stb_leakcheck_dumpmem();
#endif
    return 0;
}
