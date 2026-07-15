#ifndef TOY_MODULE_H
#define TOY_MODULE_H

#include "toy.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOY_MODULE_ABI_VERSION 1u
#define TOY_MODULE_ENTRY_SYMBOL "toy_module_v1"

#if defined(_WIN32)
#define TOY_MODULE_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define TOY_MODULE_EXPORT __attribute__((visibility("default")))
#else
#define TOY_MODULE_EXPORT
#endif

/*
 * Host operations available to shared native modules. The table has static
 * process lifetime. New ABI-compatible fields may only be appended.
 */
typedef struct {
    uint32_t abi_version;
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
} toy_module_api;

/* Returned by the exported toy_module_v1 entry point. */
typedef struct {
    uint32_t abi_version;
    size_t struct_size;
    const char *name;
    const toy_native_word *words;
    size_t word_count;
} toy_module_export;

typedef const toy_module_export *(*toy_module_entry)(
    const toy_module_api *api);

TOY_MODULE_EXPORT const toy_module_export *toy_module_v1(
    const toy_module_api *api);

/* Bind the host table before returning a module descriptor. Shared modules
 * link Toy's module-support object, not the Toy runtime itself. */
bool toy_module_bind(const toy_module_api *api);

#ifdef __cplusplus
}
#endif

#endif  // TOY_MODULE_H
