#ifndef TF_RUNTIME_H
#define TF_RUNTIME_H

#include "tf_exec.h"

/* Parse source into an owned top-level program vector. */
tf_obj *tf_parse_source(const char *filename, const char *source);
tf_obj *tf_parse_source_ctx(tf_ctx *ctx, const char *filename,
                            const char *source);

/* Parse and execute source without frontend-specific output. */
tf_ret tf_eval_source(tf_ctx *ctx, const char *filename, const char *source);

#endif  // TF_RUNTIME_H
