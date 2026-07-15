#include "toy_module.h"

#include <stdint.h>
#include <stdlib.h>

static toy_status plugin_double(toy_state *state) {
    int64_t value = 0;
    if (!toy_get_int(state, 0, &value)) {
        return toy_fail(state, "test.plugin.double expected an integer");
    }
    if (!toy_pop(state, 1)) {
        return toy_fail(state, "test.plugin.double failed to pop its input");
    }
    return toy_push_int(state, value * 2);
}

static void destroy_resource(void *resource, void *userdata) {
    (void)userdata;
    free(resource);
}

static toy_status plugin_make_resource(toy_state *state) {
    int *value = malloc(sizeof(*value));
    if (!value) return toy_fail(state, "test plugin allocation failed");
    *value = 42;
    toy_status status = toy_push_resource(
        state, "test.plugin.resource", value, destroy_resource, NULL);
    if (status != TOY_OK) free(value);
    return status;
}

static toy_status plugin_sequence_size(toy_state *state) {
    toy_value *sequence = toy_value_retain(state, 0);
    if (!sequence) {
        return toy_fail(state, "test.plugin.sequence-size expected a value");
    }

    size_t size = 0;
    if (!toy_sequence_size(sequence, &size) ||
        (uint64_t)size > INT64_MAX) {
        toy_value_release(sequence);
        return toy_fail(state,
                        "test.plugin.sequence-size expected a sequence");
    }
    if (!toy_pop(state, 1)) {
        toy_value_release(sequence);
        return toy_fail(state,
                        "test.plugin.sequence-size failed to pop its input");
    }
    toy_value_release(sequence);
    return toy_push_int(state, (int64_t)size);
}

static toy_status plugin_make_pair(toy_state *state) {
    toy_status status = toy_push_int(state, 7);
    if (status == TOY_OK) status = toy_push_int(state, 9);
    if (status == TOY_OK) status = toy_make_vector(state, 2);
    return status;
}

static const toy_native_word plugin_words[] = {
    {"double", plugin_double},
    {"make-resource", plugin_make_resource},
    {"sequence-size", plugin_sequence_size},
    {"make-pair", plugin_make_pair},
};

static const toy_module_export plugin = {
    TOY_MODULE_ABI_VERSION,
    sizeof(toy_module_export),
    "test.plugin",
    plugin_words,
    sizeof(plugin_words) / sizeof(plugin_words[0]),
};

TOY_MODULE_EXPORT const toy_module_export *toy_module_v1(
    const toy_module_api *api) {
    if (!toy_module_bind(api)) return NULL;
    return &plugin;
}
