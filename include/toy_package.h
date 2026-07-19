#ifndef TOY_PACKAGE_H
#define TOY_PACKAGE_H

/* Define TOY_PACKAGE_IMPLEMENTATION before including this header in exactly one
 * source file of a loadable package. No Toy library is required. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tf_ctx toy_state;
typedef struct toy_value toy_value;

typedef uint32_t toy_status;
enum {
    TOY_OK,
    TOY_ERROR,
    TOY_INTERRUPTED,
    TOY_EXIT_REQUESTED
};

typedef uint32_t toy_type;
enum {
    TOY_TYPE_MISSING,
    TOY_TYPE_BOOL,
    TOY_TYPE_INT,
    TOY_TYPE_FLOAT,
    TOY_TYPE_STRING,
    TOY_TYPE_SYMBOL,
    TOY_TYPE_CALL,
    TOY_TYPE_VECTOR,
    TOY_TYPE_LIST,
    TOY_TYPE_MAP,
    TOY_TYPE_SET,
    TOY_TYPE_DEQUE,
    TOY_TYPE_PQUEUE,
    TOY_TYPE_RESOURCE,
    TOY_TYPE_INTERNAL
};

typedef toy_status (*toy_native_fn)(toy_state *state);
typedef void (*toy_resource_destructor)(void *resource, void *userdata);

/* Native word names are copied when a host registers the package. */
typedef struct {
    const char *name;
    toy_native_fn callback;
} toy_native_word;

/* Data-stack access. Depth zero addresses the top value. */
size_t toy_stack_size(toy_state *state);
toy_type toy_stack_type(toy_state *state, size_t depth);
bool toy_get_bool(toy_state *state, size_t depth, bool *value);
bool toy_get_int(toy_state *state, size_t depth, int64_t *value);
bool toy_get_float(toy_state *state, size_t depth, double *value);
bool toy_get_string(toy_state *state, size_t depth, const char **data,
                    size_t *length);
bool toy_get_resource(toy_state *state, size_t depth,
                      const char *expected_type, void **resource);
bool toy_get_resource_type(toy_state *state, size_t depth,
                           const char **type_name);
bool toy_pop(toy_state *state, size_t count);
toy_status toy_push_bool(toy_state *state, bool value);
toy_status toy_push_int(toy_state *state, int64_t value);
toy_status toy_push_float(toy_state *state, double value);
toy_status toy_push_string(toy_state *state, const char *data, size_t length);
/* Ownership transfers to Toy only on success. The type name is copied. */
toy_status toy_push_resource(toy_state *state, const char *type_name,
                             void *resource,
                             toy_resource_destructor destructor,
                             void *destructor_userdata);

/* Persistent, state-bound references to Toy values. A retained value must be
 * released before its state is destroyed. */
toy_value *toy_value_retain(toy_state *state, size_t depth);
void toy_value_release(toy_value *value);
toy_type toy_value_type(const toy_value *value);
bool toy_value_get_bool(const toy_value *value, bool *result);
bool toy_value_get_int(const toy_value *value, int64_t *result);
bool toy_value_get_float(const toy_value *value, double *result);
bool toy_value_get_string(const toy_value *value, const char **data,
                          size_t *length);
bool toy_value_get_resource(const toy_value *value,
                            const char *expected_type, void **resource);
bool toy_value_get_resource_type(const toy_value *value,
                                 const char **type_name);
toy_status toy_push_value(toy_state *state, const toy_value *value);

/* Vector, list, and string inspection. Returned items are new retained values.
 * A string item is a one-byte string, matching the Toy sequence contract. */
bool toy_sequence_size(const toy_value *sequence, size_t *size);
toy_value *toy_sequence_get(const toy_value *sequence, size_t index);

/* Map entries follow insertion order. Both outputs are new retained values. */
bool toy_map_size(const toy_value *map, size_t *size);
bool toy_map_entry(const toy_value *map, size_t index, toy_value **key,
                   toy_value **value);

/* Stack construction. These consume their inputs only after validation and
 * push the resulting collection. Vector items and map pairs preserve their
 * deepest-to-top order; map input is key value key value ... */
toy_status toy_make_vector(toy_state *state, size_t item_count);
toy_status toy_make_map(toy_state *state, size_t pair_count);

/* Invoke a retained quotation, symbol, or call using the current data stack.
 * Like host evaluation calls, this requires an idle state. */
toy_status toy_call_value(toy_state *state, const toy_value *callable);

