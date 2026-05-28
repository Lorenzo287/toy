#include "tf_exec.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "tf_alloc.h"
#include "tf_console.h"
#include "tf_lib.h"
#include <signal.h>

/* === Data Stack === */

size_t tf_stack_len(tf_ctx *ctx) {
    return ctx->data_stack->list.len;
}

void tf_stack_push(tf_ctx *ctx, tf_obj *o) {
    tf_list_push(ctx->data_stack, o);
}

tf_obj *tf_stack_pop(tf_ctx *ctx) {
    return tf_list_pop(ctx->data_stack);
}

tf_obj *tf_stack_pop_type(tf_ctx *ctx, tf_type type) {
    return tf_list_pop_type(ctx->data_stack, type);
}

tf_obj *tf_stack_pop_callable(tf_ctx *ctx) {
    if (tf_stack_len(ctx) == 0) return NULL;
    tf_obj *o = tf_stack_peek(ctx, 0);
    if (!tf_obj_is_callable(o)) return NULL;
    return tf_stack_pop(ctx);
}

tf_obj *tf_stack_peek(tf_ctx *ctx, size_t depth) {
    size_t len = tf_stack_len(ctx);
    if (depth >= len) return NULL;
    return ctx->data_stack->list.elem[len - 1 - depth];
}

/* === Execution Frames === */

void tf_frame_push_program(tf_ctx *ctx, tf_obj *program) {
    if (ctx->call_stack_len >= ctx->call_stack_cap) {
        ctx->call_stack_cap =
            ctx->call_stack_cap == 0 ? 64 : ctx->call_stack_cap * 2;
        ctx->call_stack =
            tf_xrealloc(ctx->call_stack, sizeof(tf_frame) * ctx->call_stack_cap);
    }
    ctx->call_stack[ctx->call_stack_len].kind = TF_FRAME_PROGRAM;
    ctx->call_stack[ctx->call_stack_len].program = program;
    ctx->call_stack[ctx->call_stack_len].pc = 0;
    ctx->call_stack[ctx->call_stack_len].vars.vars = NULL;
    ctx->call_stack[ctx->call_stack_len].vars.len = 0;
    ctx->call_stack[ctx->call_stack_len].vars.cap = 0;
    ctx->call_stack[ctx->call_stack_len].step = NULL;
    ctx->call_stack[ctx->call_stack_len].cleanup = NULL;
    ctx->call_stack[ctx->call_stack_len].on_error = NULL;
    ctx->call_stack[ctx->call_stack_len].state = NULL;
    ctx->call_stack[ctx->call_stack_len].call_site = ctx->current_span;
    tf_obj_retain(program);
    ctx->call_stack_len++;
}

void tf_frame_push_native_handler(tf_ctx *ctx, tf_frame_step_fn step,
                                  tf_frame_cleanup_fn cleanup,
                                  tf_frame_error_fn on_error, void *state) {
    if (ctx->call_stack_len >= ctx->call_stack_cap) {
        ctx->call_stack_cap =
            ctx->call_stack_cap == 0 ? 64 : ctx->call_stack_cap * 2;
        ctx->call_stack =
            tf_xrealloc(ctx->call_stack, sizeof(tf_frame) * ctx->call_stack_cap);
    }
    ctx->call_stack[ctx->call_stack_len].kind = TF_FRAME_NATIVE;
    ctx->call_stack[ctx->call_stack_len].program = NULL;
    ctx->call_stack[ctx->call_stack_len].pc = 0;
    ctx->call_stack[ctx->call_stack_len].vars.vars = NULL;
    ctx->call_stack[ctx->call_stack_len].vars.len = 0;
    ctx->call_stack[ctx->call_stack_len].vars.cap = 0;
    ctx->call_stack[ctx->call_stack_len].step = step;
    ctx->call_stack[ctx->call_stack_len].cleanup = cleanup;
    ctx->call_stack[ctx->call_stack_len].on_error = on_error;
    ctx->call_stack[ctx->call_stack_len].state = state;
    ctx->call_stack[ctx->call_stack_len].call_site = ctx->current_span;
    ctx->call_stack_len++;
}

void tf_frame_push_native(tf_ctx *ctx, tf_frame_step_fn step,
                          tf_frame_cleanup_fn cleanup, void *state) {
    tf_frame_push_native_handler(ctx, step, cleanup, NULL, state);
}

