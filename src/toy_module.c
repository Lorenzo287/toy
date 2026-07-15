#include "toy_module.h"

static const toy_module_api *host = NULL;

bool toy_module_bind(const toy_module_api *api) {
    if (!api || api->abi_version != TOY_MODULE_ABI_VERSION ||
        api->struct_size < sizeof(toy_module_api)) {
        return false;
    }
    host = api;
    return true;
}

size_t toy_stack_size(toy_state *state) {
    return host->stack_size(state);
}

toy_type toy_stack_type(toy_state *state, size_t depth) {
    return host->stack_type(state, depth);
}

bool toy_get_bool(toy_state *state, size_t depth, bool *value) {
    return host->get_bool(state, depth, value);
}

bool toy_get_int(toy_state *state, size_t depth, int64_t *value) {
    return host->get_int(state, depth, value);
}

bool toy_get_float(toy_state *state, size_t depth, double *value) {
    return host->get_float(state, depth, value);
}

bool toy_get_string(toy_state *state, size_t depth, const char **data,
                    size_t *length) {
    return host->get_string(state, depth, data, length);
}

bool toy_get_resource(toy_state *state, size_t depth,
                      const char *expected_type, void **resource) {
    return host->get_resource(state, depth, expected_type, resource);
}

bool toy_get_resource_type(toy_state *state, size_t depth,
                           const char **type_name) {
    return host->get_resource_type(state, depth, type_name);
}

bool toy_pop(toy_state *state, size_t count) {
    return host->pop(state, count);
}

toy_status toy_push_bool(toy_state *state, bool value) {
    return host->push_bool(state, value);
}

toy_status toy_push_int(toy_state *state, int64_t value) {
    return host->push_int(state, value);
}

toy_status toy_push_float(toy_state *state, double value) {
    return host->push_float(state, value);
}

toy_status toy_push_string(toy_state *state, const char *data, size_t length) {
    return host->push_string(state, data, length);
}

toy_status toy_push_resource(toy_state *state, const char *type_name,
                             void *resource,
                             toy_resource_destructor destructor,
                             void *destructor_userdata) {
    return host->push_resource(state, type_name, resource, destructor,
                               destructor_userdata);
}

const char *toy_get_error(toy_state *state) {
    return host->get_error(state);
}

void toy_clear_error(toy_state *state) {
    host->clear_error(state);
}

toy_status toy_fail(toy_state *state, const char *message) {
    return host->fail(state, message);
}

void toy_interrupt(toy_state *state) {
    host->interrupt(state);
}
