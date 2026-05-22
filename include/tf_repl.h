#ifndef TF_REPL_H
#define TF_REPL_H

#include <stdbool.h>
#include "tf_exec.h"

tf_ret run_file(tf_ctx *ctx, const char *filename, bool debug);
tf_ret run_string(tf_ctx *ctx, const char *source, bool debug);
tf_ret run_repl(tf_ctx *ctx, bool debug);

#endif  // TF_REPL_H