void tf_frame_pop(tf_ctx *ctx, tf_ret status) {
    if (ctx->call_stack_len == 0) return;
    tf_frame *f = &ctx->call_stack[ctx->call_stack_len - 1];

    if (f->kind == TF_FRAME_PROGRAM) {
        for (size_t i = 0; i < f->vars.len; i++) {
            tf_obj_release(f->vars.vars[i].name);
            tf_obj_release(f->vars.vars[i].val);
        }
        free(f->vars.vars);
        tf_obj_release(f->program);
    } else if (f->cleanup) {
        f->cleanup(ctx, f->state, status);
    }

    ctx->call_stack_len--;

    if (ctx->call_stack_len < ctx->call_stack_cap / 4 &&
        ctx->call_stack_cap > 64) {
        ctx->call_stack_cap /= 2;
        ctx->call_stack =
            tf_xrealloc(ctx->call_stack, sizeof(tf_frame) * ctx->call_stack_cap);
    }
}

static void frame_unwind_to(tf_ctx *ctx, size_t entry_depth, tf_ret status) {
    while (ctx->call_stack_len > entry_depth) {
        tf_frame_pop(ctx, status);
    }
}

static tf_source_span ctx_best_span(tf_ctx *ctx) {
    if (ctx->call_stack_len > 0) {
        tf_frame *f = &ctx->call_stack[ctx->call_stack_len - 1];
        if (f->kind == TF_FRAME_NATIVE && f->call_site.valid) {
            return f->call_site;
        }
    }
    if (ctx->current_span.valid) return ctx->current_span;
    return (tf_source_span){0};
}

static const char *source_basename(const char *path) {
    if (!path) return "<unknown>";
    const char *name = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') name = p + 1;
    }
    return name;
}

static const char *current_word_name(tf_ctx *ctx) {
    return ctx->current_word ? ctx->current_word : "<native>";
}

static void ctx_diagnostic_vf(tf_ctx *ctx, const char *label, const char *color,
                              const char *fmt, va_list args) {
    fprintf(stderr, "%s%s:%s ", tf_console_clr(color), label,
            tf_console_clr(TF_CLR_RESET));
    vfprintf(stderr, fmt, args);

    tf_source_span span = ctx_best_span(ctx);
    if (span.valid) {
        fprintf(stderr, "  at %s:%zu:%zu\n", source_basename(span.filename),
                span.line, span.col);
    }
}

void tf_ctx_runtime_errorf(tf_ctx *ctx, const char *fmt, ...) {
    if (ctx->error_suppression_depth > 0) return;
    va_list args;
    va_start(args, fmt);
    ctx_diagnostic_vf(ctx, "runtime error", TF_CLR_ERR, fmt, args);
    va_end(args);
    ctx->error_reported = true;
}

void tf_ctx_program_errorf(tf_ctx *ctx, const char *fmt, ...) {
    if (ctx->error_suppression_depth > 0) return;
    va_list args;
    va_start(args, fmt);
    ctx_diagnostic_vf(ctx, "program error", TF_CLR_PROGRAM_ERR, fmt, args);
    va_end(args);
    ctx->error_reported = true;
}

const char *tf_type_name(tf_type type) {
    switch (type) {
    case TF_OBJ_TYPE_BOOL:
        return "bool";
    case TF_OBJ_TYPE_INT:
        return "int";
    case TF_OBJ_TYPE_FLOAT:
        return "float";
    case TF_OBJ_TYPE_STR:
        return "string";
    case TF_OBJ_TYPE_SYMBOL:
        return "symbol";
    case TF_OBJ_TYPE_LIST:
        return "list";
    case TF_OBJ_TYPE_VARLIST:
        return "capture list";
    case TF_OBJ_TYPE_VARFETCH:
        return "variable fetch";
    }
    return "unknown";
}

const char *tf_obj_type_name(tf_obj *o) {
    return o ? tf_type_name(o->type) : "missing";
}

bool tf_ctx_require_stack(tf_ctx *ctx, size_t needed) {
    size_t found = tf_stack_len(ctx);
    if (found >= needed) return true;

    tf_ctx_runtime_errorf(ctx, "'%s' expected %zu value%s on the stack, found %zu\n",
                          current_word_name(ctx), needed, needed == 1 ? "" : "s",
                          found);
    return false;
}

