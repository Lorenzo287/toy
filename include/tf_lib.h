#ifndef TF_LIB_H
#define TF_LIB_H

#include "tf_exec.h"

// NOTE: `_r` marks native words that synchronously run quotations by calling exec()
// and waiting for completion. These still consume C call stack across nested
// native quotation runners, even though user-defined words use tf_frame.

// Core arithmetic and numeric operations
tf_ret tf_add(tf_ctx *ctx);
tf_ret tf_sub(tf_ctx *ctx);
tf_ret tf_mul(tf_ctx *ctx);
tf_ret tf_div(tf_ctx *ctx);
tf_ret tf_mod(tf_ctx *ctx);
tf_ret tf_neg(tf_ctx *ctx);
tf_ret tf_abs(tf_ctx *ctx);
tf_ret tf_max(tf_ctx *ctx);
tf_ret tf_min(tf_ctx *ctx);
tf_ret tf_sqrt(tf_ctx *ctx);
tf_ret tf_pow(tf_ctx *ctx);
tf_ret tf_exp(tf_ctx *ctx);
tf_ret tf_log(tf_ctx *ctx);
tf_ret tf_log10(tf_ctx *ctx);
tf_ret tf_sin(tf_ctx *ctx);
tf_ret tf_cos(tf_ctx *ctx);
tf_ret tf_tan(tf_ctx *ctx);
tf_ret tf_floor(tf_ctx *ctx);
tf_ret tf_ceil(tf_ctx *ctx);
tf_ret tf_round(tf_ctx *ctx);
tf_ret tf_pred(tf_ctx *ctx);
tf_ret tf_succ(tf_ctx *ctx);
tf_ret tf_square(tf_ctx *ctx);
tf_ret tf_cube(tf_ctx *ctx);
tf_ret tf_pi(tf_ctx *ctx);
tf_ret tf_e(tf_ctx *ctx);
tf_ret tf_tau(tf_ctx *ctx);

// Core logic/bitwise operations
tf_ret tf_and(tf_ctx *ctx);
tf_ret tf_or(tf_ctx *ctx);
tf_ret tf_xor(tf_ctx *ctx);
tf_ret tf_not(tf_ctx *ctx);
tf_ret tf_shl(tf_ctx *ctx);
tf_ret tf_shr(tf_ctx *ctx);

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

// Comparison operations
tf_ret tf_eq(tf_ctx *ctx);
tf_ret tf_ne(tf_ctx *ctx);
tf_ret tf_lt(tf_ctx *ctx);
tf_ret tf_gt(tf_ctx *ctx);
tf_ret tf_le(tf_ctx *ctx);
tf_ret tf_ge(tf_ctx *ctx);

// Console and file I/O operations
tf_ret tf_printf(tf_ctx *ctx);
tf_ret tf_print(tf_ctx *ctx);
tf_ret tf_dot(tf_ctx *ctx);
tf_ret tf_cr(tf_ctx *ctx);
tf_ret tf_stack(tf_ctx *ctx);
tf_ret tf_key(tf_ctx *ctx);
tf_ret tf_input(tf_ctx *ctx);
tf_ret tf_load_r(tf_ctx *ctx);
tf_ret tf_readf(tf_ctx *ctx);
tf_ret tf_writef(tf_ctx *ctx);
tf_ret tf_delf(tf_ctx *ctx);
tf_ret tf_readl(tf_ctx *ctx);
tf_ret tf_exists_q(tf_ctx *ctx);
tf_ret tf_clear(tf_ctx *ctx);

// Dictionary, definition, and type introspection
tf_ret tf_typeof(tf_ctx *ctx);
tf_ret tf_bool_q(tf_ctx *ctx);
tf_ret tf_int_q(tf_ctx *ctx);
tf_ret tf_float_q(tf_ctx *ctx);
tf_ret tf_str_q(tf_ctx *ctx);
tf_ret tf_symbol_q(tf_ctx *ctx);
tf_ret tf_list_q(tf_ctx *ctx);
tf_ret tf_number_q(tf_ctx *ctx);
tf_ret tf_nan_q(tf_ctx *ctx);
tf_ret tf_inf_q(tf_ctx *ctx);
tf_ret tf_word_q(tf_ctx *ctx);
tf_ret tf_var_q(tf_ctx *ctx);
tf_ret tf_inf(tf_ctx *ctx);
tf_ret tf_nan(tf_ctx *ctx);
tf_ret tf_body(tf_ctx *ctx);
tf_ret tf_intern(tf_ctx *ctx);
tf_ret tf_name(tf_ctx *ctx);
tf_ret tf_words(tf_ctx *ctx);
tf_ret tf_see(tf_ctx *ctx);
tf_ret tf_colon(tf_ctx *ctx);
tf_ret tf_def(tf_ctx *ctx);

