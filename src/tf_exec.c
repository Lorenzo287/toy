#include "tf_exec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "tf_alloc.h"
#include "tf_console.h"
#include "tf_lib.h"
#include <signal.h>

/* === Context Manipulation Helpers === */

/* wrappers for managing the context forth stack, based on less abstract object
   manipulation functions defined in tf_obj */
size_t stack_len(tf_ctx *ctx) {
    return ctx->forth_stack->list.len;
}
void stack_push(tf_ctx *ctx, tf_obj *o) {
    push_obj(ctx->forth_stack, o);
}
tf_obj *stack_pop(tf_ctx *ctx) {
    return pop_obj(ctx->forth_stack);
}
tf_obj *stack_pop_type(tf_ctx *ctx, tf_type type) {
    return pop_obj_type(ctx->forth_stack, type);
}

tf_obj *stack_peek(tf_ctx *ctx, size_t depth) {
    size_t len = stack_len(ctx);
    if (depth >= len) return NULL;
    return ctx->forth_stack->list.elem[len - 1 - depth];
}

/* helpers for managing the context call stack */
void frame_push(tf_ctx *ctx, tf_obj *prg) {
    if (ctx->cstack_len >= ctx->cstack_cap) {
        ctx->cstack_cap = ctx->cstack_cap == 0 ? 64 : ctx->cstack_cap * 2;
        ctx->call_stack =
            xrealloc(ctx->call_stack, sizeof(tf_frame) * ctx->cstack_cap);
    }
    ctx->call_stack[ctx->cstack_len].prg = prg;
    ctx->call_stack[ctx->cstack_len].pc = 0;
    ctx->call_stack[ctx->cstack_len].vars.vars = NULL;
    ctx->call_stack[ctx->cstack_len].vars.len = 0;
    ctx->call_stack[ctx->cstack_len].vars.cap = 0;
    retain_obj(prg);
    ctx->cstack_len++;
}

void frame_pop(tf_ctx *ctx) {
    if (ctx->cstack_len == 0) return;
    tf_frame *f = &ctx->call_stack[ctx->cstack_len - 1];

    for (size_t i = 0; i < f->vars.len; i++) {
        release_obj(f->vars.vars[i].name);
        release_obj(f->vars.vars[i].val);
    }
    free(f->vars.vars);

    release_obj(f->prg);
    ctx->cstack_len--;

    if (ctx->cstack_len < ctx->cstack_cap / 4 && ctx->cstack_cap > 64) {
        ctx->cstack_cap /= 2;
        ctx->call_stack =
            xrealloc(ctx->call_stack, sizeof(tf_frame) * ctx->cstack_cap);
    }
}

/* === Function Table Helpers === */

static unsigned long tf_hash(tf_obj *o) {
    unsigned long hash = 5381;
    char *ptr = o->str.ptr;
    size_t len = o->str.len;
    for (size_t i = 0; i < len; i++) { hash = ((hash << 5) + hash) + ptr[i]; }
    return hash;
}

static void tf_table_resize(tf_ctx *ctx) {
    size_t old_cap = ctx->functions.capacity;
    tf_func **old_buckets = ctx->functions.buckets;

    ctx->functions.capacity *= 2;
    ctx->functions.buckets = xcalloc(ctx->functions.capacity, sizeof(tf_func *));

    for (size_t i = 0; i < old_cap; i++) {
        tf_func *f = old_buckets[i];
        if (f) {
            unsigned long h = tf_hash(f->name);
            size_t idx = h % ctx->functions.capacity;
            while (ctx->functions.buckets[idx]) {
                idx = (idx + 1) % ctx->functions.capacity;
            }
            ctx->functions.buckets[idx] = f;
        }
    }
    free(old_buckets);
}

/* === Context Initialization === */

typedef struct {
    const char *name;
    tf_cb cb;
} tf_native_word;