bool tf_ctx_require_type(tf_ctx *ctx, size_t depth, tf_type type) {
    if (!tf_ctx_require_stack(ctx, depth + 1)) return false;

    tf_obj *o = tf_stack_peek(ctx, depth);
    if (o->type == type) return true;

    tf_ctx_runtime_errorf(ctx, "'%s' expected %s at stack depth %zu, found %s\n",
                          current_word_name(ctx), tf_type_name(type), depth,
                          tf_obj_type_name(o));
    return false;
}

bool tf_ctx_require_number(tf_ctx *ctx, size_t depth) {
    if (!tf_ctx_require_stack(ctx, depth + 1)) return false;

    tf_obj *o = tf_stack_peek(ctx, depth);
    if (o->type == TF_OBJ_TYPE_INT || o->type == TF_OBJ_TYPE_FLOAT) return true;

    tf_ctx_runtime_errorf(ctx, "'%s' expected number at stack depth %zu, found %s\n",
                          current_word_name(ctx), depth, tf_obj_type_name(o));
    return false;
}

bool tf_ctx_require_sequence(tf_ctx *ctx, size_t depth) {
    if (!tf_ctx_require_stack(ctx, depth + 1)) return false;

    tf_obj *o = tf_stack_peek(ctx, depth);
    if (o->type == TF_OBJ_TYPE_LIST || o->type == TF_OBJ_TYPE_STR) return true;

    tf_ctx_runtime_errorf(ctx, "'%s' expected sequence at stack depth %zu, found %s\n",
                          current_word_name(ctx), depth, tf_obj_type_name(o));
    return false;
}

bool tf_ctx_require_callable(tf_ctx *ctx, size_t depth) {
    if (!tf_ctx_require_stack(ctx, depth + 1)) return false;

    tf_obj *o = tf_stack_peek(ctx, depth);
    if (tf_obj_is_callable(o)) return true;

    tf_ctx_runtime_errorf(ctx, "'%s' expected callable at stack depth %zu, found %s\n",
                          current_word_name(ctx), depth, tf_obj_type_name(o));
    return false;
}

static bool frame_handle_error(tf_ctx *ctx, size_t entry_depth,
                               tf_ret status) {
    while (ctx->call_stack_len > entry_depth) {
        tf_frame *f = &ctx->call_stack[ctx->call_stack_len - 1];
        if (f->kind == TF_FRAME_NATIVE && f->on_error) {
            bool handled = false;
            tf_ret res = f->on_error(ctx, f->state, status, &handled);
            if (res != TF_OK) {
                tf_frame_pop(ctx, res);
                status = res;
                continue;
            }
            if (handled) return true;
        }
        tf_frame_pop(ctx, status);
    }
    return false;
}

/* === Word Dictionary === */

static unsigned long dict_hash(tf_obj *o) {
    unsigned long hash = 5381;
    char *ptr = o->str.ptr;
    size_t len = o->str.len;
    for (size_t i = 0; i < len; i++) { hash = ((hash << 5) + hash) + ptr[i]; }
    return hash;
}

static void dict_resize(tf_ctx *ctx) {
    size_t old_cap = ctx->words.capacity;
    tf_word **old_buckets = ctx->words.buckets;

    ctx->words.capacity *= 2;
    ctx->words.buckets = tf_xcalloc(ctx->words.capacity, sizeof(tf_word *));

    for (size_t i = 0; i < old_cap; i++) {
        tf_word *f = old_buckets[i];
        if (f) {
            unsigned long h = dict_hash(f->name);
            size_t idx = h % ctx->words.capacity;
            while (ctx->words.buckets[idx]) {
                idx = (idx + 1) % ctx->words.capacity;
            }
            ctx->words.buckets[idx] = f;
        }
    }
    free(old_buckets);
}

/* === Context Initialization === */

typedef struct {
    const char *name;
    tf_native_fn cb;
} builtin_word;

static void register_builtin_group(tf_ctx *ctx, const builtin_word *words) {
    for (size_t i = 0; words[i].name; i++) {
        tf_dict_set_native(ctx, words[i].name, words[i].cb);
    }
}

