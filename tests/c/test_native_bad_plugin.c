#include "toy_module.h"

static toy_status unavailable(toy_state *state) {
    (void)state;
    return TOY_ERROR;
}

static const toy_native_word words[] = {
    {"unavailable", unavailable},
};

static const toy_module_export plugin = {
    TOY_MODULE_ABI_VERSION + 1,
    sizeof(toy_module_export),
    "test.bad",
    words,
    sizeof(words) / sizeof(words[0]),
};

TOY_MODULE_EXPORT const toy_module_export *toy_module_v1(
    const toy_module_api *api) {
    (void)api;
    return &plugin;
}
