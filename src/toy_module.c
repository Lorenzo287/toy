#include "toy_module.h"

static const toy_module_api *host = NULL;

bool toy_module_bind(uint32_t abi_version, const toy_module_api *api) {
    if (abi_version != TOY_MODULE_ABI_VERSION || !api ||
        api->struct_size != sizeof(toy_module_api)) {
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

toy_value *toy_value_retain(toy_state *state, size_t depth) {
    return host->value_retain(state, depth);
}

void toy_value_release(toy_value *value) {
    host->value_release(value);
}

toy_type toy_value_type(const toy_value *value) {
    return host->value_type(value);
}

bool toy_value_get_bool(const toy_value *value, bool *result) {
    return host->value_get_bool(value, result);
}

bool toy_value_get_int(const toy_value *value, int64_t *result) {
    return host->value_get_int(value, result);
}

bool toy_value_get_float(const toy_value *value, double *result) {
    return host->value_get_float(value, result);
}

bool toy_value_get_string(const toy_value *value, const char **data,
                          size_t *length) {
    return host->value_get_string(value, data, length);
}

bool toy_value_get_resource(const toy_value *value,
                            const char *expected_type, void **resource) {
    return host->value_get_resource(value, expected_type, resource);
}

bool toy_value_get_resource_type(const toy_value *value,
                                 const char **type_name) {
    return host->value_get_resource_type(value, type_name);
}

toy_status toy_push_value(toy_state *state, const toy_value *value) {
    return host->push_value(state, value);
}

bool toy_sequence_size(const toy_value *sequence, size_t *size) {
    return host->sequence_size(sequence, size);
}

toy_value *toy_sequence_get(const toy_value *sequence, size_t index) {
    return host->sequence_get(sequence, index);
}

bool toy_map_size(const toy_value *map, size_t *size) {
    return host->map_size(map, size);
}

bool toy_map_entry(const toy_value *map, size_t index, toy_value **key,
                   toy_value **value) {
    return host->map_entry(map, index, key, value);
}

toy_status toy_make_vector(toy_state *state, size_t item_count) {
    return host->make_vector(state, item_count);
}

toy_status toy_make_map(toy_state *state, size_t pair_count) {
    return host->make_map(state, pair_count);
}

toy_status toy_call_value(toy_state *state, const toy_value *callable) {
    return host->call_value(state, callable);
}