/* Diagnostics and cooperative interruption. */
const char *toy_get_error(toy_state *state);
void toy_clear_error(toy_state *state);
/* Store and emit a native diagnostic, then return TOY_ERROR. */
toy_status toy_fail(toy_state *state, const char *message);
void toy_interrupt(toy_state *state);

#define TOY_PACKAGE_ABI_VERSION 1u
#define TOY_PACKAGE_ENTRY_SYMBOL "toy_package_init"

#if defined(_WIN32)
#define TOY_PACKAGE_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define TOY_PACKAGE_EXPORT __attribute__((visibility("default")))
#else
#define TOY_PACKAGE_EXPORT
#endif

/*
 * Host operations available to shared native packages. The table has static
 * process lifetime. The structure size must match exactly.
 */
typedef struct {
    size_t struct_size;

    size_t (*stack_size)(toy_state *state);
    toy_type (*stack_type)(toy_state *state, size_t depth);
    bool (*get_bool)(toy_state *state, size_t depth, bool *value);
    bool (*get_int)(toy_state *state, size_t depth, int64_t *value);
    bool (*get_float)(toy_state *state, size_t depth, double *value);
    bool (*get_string)(toy_state *state, size_t depth, const char **data,
                       size_t *length);
    bool (*get_resource)(toy_state *state, size_t depth,
                         const char *expected_type, void **resource);
    bool (*get_resource_type)(toy_state *state, size_t depth,
                              const char **type_name);
    bool (*pop)(toy_state *state, size_t count);

    toy_status (*push_bool)(toy_state *state, bool value);
    toy_status (*push_int)(toy_state *state, int64_t value);
    toy_status (*push_float)(toy_state *state, double value);
    toy_status (*push_string)(toy_state *state, const char *data,
                              size_t length);
    toy_status (*push_resource)(toy_state *state, const char *type_name,
                                void *resource,
                                toy_resource_destructor destructor,
                                void *destructor_userdata);

    const char *(*get_error)(toy_state *state);
    void (*clear_error)(toy_state *state);
    toy_status (*fail)(toy_state *state, const char *message);
    void (*interrupt)(toy_state *state);

    toy_value *(*value_retain)(toy_state *state, size_t depth);
    void (*value_release)(toy_value *value);
    toy_type (*value_type)(const toy_value *value);
    bool (*value_get_bool)(const toy_value *value, bool *result);
    bool (*value_get_int)(const toy_value *value, int64_t *result);
    bool (*value_get_float)(const toy_value *value, double *result);
    bool (*value_get_string)(const toy_value *value, const char **data,
                             size_t *length);
    bool (*value_get_resource)(const toy_value *value,
                               const char *expected_type, void **resource);
    bool (*value_get_resource_type)(const toy_value *value,
                                    const char **type_name);
    toy_status (*push_value)(toy_state *state, const toy_value *value);
    bool (*sequence_size)(const toy_value *sequence, size_t *size);
    toy_value *(*sequence_get)(const toy_value *sequence, size_t index);
    bool (*map_size)(const toy_value *map, size_t *size);
    bool (*map_entry)(const toy_value *map, size_t index, toy_value **key,
                      toy_value **value);
    toy_status (*make_vector)(toy_state *state, size_t item_count);
    toy_status (*make_map)(toy_state *state, size_t pair_count);
    toy_status (*call_value)(toy_state *state, const toy_value *callable);
} toy_package_api;

/* Returned by the exported toy_package_init entry point. */
typedef struct {
    size_t struct_size;
    const char *name;
    const toy_native_word *words;
    size_t word_count;
} toy_package_export;

typedef const toy_package_export *(*toy_package_entry)(
    uint32_t abi_version, const toy_package_api *api);

TOY_PACKAGE_EXPORT const toy_package_export *toy_package_init(
    uint32_t abi_version, const toy_package_api *api);

/* Check the ABI version and bind the host table. */
bool toy_package_bind(uint32_t abi_version, const toy_package_api *api);

#ifdef TOY_PACKAGE_IMPLEMENTATION

static const toy_package_api *toy_package_host = NULL;

bool toy_package_bind(uint32_t abi_version, const toy_package_api *api) {
    if (abi_version != TOY_PACKAGE_ABI_VERSION || !api ||
        api->struct_size != sizeof(toy_package_api)) {
        return false;
    }
    toy_package_host = api;
    return true;
}

size_t toy_stack_size(toy_state *state) {
    return toy_package_host->stack_size(state);
}