static void register_native_group(tf_ctx *ctx, const tf_native_word *words) {
    for (size_t i = 0; words[i].name; i++) {
        set_native_func(ctx, words[i].name, words[i].cb);
    }
}

static const tf_native_word native_math_words[] = {
    {"+", tf_add},       {"-", tf_sub},     {"*", tf_mul},
    {"/", tf_div},       {"%", tf_mod},     {"mod", tf_mod},
    {"neg", tf_neg},     {"abs", tf_abs},   {"max", tf_max},
    {"min", tf_min},     {"sqrt", tf_sqrt}, {"pow", tf_pow},
    {"exp", tf_exp},     {"log", tf_log},   {"log10", tf_log10},
    {"sin", tf_sin},     {"cos", tf_cos},   {"tan", tf_tan},
    {"floor", tf_floor}, {"ceil", tf_ceil}, {"round", tf_round},
    {"pred", tf_pred},   {"succ", tf_succ}, {"square", tf_square},
    {"cube", tf_cube},   {"pi", tf_pi},     {"e", tf_e},
    {"tau", tf_tau},     {NULL, NULL},
};

static const tf_native_word native_logic_words[] = {
    {"and", tf_and}, {"or", tf_or},   {"xor", tf_xor},
    {"not", tf_not}, {"shl", tf_shl}, {"shr", tf_shr},
    {NULL, NULL},
};

static const tf_native_word native_stack_words[] = {
    {"dup", tf_dup},     {"drop", tf_drop}, {"swap", tf_swap},
    {"over", tf_over},   {"rot", tf_rot},   {"swapd", tf_swapd},
    {"nip", tf_nip},     {"tuck", tf_tuck}, {"pick", tf_pick},
    {"roll", tf_roll},   {"empty", tf_empty},
    {NULL, NULL},
};

static const tf_native_word native_io_words[] = {
    {"printf", tf_printf}, {"print", tf_print}, {"cr", tf_cr},
    {".", tf_dot},         {".s", tf_stack},    {"key", tf_key},
    {"input", tf_input},   {"load", tf_load_r}, {"readf", tf_readf},
    {"writef", tf_writef}, {"delf", tf_delf},   {"readl", tf_readl},
    {"exists?", tf_exists_q}, {"clear", tf_clear}, {"page", tf_clear},
    {NULL, NULL},
};

static const tf_native_word native_comparison_words[] = {
    {"==", tf_eq}, {"!=", tf_ne}, {"<", tf_lt},
    {">", tf_gt}, {"<=", tf_le}, {">=", tf_ge},
    {NULL, NULL},
};

static const tf_native_word native_definition_words[] = {
    {":", tf_colon}, {"def", tf_def}, {NULL, NULL},
};

static const tf_native_word native_control_words[] = {
    {"exec", tf_exec},       {"i", tf_exec},
    {"app2", tf_app2},       {"if", tf_if_r},
    {"ifelse", tf_ifelse_r}, {"while", tf_while_r},
    {"try", tf_try_r},       {"error", tf_error},
    {"infra", tf_infra_r},   {"cond", tf_cond_r},
    {"cleave", tf_cleave_r}, {"construct", tf_construct_r},
    {"replicate", tf_replicate_r}, {"times", tf_times_r},
    {"dip", tf_dip_r},       {"keep", tf_keep_r},
    {"bi", tf_bi_r},         {"linrec", tf_linrec_r},
    {"binrec", tf_binrec_r}, {"genrec", tf_genrec_r},
    {"treerec", tf_treerec_r},
    {NULL, NULL},
};

static const tf_native_word native_collection_combinator_words[] = {
    {"each", tf_each_r},     {"map", tf_map_r},
    {"fold", tf_fold_r},     {"filter", tf_filter_r},
    {"some", tf_some_r},     {"all", tf_all_r},
    {"split", tf_split_r},   {"merge", tf_merge_r},
    {NULL, NULL},
};

