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
tf_ret tf_succ(tf_ctx *ctx);
tf_ret tf_pred(tf_ctx *ctx);
tf_ret tf_abs(tf_ctx *ctx);
tf_ret tf_max(tf_ctx *ctx);
tf_ret tf_min(tf_ctx *ctx);

// Stack operations
tf_ret tf_dup(tf_ctx *ctx);
tf_ret tf_drop(tf_ctx *ctx);
tf_ret tf_swap(tf_ctx *ctx);
tf_ret tf_over(tf_ctx *ctx);
tf_ret tf_rot(tf_ctx *ctx);
tf_ret tf_swapd(tf_ctx *ctx);
tf_ret tf_nip(tf_ctx *ctx);
tf_ret tf_tuck(tf_ctx *ctx);
tf_ret tf_pick(tf_ctx *ctx);
tf_ret tf_roll(tf_ctx *ctx);
tf_ret tf_empty(tf_ctx *ctx);

// I/O and system operations
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
tf_ret tf_load_r(tf_ctx *ctx);
tf_ret tf_exit(tf_ctx *ctx);

// Comparison operations
tf_ret tf_eq(tf_ctx *ctx);
tf_ret tf_ne(tf_ctx *ctx);
tf_ret tf_lt(tf_ctx *ctx);
tf_ret tf_gt(tf_ctx *ctx);
tf_ret tf_le(tf_ctx *ctx);
tf_ret tf_ge(tf_ctx *ctx);

// Control operations
// `_r` marks native words that synchronously run quotations by calling exec()
// and waiting for completion. These still consume C call stack across nested
// native quotation runners, even though user-defined words use tf_frame.
tf_ret tf_exec(tf_ctx *ctx);
tf_ret tf_app2(tf_ctx *ctx);
tf_ret tf_if_r(tf_ctx *ctx);
tf_ret tf_ifelse_r(tf_ctx *ctx);
tf_ret tf_times_r(tf_ctx *ctx);
tf_ret tf_each_r(tf_ctx *ctx);
tf_ret tf_map_r(tf_ctx *ctx);
tf_ret tf_fold_r(tf_ctx *ctx);
tf_ret tf_while_r(tf_ctx *ctx);
tf_ret tf_dip_r(tf_ctx *ctx);
tf_ret tf_keep_r(tf_ctx *ctx);
tf_ret tf_bi_r(tf_ctx *ctx);
tf_ret tf_linrec_r(tf_ctx *ctx);
tf_ret tf_binrec_r(tf_ctx *ctx);

// Definition operations
tf_ret tf_colon(tf_ctx *ctx);
tf_ret tf_def(tf_ctx *ctx);

// List operations
tf_ret tf_geth(tf_ctx *ctx);
tf_ret tf_seth(tf_ctx *ctx);
tf_ret tf_len(tf_ctx *ctx);
tf_ret tf_first(tf_ctx *ctx);
tf_ret tf_rest(tf_ctx *ctx);
tf_ret tf_uncons(tf_ctx *ctx);
tf_ret tf_cons(tf_ctx *ctx);
tf_ret tf_append(tf_ctx *ctx);
tf_ret tf_concat(tf_ctx *ctx);
tf_ret tf_split_r(tf_ctx *ctx);
tf_ret tf_splitmid(tf_ctx *ctx);
tf_ret tf_range(tf_ctx *ctx);
tf_ret tf_empty_q(tf_ctx *ctx);

// System operations
tf_ret tf_rand(tf_ctx *ctx);
tf_ret tf_sleep(tf_ctx *ctx);

#endif  // TF_LIB_H
