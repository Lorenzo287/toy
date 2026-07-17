#ifndef TOY_H
#define TOY_H

#include "toy_module.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*toy_write_fn)(void *userdata, const char *data, size_t length);

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

#ifdef __cplusplus
}
#endif

#endif  // TOY_H