static const tf_native_word native_data_words[] = {
    {"geth", tf_geth},       {"seth", tf_seth},
    {"slice", tf_slice},     {"take", tf_take},
    {"dropn", tf_dropn},     {"len", tf_len},
    {"first", tf_first},     {"rest", tf_rest},
    {"uncons", tf_uncons},   {"cons", tf_cons},
    {"append", tf_append},   {"concat", tf_concat},
    {"join", tf_join},       {"trim", tf_trim},
    {"upper", tf_upper},     {"lower", tf_lower},
    {"splitmid", tf_splitmid},
    {"range", tf_range},     {"empty?", tf_empty_q},
    {NULL, NULL},
};

static const tf_native_word native_introspection_words[] = {
    {"typeof", tf_typeof},   {"bool?", tf_bool_q},
    {"int?", tf_int_q},      {"float?", tf_float_q},
    {"str?", tf_str_q},      {"symbol?", tf_symbol_q},
    {"list?", tf_list_q},    {"number?", tf_number_q},
    {"nan?", tf_nan_q},      {"inf?", tf_inf_q},
    {"word?", tf_word_q},    {"var?", tf_var_q},
    {"inf", tf_inf},         {"nan", tf_nan},
    {"body", tf_body},       {"intern", tf_intern},
    {"name", tf_name},       {"words", tf_words},
    {"see", tf_see},
    {NULL, NULL},
};

static const tf_native_word native_system_words[] = {
    {"rand", tf_rand},       {"sleep", tf_sleep},
    {"argc", tf_argc},       {"argv", tf_argv},
    {"getenv", tf_getenv},   {"setenv", tf_setenv},
    {"pwd", tf_pwd},         {"shell", tf_shell},
    {"time", tf_time},       {"clock", tf_clock},
    {"bye", tf_exit},        {"exit", tf_exit},
    {NULL, NULL},
};

tf_ctx *init_ctx(int argc, char **argv) {
    srand(time(NULL));
    tf_ctx *ctx = xmalloc(sizeof(tf_ctx));
    ctx->forth_stack = init_list_obj();
    ctx->functions.capacity = 128;
    ctx->functions.count = 0;
    ctx->functions.buckets = xcalloc(ctx->functions.capacity, sizeof(tf_func *));
    ctx->call_stack = NULL;
    ctx->cstack_len = 0;
    ctx->cstack_cap = 0;
    ctx->argc = argc;
    ctx->argv = argv;
    ctx->error_suppression_depth = 0;

    register_native_group(ctx, native_math_words);
    register_native_group(ctx, native_logic_words);
    register_native_group(ctx, native_stack_words);
    register_native_group(ctx, native_io_words);
    register_native_group(ctx, native_comparison_words);
    register_native_group(ctx, native_definition_words);
    register_native_group(ctx, native_control_words);
    register_native_group(ctx, native_collection_combinator_words);
    register_native_group(ctx, native_data_words);
    register_native_group(ctx, native_introspection_words);
    register_native_group(ctx, native_system_words);

    return ctx;
}

void free_ctx(tf_ctx *ctx) {
    release_obj(ctx->forth_stack);
    for (size_t i = 0; i < ctx->functions.capacity; i++) {
        tf_func *f = ctx->functions.buckets[i];
        if (f) {
            release_obj(f->name);
            if (f->type == TF_FUNC_TYPE_USER) { release_obj(f->user_impl); }
            free(f);
        }
    }
    free(ctx->functions.buckets);
    while (ctx->cstack_len > 0) { frame_pop(ctx); }
    free(ctx->call_stack);
    free(ctx);
}

tf_func *init_func(tf_ctx *ctx, tf_obj *name) {
    if (ctx->functions.count >= ctx->functions.capacity * 0.7) {
        tf_table_resize(ctx);
    }

    unsigned long h = tf_hash(name);
    size_t idx = h % ctx->functions.capacity;
    while (ctx->functions.buckets[idx]) {
        idx = (idx + 1) % ctx->functions.capacity;
    }

    tf_func *f = xmalloc(sizeof(tf_func));
    f->name = name;
    retain_obj(name);
    f->type = TF_FUNC_TYPE_NATIVE;
    f->native_impl = NULL;
    ctx->functions.buckets[idx] = f;
    ctx->functions.count++;
    return f;
}