static const builtin_word native_math_words[] = {
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

static const builtin_word native_logic_words[] = {
    {"and", tf_and}, {"or", tf_or},   {"xor", tf_xor},
    {"not", tf_not}, {"shl", tf_shl}, {"shr", tf_shr},
    {NULL, NULL},
};

static const builtin_word native_stack_words[] = {
    {"dup", tf_dup},     {"drop", tf_drop}, {"swap", tf_swap},
    {"over", tf_over},   {"rot", tf_rot},   {"swapd", tf_swapd},
    {"nip", tf_nip},     {"tuck", tf_tuck}, {"pick", tf_pick},
    {"roll", tf_roll},   {"empty", tf_empty},
    {NULL, NULL},
};

static const builtin_word native_io_words[] = {
    {"printf", tf_printf}, {"print", tf_print}, {"cr", tf_cr},
    {".", tf_dot},         {".s", tf_stack},    {".S", tf_stack_source},
    {"key", tf_key},
    {"input", tf_input},   {"load", tf_load}, {"readf", tf_readf},
    {"writef", tf_writef}, {"delf", tf_delf},   {"readl", tf_readl},
    {"exists?", tf_exists_q}, {"clear", tf_clear}, {"page", tf_clear},
    {NULL, NULL},
};

static const builtin_word native_comparison_words[] = {
    {"==", tf_eq}, {"!=", tf_ne}, {"<", tf_lt},
    {">", tf_gt}, {"<=", tf_le}, {">=", tf_ge},
    {NULL, NULL},
};

static const builtin_word native_definition_words[] = {
    {":", tf_colon}, {"def", tf_def}, {NULL, NULL},
};

static const builtin_word native_control_words[] = {
    {"exec", tf_exec},       {"i", tf_exec},
    {"app2", tf_app2},       {"if", tf_if},
    {"ifelse", tf_ifelse}, {"while", tf_while},
    {"try", tf_try},       {"error", tf_error},
    {"infra", tf_infra},   {"cond", tf_cond},
    {"cleave", tf_cleave}, {"construct", tf_construct},
    {"replicate", tf_replicate}, {"times", tf_times},
    {"dip", tf_dip},       {"keep", tf_keep},
    {"bi", tf_bi},         {"linrec", tf_linrec},
    {"binrec", tf_binrec}, {"genrec", tf_genrec},
    {"treerec", tf_treerec},
    {NULL, NULL},
};

static const builtin_word native_collection_combinator_words[] = {
    {"each", tf_each},     {"map", tf_map},
    {"fold", tf_fold},     {"filter", tf_filter},
    {"some", tf_some},     {"all", tf_all},
    {"split", tf_split},   {"merge", tf_merge},
    {NULL, NULL},
};

/* Sequence data words are shared by lists and strings when the operation has
 * the same shape. String items are one-byte strings: append adds one item,
 * while concat combines two sequences. */
static const builtin_word native_data_words[] = {
    {"geth", tf_geth},       {"seth", tf_seth},
    {"slice", tf_slice},     {"take", tf_take},
    {"dropn", tf_dropn},     {"len", tf_len},
    {"first", tf_first},     {"rest", tf_rest},
    {"uncons", tf_uncons},   {"cons", tf_cons},
    {"append", tf_append},   {"concat", tf_concat},
    {"reverse", tf_reverse},
    {"join", tf_join},       {"trim", tf_trim},
    {"upper", tf_upper},     {"lower", tf_lower},
    {"splitmid", tf_splitmid},
    {"range", tf_range},     {"empty?", tf_empty_q},
    {"char?", tf_char_q},    {"letter?", tf_letter_q},
    {"digit?", tf_digit_q},  {"alnum?", tf_alnum_q},
    {"space?", tf_space_q},  {"upper?", tf_upper_q},
    {"lower?", tf_lower_q},  {"punct?", tf_punct_q},
    {NULL, NULL},
};

static const builtin_word native_introspection_words[] = {
    {"typeof", tf_typeof},   {"bool?", tf_bool_q},
    {"int?", tf_int_q},      {"float?", tf_float_q},
    {"str?", tf_str_q},      {"symbol?", tf_symbol_q},
    {"list?", tf_list_q},       {"number?", tf_number_q},
    {"sequence?", tf_sequence_q},
    {"callable?", tf_callable_q},
    {"nan?", tf_nan_q},         {"inf?", tf_inf_q},
    {"word?", tf_word_q},    {"var?", tf_var_q},
    {"inf", tf_inf},         {"nan", tf_nan},
    {"body", tf_body},       {"intern", tf_intern},
    {"name", tf_name},       {"words", tf_words},
    {"see", tf_see},
    {NULL, NULL},
};

static const builtin_word native_system_words[] = {
    {"rand", tf_rand},       {"sleep", tf_sleep},
    {"argc", tf_argc},       {"argv", tf_argv},
    {"env?", tf_env_q},      {"getenv", tf_getenv},
    {"setenv", tf_setenv},
    {"pwd", tf_pwd},         {"shell", tf_shell},
    {"time", tf_time},       {"clock", tf_clock},
    {"bye", tf_exit},        {"exit", tf_exit},
    {NULL, NULL},
};

tf_ctx *tf_ctx_new(int argc, char **argv) {
    srand(time(NULL));
    tf_ctx *ctx = tf_xmalloc(sizeof(tf_ctx));
    ctx->data_stack = tf_obj_new_list();
    ctx->words.capacity = 128;
    ctx->words.count = 0;
    ctx->words.buckets = tf_xcalloc(ctx->words.capacity, sizeof(tf_word *));
    ctx->call_stack = NULL;
    ctx->call_stack_len = 0;
    ctx->call_stack_cap = 0;
    ctx->argc = argc;
    ctx->argv = argv;
    ctx->error_suppression_depth = 0;
    ctx->error_reported = false;
    ctx->current_span = (tf_source_span){0};
    ctx->current_word = NULL;

    register_builtin_group(ctx, native_math_words);
    register_builtin_group(ctx, native_logic_words);
    register_builtin_group(ctx, native_stack_words);
    register_builtin_group(ctx, native_io_words);
    register_builtin_group(ctx, native_comparison_words);
    register_builtin_group(ctx, native_definition_words);
    register_builtin_group(ctx, native_control_words);
    register_builtin_group(ctx, native_collection_combinator_words);
    register_builtin_group(ctx, native_data_words);
    register_builtin_group(ctx, native_introspection_words);
    register_builtin_group(ctx, native_system_words);

    return ctx;
}

void tf_ctx_free(tf_ctx *ctx) {
    tf_obj_release(ctx->data_stack);
    for (size_t i = 0; i < ctx->words.capacity; i++) {
        tf_word *f = ctx->words.buckets[i];
        if (f) {
            tf_obj_release(f->name);
            if (f->type == TF_WORD_USER) { tf_obj_release(f->user_impl); }
            free(f);
        }
    }
    free(ctx->words.buckets);
    while (ctx->call_stack_len > 0) tf_frame_pop(ctx, TF_OK);
    free(ctx->call_stack);
    free(ctx);
}

static tf_word *dict_insert_word(tf_ctx *ctx, tf_obj *name) {
    if (ctx->words.count >= ctx->words.capacity * 0.7) {
        dict_resize(ctx);
    }

    unsigned long h = dict_hash(name);
    size_t idx = h % ctx->words.capacity;
    while (ctx->words.buckets[idx]) {
        idx = (idx + 1) % ctx->words.capacity;
    }

    tf_word *f = tf_xmalloc(sizeof(tf_word));
    f->name = name;
    tf_obj_retain(name);
    f->type = TF_WORD_NATIVE;
    f->native_impl = NULL;
    ctx->words.buckets[idx] = f;
    ctx->words.count++;
    return f;
}

void tf_dict_set_native(tf_ctx *ctx, const char *name, tf_native_fn cb) {
    tf_obj *o_name = tf_obj_new_symbol(name, strlen(name));
    tf_word *f = tf_dict_lookup(ctx, o_name);
    if (f) {  // overwrite if name is already taken
        if (f->type == TF_WORD_USER) tf_obj_release(f->user_impl);
    } else {  // allocate if name is not taken
        f = dict_insert_word(ctx, o_name);
    }
    f->type = TF_WORD_NATIVE;
    f->native_impl = cb;
    tf_obj_release(o_name);
}

void tf_dict_set_user(tf_ctx *ctx, tf_obj *name, tf_obj *uf) {
    tf_word *f = tf_dict_lookup(ctx, name);
    if (f) {
        if (f->type == TF_WORD_USER) { tf_obj_release(f->user_impl); }
    } else {
        f = dict_insert_word(ctx, name);
    }
    f->type = TF_WORD_USER;
    f->user_impl = uf;
    tf_obj_retain(uf);
}

tf_word *tf_dict_lookup(tf_ctx *ctx, tf_obj *name) {
    if (!name || name->type != TF_OBJ_TYPE_SYMBOL) return NULL;
    if (ctx->words.capacity == 0) return NULL;
    unsigned long h = dict_hash(name);
    size_t idx = h % ctx->words.capacity;
    // linear probing
    while (ctx->words.buckets[idx]) {
        if (tf_obj_compare_string(ctx->words.buckets[idx]->name, name) == 0) {
            return ctx->words.buckets[idx];
        }
        idx = (idx + 1) % ctx->words.capacity;
    }
    return NULL;
}

/* === Dynamic Capture Scope === */

static void scope_bind_var(tf_ctx *ctx, tf_obj *name, tf_obj *val) {
    if (ctx->call_stack_len == 0) return;
    tf_frame *f = &ctx->call_stack[ctx->call_stack_len - 1];
    if (f->kind != TF_FRAME_PROGRAM) return;

    // check if variable already exists in current frame and update it
    for (int i = (int)f->vars.len - 1; i >= 0; i--) {
        if (tf_obj_compare_string(f->vars.vars[i].name, name) == 0) {
            tf_obj_release(f->vars.vars[i].val);
            f->vars.vars[i].val = val;
            tf_obj_retain(val);
            return;
        }
    }

    // otherwise append new binding
    if (f->vars.len >= f->vars.cap) {
        f->vars.cap = f->vars.cap == 0 ? 16 : f->vars.cap * 2;
        f->vars.vars = tf_xrealloc(f->vars.vars, sizeof(tf_var) * f->vars.cap);
    }
    f->vars.vars[f->vars.len].name = name;
    f->vars.vars[f->vars.len].val = val;
    tf_obj_retain(name);
    tf_obj_retain(val);
    f->vars.len++;
}

tf_obj *tf_scope_lookup_var(tf_ctx *ctx, tf_obj *name) {
    for (int i = (int)ctx->call_stack_len - 1; i >= 0; i--) {
        tf_frame *f = &ctx->call_stack[i];
        if (f->kind != TF_FRAME_PROGRAM) continue;
        for (int j = (int)f->vars.len - 1; j >= 0; j--) {
            if (tf_obj_compare_string(f->vars.vars[j].name, name) == 0) {
                return f->vars.vars[j].val;
            }
        }
    }
    return NULL;
}

static volatile sig_atomic_t interrupted = 0;
void tf_vm_handle_sigint(int sig) {
    (void)sig;
    signal(SIGINT, tf_vm_handle_sigint);
    interrupted = 1;
}

/*
 * Dispatch a dictionary entry that has already been resolved by tf_dict_lookup().
 * User words schedule program frames; native words run immediately and may
 * schedule continuations before returning.
 */
static tf_ret dict_call_resolved(tf_ctx *ctx, tf_word *word) {
    if (word->type == TF_WORD_USER) {
        tf_frame_push_program(ctx, word->user_impl);
        return TF_OK;
    }
    return word->native_impl(ctx);
}

/*
 * The main iterative execution engine.
 * Instead of recursive C calls, it uses an explicit `call_stack` of frames.
 * This ensures deep user-defined word recursion does not overflow the C stack.
 */
tf_ret tf_vm_exec(tf_ctx *ctx, tf_obj *program) {
    ctx->current_span = program ? program->span : (tf_source_span){0};
    if (program->type != TF_OBJ_TYPE_LIST) {
        if (ctx->error_suppression_depth == 0) {
            tf_ctx_runtime_errorf(ctx, "attempted to execute non-block object\n");
        }
        return TF_ERR;
    }

    /* Run until this invocation's root frame has been popped. Native words
     * that need to resume after user code use TF_FRAME_NATIVE continuations
     * instead of re-entering tf_vm_exec(). */
    size_t entry_depth = ctx->call_stack_len;
    ctx->error_reported = false;
    tf_frame_push_program(ctx, program);

    while (ctx->call_stack_len > entry_depth) {
        if (interrupted) {
            frame_unwind_to(ctx, entry_depth, TF_INTERRUPTED);
            interrupted = 0;  // reset for next run
            return TF_INTERRUPTED;
        }

        tf_frame *f = &ctx->call_stack[ctx->call_stack_len - 1];
        if (f->kind == TF_FRAME_NATIVE) {
            bool done = false;
            tf_ret cont_res = f->step(ctx, f->state, &done);
            if (cont_res != TF_OK) {
                if (ctx->error_suppression_depth == 0 && !ctx->error_reported) {
                    tf_ctx_runtime_errorf(ctx, "execution failed\n");
                }
                if (frame_handle_error(ctx, entry_depth, cont_res)) continue;
                return cont_res;
            }
            if (done) tf_frame_pop(ctx, TF_OK);
            continue;
        }

        if (f->pc >= f->program->list.len) {
            tf_frame_pop(ctx, TF_OK);
            continue;
        }

        tf_obj *o = f->program->list.elem[f->pc++];
        ctx->current_span = o->span;
        switch (o->type) {
        case TF_OBJ_TYPE_SYMBOL:
            if (o->str.quoted) {
                tf_stack_push(ctx, o);
                tf_obj_retain(o);
            } else {
                ctx->current_word = o->str.ptr;
                tf_word *word = tf_dict_lookup(ctx, o);
                if (!word) {
                    if (ctx->error_suppression_depth == 0) {
                        tf_ctx_runtime_errorf(ctx, "undefined word '%s'\n",
                                              o->str.ptr);
                    }
                    if (frame_handle_error(ctx, entry_depth, TF_ERR)) {
                        continue;
                    }
                    return TF_ERR;
                }
                tf_ret call_res = dict_call_resolved(ctx, word);
                if (call_res == TF_INTERRUPTED) {
                    frame_unwind_to(ctx, entry_depth, TF_INTERRUPTED);
                    return TF_INTERRUPTED;
                }
                if (call_res == TF_ERR) {
                    if (ctx->error_suppression_depth == 0 &&
                        !ctx->error_reported) {
                        tf_ctx_runtime_errorf(ctx, "execution of word '%s' failed\n",
                                              o->str.ptr);
                    }
                    // unwind remaining frames
                    if (frame_handle_error(ctx, entry_depth, TF_ERR)) {
                        continue;
                    }
                    return TF_ERR;
                }
            }
            break;
        case TF_OBJ_TYPE_VARLIST:
            for (int i = (int)o->list.len - 1; i >= 0; i--) {
                tf_obj *val = tf_stack_pop(ctx);
                if (!val) {
                    if (ctx->error_suppression_depth == 0) {
                        tf_ctx_runtime_errorf(ctx,
                            "stack underflow during variable binding\n");
                    }
                    if (frame_handle_error(ctx, entry_depth, TF_ERR)) {
                        continue;
                    }
                    return TF_ERR;
                }
                scope_bind_var(ctx, o->list.elem[i], val);
                tf_obj_release(val);
            }
            break;
        case TF_OBJ_TYPE_VARFETCH: {
            tf_obj *val = tf_scope_lookup_var(ctx, o);
            if (!val) {
                if (ctx->error_suppression_depth == 0) {
                    tf_ctx_runtime_errorf(ctx, "undefined variable '$%s'\n",
                                          o->str.ptr);
                }
                if (frame_handle_error(ctx, entry_depth, TF_ERR)) {
                    continue;
                }
                return TF_ERR;
            }
            tf_stack_push(ctx, val);
            tf_obj_retain(val);
            break;
        }
        default:
            tf_stack_push(ctx, o);
            tf_obj_retain(o);
            break;
        }
    }
    return TF_OK;
}

bool tf_obj_is_callable(tf_obj *o) {
    return o && (o->type == TF_OBJ_TYPE_LIST || o->type == TF_OBJ_TYPE_SYMBOL);
}

tf_ret tf_vm_call_callable(tf_ctx *ctx, tf_obj *callable) {
    if (callable->type == TF_OBJ_TYPE_LIST) {
        tf_frame_push_program(ctx, callable);
        return TF_OK;
    }
    if (callable->type == TF_OBJ_TYPE_SYMBOL) {
        tf_word *word = tf_dict_lookup(ctx, callable);
        if (!word) return TF_ERR;
        return dict_call_resolved(ctx, word);
    }
    return TF_ERR;
}
