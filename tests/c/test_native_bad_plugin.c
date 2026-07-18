#include "toy_package.h"

static toy_status unavailable(toy_state *state) {
    (void)state;
    return TOY_ERROR;
}

static const toy_native_word words[] = {
    {"unavailable", unavailable},
};

static const toy_package_export plugin = {
    sizeof(toy_package_export) + 1,
    "bad",
    words,
    sizeof(words) / sizeof(words[0]),
};

TOY_PACKAGE_EXPORT const toy_package_export *toy_package_init(
    uint32_t abi_version, const toy_package_api *api) {
    (void)abi_version;
    (void)api;
    return &plugin;
}