void set_native_func(tf_ctx *ctx, const char *name, tf_cb cb) {
    tf_obj *o_name = create_string_obj(name, strlen(name));
    tf_func *f = get_func(ctx, o_name);
    if (f) {  // overwrite if name is already taken
        if (f->type == TF_FUNC_TYPE_USER) release_obj(f->user_impl);
    } else {  // allocate if name is not taken
        f = init_func(ctx, o_name);
    }
    f->type = TF_FUNC_TYPE_NATIVE;
    f->native_impl = cb;
    release_obj(o_name);
}

void set_user_func(tf_ctx *ctx, tf_obj *name, tf_obj *uf) {
    tf_func *f = get_func(ctx, name);
    if (f) {
        if (f->type == TF_FUNC_TYPE_USER) { release_obj(f->user_impl); }
    } else {
        f = init_func(ctx, name);
    }
    f->type = TF_FUNC_TYPE_USER;
    f->user_impl = uf;
    retain_obj(uf);
}

tf_func *get_func(tf_ctx *ctx, tf_obj *name) {
    if (ctx->functions.capacity == 0) return NULL;
    unsigned long h = tf_hash(name);
    size_t idx = h % ctx->functions.capacity;
    // linear probing
    while (ctx->functions.buckets[idx]) {
        if (compare_string_obj(ctx->functions.buckets[idx]->name, name) == 0) {
            return ctx->functions.buckets[idx];
        }
        idx = (idx + 1) % ctx->functions.capacity;
    }
    return NULL;
}

/* === Variable Helpers === */

static void tf_var_bind(tf_ctx *ctx, tf_obj *name, tf_obj *val) {
    if (ctx->cstack_len == 0) return;
    tf_frame *f = &ctx->call_stack[ctx->cstack_len - 1];

    // check if variable already exists in current frame and update it
    for (int i = (int)f->vars.len - 1; i >= 0; i--) {
        if (compare_string_obj(f->vars.vars[i].name, name) == 0) {
            release_obj(f->vars.vars[i].val);
            f->vars.vars[i].val = val;
            retain_obj(val);
            return;
        }
    }

    // otherwise append new binding
    if (f->vars.len >= f->vars.cap) {
        f->vars.cap = f->vars.cap == 0 ? 16 : f->vars.cap * 2;
        f->vars.vars = xrealloc(f->vars.vars, sizeof(tf_var) * f->vars.cap);
    }
    f->vars.vars[f->vars.len].name = name;
    f->vars.vars[f->vars.len].val = val;
    retain_obj(name);
    retain_obj(val);
    f->vars.len++;
}

tf_obj *tf_var_fetch(tf_ctx *ctx, tf_obj *name) {
    for (int i = (int)ctx->cstack_len - 1; i >= 0; i--) {
        tf_frame *f = &ctx->call_stack[i];
        for (int j = (int)f->vars.len - 1; j >= 0; j--) {
            if (compare_string_obj(f->vars.vars[j].name, name) == 0) {
                return f->vars.vars[j].val;
            }
        }
    }
    return NULL;
}

static volatile sig_atomic_t interrupted = 0;
void handle_sigint(int sig) {
    (void)sig;
    signal(SIGINT, handle_sigint);
    interrupted = 1;
}

/*
 * The main iterative execution engine.
 * Instead of recursive C calls, it uses an explicit `call_stack` of frames.
 * This ensures deep user-defined word recursion does not overflow the C stack.
 */
