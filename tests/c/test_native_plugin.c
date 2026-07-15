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

static const toy_native_word plugin_words[] = {
    {"double", plugin_double},
    {"make-resource", plugin_make_resource},
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
