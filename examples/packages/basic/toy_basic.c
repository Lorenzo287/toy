#define TOY_PACKAGE_IMPLEMENTATION
#include "toy_package.h"

static toy_status twice(toy_state *state) {
    int64_t value = 0;
    if (!toy_get_int(state, 0, &value)) {
        return toy_fail(state, "basic.twice expected an integer");
    }
    if (!toy_pop(state, 1)) {
        return toy_fail(state, "basic.twice failed to pop its input");
    }
    return toy_push_int(state, value * 2);
}

static const toy_native_word words[] = {
    {"twice", twice},
};

static const toy_package_export package = {
    sizeof(toy_package_export),
    "basic",
    words,
    sizeof(words) / sizeof(words[0]),
};

TOY_PACKAGE_EXPORT const toy_package_export *toy_package_init(
    uint32_t abi_version, const toy_package_api *api) {
    if (!toy_package_bind(abi_version, api)) return NULL;
    return &package;
}