// Quotation execution, control flow, and recursion
// `_r` words still call exec() synchronously and should eventually be converted
// to continuation-style frame scheduling.
tf_ret tf_exec(tf_ctx *ctx);
tf_ret tf_app2(tf_ctx *ctx);
tf_ret tf_if_r(tf_ctx *ctx);
tf_ret tf_ifelse_r(tf_ctx *ctx);
tf_ret tf_while_r(tf_ctx *ctx);
tf_ret tf_try_r(tf_ctx *ctx);
tf_ret tf_error(tf_ctx *ctx);
tf_ret tf_infra_r(tf_ctx *ctx);
tf_ret tf_cond_r(tf_ctx *ctx);
tf_ret tf_cleave_r(tf_ctx *ctx);
tf_ret tf_construct_r(tf_ctx *ctx);
tf_ret tf_genrec_r(tf_ctx *ctx);
tf_ret tf_treerec_r(tf_ctx *ctx);
tf_ret tf_replicate_r(tf_ctx *ctx);
tf_ret tf_times_r(tf_ctx *ctx);
tf_ret tf_each_r(tf_ctx *ctx);
tf_ret tf_map_r(tf_ctx *ctx);
tf_ret tf_fold_r(tf_ctx *ctx);
tf_ret tf_filter_r(tf_ctx *ctx);
tf_ret tf_some_r(tf_ctx *ctx);
tf_ret tf_all_r(tf_ctx *ctx);
tf_ret tf_split_r(tf_ctx *ctx);
tf_ret tf_merge_r(tf_ctx *ctx);
tf_ret tf_dip_r(tf_ctx *ctx);
tf_ret tf_keep_r(tf_ctx *ctx);
tf_ret tf_bi_r(tf_ctx *ctx);
tf_ret tf_linrec_r(tf_ctx *ctx);
tf_ret tf_binrec_r(tf_ctx *ctx);

// Data, collection, and string operations
tf_ret tf_geth(tf_ctx *ctx);
tf_ret tf_seth(tf_ctx *ctx);
tf_ret tf_slice(tf_ctx *ctx);
tf_ret tf_take(tf_ctx *ctx);
tf_ret tf_dropn(tf_ctx *ctx);
tf_ret tf_len(tf_ctx *ctx);
tf_ret tf_first(tf_ctx *ctx);
tf_ret tf_rest(tf_ctx *ctx);
tf_ret tf_uncons(tf_ctx *ctx);
tf_ret tf_cons(tf_ctx *ctx);
tf_ret tf_append(tf_ctx *ctx);
tf_ret tf_concat(tf_ctx *ctx);
tf_ret tf_join(tf_ctx *ctx);
tf_ret tf_trim(tf_ctx *ctx);
tf_ret tf_upper(tf_ctx *ctx);
tf_ret tf_lower(tf_ctx *ctx);
tf_ret tf_split_string(tf_ctx *ctx);
tf_ret tf_splitmid(tf_ctx *ctx);
tf_ret tf_range(tf_ctx *ctx);
tf_ret tf_empty_q(tf_ctx *ctx);

// System and process operations
tf_ret tf_rand(tf_ctx *ctx);
tf_ret tf_sleep(tf_ctx *ctx);
tf_ret tf_argc(tf_ctx *ctx);
tf_ret tf_argv(tf_ctx *ctx);
tf_ret tf_getenv(tf_ctx *ctx);
tf_ret tf_setenv(tf_ctx *ctx);
tf_ret tf_pwd(tf_ctx *ctx);
tf_ret tf_shell(tf_ctx *ctx);
tf_ret tf_time(tf_ctx *ctx);
tf_ret tf_clock(tf_ctx *ctx);
tf_ret tf_exit(tf_ctx *ctx);

#endif  // TF_LIB_H