toy_type toy_stack_type(toy_state *state, size_t depth) {
    return toy_package_host->stack_type(state, depth);
}

bool toy_get_bool(toy_state *state, size_t depth, bool *value) {
    return toy_package_host->get_bool(state, depth, value);
}

bool toy_get_int(toy_state *state, size_t depth, int64_t *value) {
    return toy_package_host->get_int(state, depth, value);
}

bool toy_get_float(toy_state *state, size_t depth, double *value) {
    return toy_package_host->get_float(state, depth, value);
}

bool toy_get_string(toy_state *state, size_t depth, const char **data,
                    size_t *length) {
    return toy_package_host->get_string(state, depth, data, length);
}

bool toy_get_resource(toy_state *state, size_t depth,
                      const char *expected_type, void **resource) {
    return toy_package_host->get_resource(state, depth, expected_type,
                                         resource);
}

bool toy_get_resource_type(toy_state *state, size_t depth,
                           const char **type_name) {
    return toy_package_host->get_resource_type(state, depth, type_name);
}

bool toy_pop(toy_state *state, size_t count) {
    return toy_package_host->pop(state, count);
}

toy_status toy_push_bool(toy_state *state, bool value) {
    return toy_package_host->push_bool(state, value);
}

toy_status toy_push_int(toy_state *state, int64_t value) {
    return toy_package_host->push_int(state, value);
}

toy_status toy_push_float(toy_state *state, double value) {
    return toy_package_host->push_float(state, value);
}

toy_status toy_push_string(toy_state *state, const char *data, size_t length) {
    return toy_package_host->push_string(state, data, length);
}

toy_status toy_push_resource(toy_state *state, const char *type_name,
                             void *resource,
                             toy_resource_destructor destructor,
                             void *destructor_userdata) {
    return toy_package_host->push_resource(state, type_name, resource,
                                          destructor, destructor_userdata);
}

const char *toy_get_error(toy_state *state) {
    return toy_package_host->get_error(state);
}

void toy_clear_error(toy_state *state) {
    toy_package_host->clear_error(state);
}

toy_status toy_fail(toy_state *state, const char *message) {
    return toy_package_host->fail(state, message);
}

void toy_interrupt(toy_state *state) {
    toy_package_host->interrupt(state);
}

toy_value *toy_value_retain(toy_state *state, size_t depth) {
    return toy_package_host->value_retain(state, depth);
}

void toy_value_release(toy_value *value) {
    toy_package_host->value_release(value);
}

toy_type toy_value_type(const toy_value *value) {
    return toy_package_host->value_type(value);
}

bool toy_value_get_bool(const toy_value *value, bool *result) {
    return toy_package_host->value_get_bool(value, result);
}

bool toy_value_get_int(const toy_value *value, int64_t *result) {
    return toy_package_host->value_get_int(value, result);
}

bool toy_value_get_float(const toy_value *value, double *result) {
    return toy_package_host->value_get_float(value, result);
}

bool toy_value_get_string(const toy_value *value, const char **data,
                          size_t *length) {
    return toy_package_host->value_get_string(value, data, length);
}

bool toy_value_get_resource(const toy_value *value,
                            const char *expected_type, void **resource) {
    return toy_package_host->value_get_resource(value, expected_type, resource);
}

bool toy_value_get_resource_type(const toy_value *value,
                                 const char **type_name) {
    return toy_package_host->value_get_resource_type(value, type_name);
}

toy_status toy_push_value(toy_state *state, const toy_value *value) {
    return toy_package_host->push_value(state, value);
}

bool toy_sequence_size(const toy_value *sequence, size_t *size) {
    return toy_package_host->sequence_size(sequence, size);
}

toy_value *toy_sequence_get(const toy_value *sequence, size_t index) {
    return toy_package_host->sequence_get(sequence, index);
}

bool toy_map_size(const toy_value *map, size_t *size) {
    return toy_package_host->map_size(map, size);
}

bool toy_map_entry(const toy_value *map, size_t index, toy_value **key,
                   toy_value **value) {
    return toy_package_host->map_entry(map, index, key, value);
}

toy_status toy_make_vector(toy_state *state, size_t item_count) {
    return toy_package_host->make_vector(state, item_count);
}

toy_status toy_make_map(toy_state *state, size_t pair_count) {
    return toy_package_host->make_map(state, pair_count);
}

toy_status toy_call_value(toy_state *state, const toy_value *callable) {
    return toy_package_host->call_value(state, callable);
}

#endif  // TOY_PACKAGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif  // TOY_PACKAGE_H
