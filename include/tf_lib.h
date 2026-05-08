#ifndef TF_LIB_H
#define TF_LIB_H

#include "tf_exec.h"

// Math operations
tf_ret tf_add(tf_ctx *ctx);
tf_ret tf_sub(tf_ctx *ctx);
tf_ret tf_mul(tf_ctx *ctx);
tf_ret tf_div(tf_ctx *ctx);
tf_ret tf_mod(tf_ctx *ctx);
tf_ret tf_neg(tf_ctx *ctx);
tf_ret tf_abs(tf_ctx *ctx);
tf_ret tf_max(tf_ctx *ctx);
tf_ret tf_min(tf_ctx *ctx);

// Stack operations
tf_ret tf_dup(tf_ctx *ctx);
tf_ret tf_drop(tf_ctx *ctx);
tf_ret tf_swap(tf_ctx *ctx);
tf_ret tf_over(tf_ctx *ctx);
tf_ret tf_rot(tf_ctx *ctx);
tf_ret tf_nip(tf_ctx *ctx);
tf_ret tf_tuck(tf_ctx *ctx);
tf_ret tf_pick(tf_ctx *ctx);
tf_ret tf_roll(tf_ctx *ctx);
tf_ret tf_empty(tf_ctx *ctx);

// I/O operations
tf_ret tf_printf(tf_ctx *ctx);
tf_ret tf_print(tf_ctx *ctx);
tf_ret tf_dot(tf_ctx *ctx);
tf_ret tf_cr(tf_ctx *ctx);
tf_ret tf_stack(tf_ctx *ctx);
tf_ret tf_key(tf_ctx *ctx);
tf_ret tf_input(tf_ctx *ctx);
tf_ret tf_time(tf_ctx *ctx);
tf_ret tf_clear(tf_ctx *ctx);
tf_ret tf_words(tf_ctx *ctx);
tf_ret tf_see(tf_ctx *ctx);
tf_ret tf_exit(tf_ctx *ctx);

// Comparison operations
tf_ret tf_eq(tf_ctx *ctx);
tf_ret tf_ne(tf_ctx *ctx);
tf_ret tf_lt(tf_ctx *ctx);
tf_ret tf_gt(tf_ctx *ctx);
tf_ret tf_le(tf_ctx *ctx);
tf_ret tf_ge(tf_ctx *ctx);

// Control operations
tf_ret tf_exec(tf_ctx *ctx);
tf_ret tf_if_r(tf_ctx *ctx);
tf_ret tf_ifelse_r(tf_ctx *ctx);
tf_ret tf_times_r(tf_ctx *ctx);
tf_ret tf_each_r(tf_ctx *ctx);
tf_ret tf_while_r(tf_ctx *ctx);

// Definition operations
tf_ret tf_colon(tf_ctx *ctx);
tf_ret tf_def(tf_ctx *ctx);

// Extended library
tf_ret tf_geth(tf_ctx *ctx);
tf_ret tf_seth(tf_ctx *ctx);
tf_ret tf_len(tf_ctx *ctx);
tf_ret tf_rand(tf_ctx *ctx);
tf_ret tf_sleep(tf_ctx *ctx);

#endif  // TF_LIB_H
