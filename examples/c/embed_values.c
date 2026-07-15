#include "toy.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int report_error(toy_state *state, const char *operation) {
    const char *message = toy_get_error(state);
    fprintf(stderr, "%s failed: %s\n", operation,
            message ? message : "no diagnostic available");
    return 1;
}

static int print_summary(const toy_value *summary) {
    size_t size = 0;
    if (!toy_map_size(summary, &size)) return 0;

    for (size_t i = 0; i < size; i++) {
        toy_value *key = NULL;
        toy_value *value = NULL;
        if (!toy_map_entry(summary, i, &key, &value)) return 0;

        const char *name = NULL;
        size_t name_length = 0;
        bool ok = toy_value_get_string(key, &name, &name_length);
        if (ok) printf("%.*s = ", (int)name_length, name);

        int64_t integer = 0;
        const char *text = NULL;
        size_t text_length = 0;
        if (ok && toy_value_get_int(value, &integer)) {
            printf("%" PRId64 "\n", integer);
        } else if (ok &&
                   toy_value_get_string(value, &text, &text_length)) {
            printf("%.*s\n", (int)text_length, text);
        } else {
            ok = false;
        }

        toy_value_release(value);
        toy_value_release(key);
        if (!ok) return 0;
    }
    return 1;
}

int main(void) {
    toy_state *state = toy_state_new(NULL);
    if (!state) {
        fputs("failed to create Toy state\n", stderr);
        return 1;
    }

    const char *program =
        "'summarize [\n"
        "    | profile |\n"
        "    { }\n"
        "    \"name\" $profile \"name\" get assoc\n"
        "    \"total\" $profile \"scores\" get 0 swap [ + ] fold assoc\n"
        "] def\n";
    if (toy_eval(state, "embed-values.toy", program) != TOY_OK) {
        int result = report_error(state, "loading Toy code");
        toy_state_free(state);
        return result;
    }

    toy_push_int(state, 3);
    toy_push_int(state, 4);
    toy_push_int(state, 5);
    if (toy_make_vector(state, 3) != TOY_OK) {
        int result = report_error(state, "constructing scores");
        toy_state_free(state);
        return result;
    }
    toy_value *scores = toy_value_retain(state, 0);
    toy_pop(state, 1);

    toy_push_string(state, "name", 4);
    toy_push_string(state, "Ada", 3);
    toy_push_string(state, "scores", 6);
    toy_push_value(state, scores);
    toy_value_release(scores);
    if (toy_make_map(state, 2) != TOY_OK ||
        toy_call(state, "summarize") != TOY_OK) {
        int result = report_error(state, "summarizing profile");
        toy_state_free(state);
        return result;
    }

    toy_value *summary = toy_value_retain(state, 0);
    toy_pop(state, 1);
    if (!summary || !print_summary(summary)) {
        fputs("Toy returned an unexpected summary\n", stderr);
        toy_value_release(summary);
        toy_state_free(state);
        return 1;
    }
    toy_value_release(summary);

    if (toy_eval(state, "callback.toy", "[ 3 * ]") != TOY_OK) {
        int result = report_error(state, "creating callback");
        toy_state_free(state);
        return result;
    }
    toy_value *callback = toy_value_retain(state, 0);
    toy_pop(state, 1);

    toy_push_int(state, 14);
    if (!callback || toy_call_value(state, callback) != TOY_OK) {
        toy_value_release(callback);
        int result = report_error(state, "calling retained quotation");
        toy_state_free(state);
        return result;
    }

    int64_t result = 0;
    if (!toy_get_int(state, 0, &result)) {
        fputs("retained quotation returned an unexpected value\n", stderr);
        toy_value_release(callback);
        toy_state_free(state);
        return 1;
    }
    printf("callback(14) = %" PRId64 "\n", result);
    toy_value_release(callback);
    toy_state_free(state);
    return 0;
}
