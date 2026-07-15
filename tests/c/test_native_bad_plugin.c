#include "toy_module.h"

static toy_status unavailable(toy_state *state) {
    (void)state;
    return TOY_ERROR;
}

static const toy_native_word words[] = {
    {"unavailable", unavailable},
};

static const toy_module_export plugin = {
    sizeof(toy_module_export) + 1,
    "test.bad",
    words,
    sizeof(words) / sizeof(words[0]),
};

TOY_MODULE_EXPORT const toy_module_export *toy_module_init(
    uint32_t abi_version, const toy_module_api *api) {
    (void)abi_version;
    (void)api;
    return &plugin;
}
