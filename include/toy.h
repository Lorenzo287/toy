#ifndef TOY_H
#define TOY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOY_API_VERSION 0

typedef struct tf_ctx toy_state;

typedef enum {
    TOY_OK,
    TOY_ERROR,
    TOY_INTERRUPTED,
    TOY_EXIT_REQUESTED
} toy_status;

typedef enum {
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
} toy_type;

typedef toy_status (*toy_native_fn)(toy_state *state);
typedef void (*toy_write_fn)(void *userdata, const char *data, size_t length);
typedef void (*toy_resource_destructor)(void *resource, void *userdata);

/* Null callbacks keep the default stdout/stderr destinations. */
typedef struct {
    toy_write_fn output;
    void *output_userdata;
    toy_write_fn diagnostic;
    void *diagnostic_userdata;
} toy_state_config;

/* Native module and word names are copied during successful registration. */
typedef struct {
    const char *name;
    toy_native_fn callback;
} toy_native_word;

typedef struct {
    const char *name;
    const toy_native_word *words;
    size_t word_count;
} toy_native_module;

/* A null config selects default output. States are not safe for concurrent
 * use. */
toy_state *toy_state_new(const toy_state_config *config);
void toy_state_free(toy_state *state);

/* Host entry points. Calls are allowed only while the state is idle. */
toy_status toy_eval(toy_state *state, const char *source_name,
                    const char *source);
toy_status toy_call(toy_state *state, const char *word);
toy_status toy_register_word(toy_state *state, const char *name,
                             toy_native_fn function);
/* Registers every word atomically and marks the module loaded. */
toy_status toy_register_module(toy_state *state,
                               const toy_native_module *module);

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

/* Diagnostics and cooperative interruption. */
const char *toy_get_error(toy_state *state);
void toy_clear_error(toy_state *state);
toy_status toy_set_error(toy_state *state, const char *message);
void toy_interrupt(toy_state *state);

#ifdef __cplusplus
}
#endif

#endif  // TOY_H
