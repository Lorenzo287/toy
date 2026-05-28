#ifndef TF_REPL_H
#define TF_REPL_H

#include <stdbool.h>
#include "tf_exec.h"

/* Parse and run a file in the provided interpreter context. */
tf_ret tf_run_file(tf_ctx *ctx, const char *filename, bool debug);

/* Parse and run an in-memory source string. */
tf_ret tf_run_string(tf_ctx *ctx, const char *source, bool debug);

/* Start the interactive REPL loop. */
tf_ret tf_run_repl(tf_ctx *ctx, bool debug);

#endif  // TF_REPL_H