tf_ret exec(tf_ctx *ctx, tf_obj *prg) {
    if (prg->type != TF_OBJ_TYPE_LIST) {
        if (ctx->error_suppression_depth == 0) {
            tf_console_runtime_errorf("attempted to execute non-block object\n");
        }
        return TF_ERR;
    }

    // push frame to the call stack
    frame_push(ctx, prg);

    /* If this is a nested call to exec, continue until the pushed frame is
     * popped. Native words with blocking quotation semantics use this path;
     * they should carry the `_r` suffix until replaced by continuation-style
     * frame scheduling. */
    size_t target_depth = ctx->cstack_len - 1;

    while (ctx->cstack_len > target_depth) {
        if (interrupted) {
            while (ctx->cstack_len > target_depth) { frame_pop(ctx); }
            interrupted = 0;  // reset for next run
            return TF_INTERRUPTED;
        }

        tf_frame *f = &ctx->call_stack[ctx->cstack_len - 1];
        if (f->pc >= f->prg->list.len) {
            frame_pop(ctx);
            continue;
        }

        tf_obj *o = f->prg->list.elem[f->pc++];
        switch (o->type) {
        case TF_OBJ_TYPE_SYMBOL:
            if (o->str.quoted) {
                stack_push(ctx, o);
                retain_obj(o);
            } else {
                tf_func *func = get_func(ctx, o);
                if (!func) {
                    if (ctx->error_suppression_depth == 0) {
                        tf_console_runtime_errorf("undefined word '%s'\n",
                                                  o->str.ptr);
                    }
                    while (ctx->cstack_len > target_depth) { frame_pop(ctx); }
                    return TF_ERR;
                }
                tf_ret call_res = call_symbol(ctx, o);
                if (call_res == TF_INTERRUPTED) {
                    while (ctx->cstack_len > target_depth) { frame_pop(ctx); }
                    return TF_INTERRUPTED;
                }
                if (call_res == TF_ERR) {
                    if (ctx->error_suppression_depth == 0) {
                        tf_console_runtime_errorf(
                            "execution of word '%s' failed\n", o->str.ptr);
                    }
                    // unwind remaining frames
                    while (ctx->cstack_len > target_depth) { frame_pop(ctx); }
                    return TF_ERR;
                }
            }
            break;
        case TF_OBJ_TYPE_VARLIST:
            for (int i = (int)o->list.len - 1; i >= 0; i--) {
                tf_obj *val = stack_pop(ctx);
                if (!val) {
                    if (ctx->error_suppression_depth == 0) {
                        tf_console_runtime_errorf(
                            "stack underflow during variable binding\n");
                    }
                    while (ctx->cstack_len > target_depth) { frame_pop(ctx); }
                    return TF_ERR;
                }
                tf_var_bind(ctx, o->list.elem[i], val);
                release_obj(val);
            }
            break;
        case TF_OBJ_TYPE_VARFETCH: {
            tf_obj *val = tf_var_fetch(ctx, o);
            if (!val) {
                if (ctx->error_suppression_depth == 0) {
                    tf_console_runtime_errorf("undefined variable '$%s'\n",
                                              o->str.ptr);
                }
                while (ctx->cstack_len > target_depth) { frame_pop(ctx); }
                return TF_ERR;
            }
            stack_push(ctx, val);
            retain_obj(val);
            break;
        }
        default:
            stack_push(ctx, o);
            retain_obj(o);
            break;
        }
    }
    return TF_OK;
}

/*
 * Hybrid symbol dispatcher:
 * - User-defined words are pushed to the call_stack to continue iteration.
 * - Native words are called directly. If a native calls exec() synchronously,
 *   nested quotation/control flow can still grow the C stack; mark those
 *   native implementations with the `_r` suffix.
 */
tf_ret call_symbol(tf_ctx *ctx, tf_obj *symb) {
    tf_func *f = get_func(ctx, symb);
    if (!f) return TF_ERR;
    if (f->type == TF_FUNC_TYPE_USER) {
        frame_push(ctx, f->user_impl);
        return TF_OK;
    } else {
        return f->native_impl(ctx);
    }
}
