#ifndef TF_DEBUG_PROTOCOL_H
#define TF_DEBUG_PROTOCOL_H

#include <stdio.h>
#include "tf_exec.h"

typedef struct tf_debug_protocol tf_debug_protocol;

/* Install a blocking machine debugger frontend on a context. */
tf_debug_protocol *tf_debug_protocol_new(FILE *output,
                                         const char *program_path);
void tf_debug_protocol_install(tf_ctx *ctx, tf_debug_protocol *protocol);
void tf_debug_protocol_finish(tf_debug_protocol *protocol, tf_ret result);
void tf_debug_protocol_free(tf_debug_protocol *protocol);

#endif  // TF_DEBUG_PROTOCOL_H
