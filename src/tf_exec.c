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

#define TF_CALL_STACK_INITIAL_CAP 8
#define TF_CAPTURE_INITIAL_CAP 4

/* === Data Stack === */

size_t tf_stack_len(tf_ctx *ctx) {
    return ctx->data_stack->vector.len;
}

void tf_stack_push(tf_ctx *ctx, tf_obj *o) {
    tf_vector_push(ctx->data_stack, o);
}

tf_obj *tf_stack_pop(tf_ctx *ctx) {
    return tf_vector_pop(ctx->data_stack);
}

tf_obj *tf_stack_pop_type(tf_ctx *ctx, tf_type type) {
    return tf_vector_pop_type(ctx->data_stack, type);
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
    return ctx->data_stack->vector.elem[len - 1 - depth];
}

/* === Execution Frames === */

static void ensure_call_stack_slot(tf_ctx *ctx) {
    if (ctx->call_stack_len < ctx->call_stack_cap) return;
    ctx->call_stack_cap = ctx->call_stack_cap == 0
                              ? TF_CALL_STACK_INITIAL_CAP
                              : ctx->call_stack_cap * 2;
    ctx->call_stack =
        tf_xrealloc(ctx->call_stack, sizeof(tf_frame) * ctx->call_stack_cap);
}

void tf_frame_push_program(tf_ctx *ctx, tf_obj *program) {
    ensure_call_stack_slot(ctx);
    ctx->call_stack[ctx->call_stack_len].kind = TF_FRAME_PROGRAM;
    ctx->call_stack[ctx->call_stack_len].as.program.program = program;
    ctx->call_stack[ctx->call_stack_len].as.program.pc = 0;
    ctx->call_stack[ctx->call_stack_len].as.program.vars.vars = NULL;
    ctx->call_stack[ctx->call_stack_len].as.program.vars.len = 0;
    ctx->call_stack[ctx->call_stack_len].as.program.vars.cap = 0;
    ctx->call_stack[ctx->call_stack_len].call_site = ctx->current_span;
    tf_obj_retain(program);
    ctx->call_stack_len++;
}

void tf_frame_push_native_handler(tf_ctx *ctx, tf_frame_step_fn step,
                                  tf_frame_cleanup_fn cleanup,
                                  tf_frame_error_fn on_error, void *state) {
    ensure_call_stack_slot(ctx);
    ctx->call_stack[ctx->call_stack_len].kind = TF_FRAME_NATIVE;
    ctx->call_stack[ctx->call_stack_len].as.native.step = step;
    ctx->call_stack[ctx->call_stack_len].as.native.cleanup = cleanup;
    ctx->call_stack[ctx->call_stack_len].as.native.on_error = on_error;
    ctx->call_stack[ctx->call_stack_len].as.native.state = state;
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
        tf_var *vars = f->as.program.vars.vars
                           ? f->as.program.vars.vars
                           : &f->as.program.vars.inline_var;
        for (size_t i = 0; i < f->as.program.vars.len; i++) {
            tf_obj_release(vars[i].name);
            tf_obj_release(vars[i].val);
        }
        free(f->as.program.vars.vars);
        tf_obj_release(f->as.program.program);
    } else if (f->as.native.cleanup) {
        f->as.native.cleanup(ctx, f->as.native.state, status);
    }

    ctx->call_stack_len--;
}

static void frame_unwind_to(tf_ctx *ctx, size_t entry_depth, tf_ret status) {
    while (ctx->call_stack_len > entry_depth) {
        tf_frame_pop(ctx, status);
    }
}

static tf_source_span ctx_best_span(tf_ctx *ctx) {
    if (ctx->call_stack_len > 0) {
        tf_frame *f = &ctx->call_stack[ctx->call_stack_len - 1];
        if (f->kind == TF_FRAME_NATIVE && f->call_site.source) {
            return f->call_site;
        }
    }
    if (ctx->current_span.source) return ctx->current_span;
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
    if (span.source) {
        fprintf(stderr, "  at %s:%zu:%zu\n",
                source_basename(tf_source_file_name(span.source)),
                (size_t)span.line, (size_t)span.col);
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
    case TF_OBJ_TYPE_VECTOR:
        return "vector";
    case TF_OBJ_TYPE_LIST:
        return "list";
    case TF_OBJ_TYPE_MAP:
        return "map";
    case TF_OBJ_TYPE_SET:
        return "set";
    case TF_OBJ_TYPE_DEQUE:
        return "deque";
    case TF_OBJ_TYPE_PQUEUE:
        return "pqueue";
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
    if (o->type == TF_OBJ_TYPE_VECTOR || o->type == TF_OBJ_TYPE_LIST ||
        o->type == TF_OBJ_TYPE_STR) return true;

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
        if (f->kind == TF_FRAME_NATIVE && f->as.native.on_error) {
            bool handled = false;
            tf_ret res = f->as.native.on_error(ctx, f->as.native.state, status,
                                               &handled);
            if (res != TF_OK) {
                tf_frame_pop(ctx, res);
                status = res;
                continue;
            }
            if (handled) {
                ctx->program_error = false;
                return true;
            }
        }
        tf_frame_pop(ctx, status);
    }
    return false;
}

/* === Word Dictionary === */

static unsigned long dict_hash(const char *name, size_t len) {
    unsigned long hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)name[i];
    }
    return hash;
}

static void dict_resize(tf_ctx *ctx) {
    size_t *old_buckets = ctx->words.buckets;

    ctx->words.capacity *= 2;
    ctx->words.buckets = tf_xcalloc(ctx->words.capacity, sizeof(size_t));

    for (size_t i = 0; i < ctx->words.count; i++) {
        tf_word *word = &ctx->words.entries[i];
        unsigned long h = dict_hash(word->name, word->name_len);
        size_t idx = h % ctx->words.capacity;
        while (ctx->words.buckets[idx]) {
            idx = (idx + 1) % ctx->words.capacity;
        }
        ctx->words.buckets[idx] = i + 1;
    }
    free(old_buckets);
}

/* === Context Initialization === */

static void register_builtin_group(tf_ctx *ctx, const tf_builtin_group *group) {
    for (size_t i = 0; group->words[i].name; i++) {
        tf_dict_set_native(ctx, group->words[i].name, group->words[i].cb);
    }
}

static const tf_builtin_word native_math_words[] = {
    {"+", tf_add},       {"-", tf_sub},     {"*", tf_mul},
    {"/", tf_div},       {"%", tf_rem},     {"mod", tf_mod},
    {"neg", tf_neg},     {"abs", tf_abs},   {"max", tf_max},
    {"min", tf_min},     {"sqrt", tf_sqrt}, {"pow", tf_pow},
    {"exp", tf_exp},     {"log", tf_log},   {"log10", tf_log10},
    {"sin", tf_sin},     {"cos", tf_cos},   {"tan", tf_tan},
    {"floor", tf_floor}, {"ceil", tf_ceil}, {"round", tf_round},
    {"pred", tf_pred},   {"succ", tf_succ}, {"square", tf_square},
    {"cube", tf_cube},   {"pi", tf_pi},     {"e", tf_e},
    {"tau", tf_tau},     {"inf", tf_inf},   {"nan", tf_nan},
    {"inf?", tf_inf_q},  {"nan?", tf_nan_q}, {"rand", tf_rand},
    {NULL, NULL},
};

static const tf_builtin_word native_logic_words[] = {
    {"and", tf_and}, {"or", tf_or},   {"xor", tf_xor},
    {"not", tf_not}, {"shl", tf_shl}, {"shr", tf_shr},
    {NULL, NULL},
};

static const tf_builtin_word native_stack_words[] = {
    {"dup", tf_dup},     {"drop", tf_drop}, {"swap", tf_swap},
    {"over", tf_over},   {"rot", tf_rot},   {"swapd", tf_swapd},
    {"nip", tf_nip},     {"tuck", tf_tuck}, {"pick", tf_pick},
    {"roll", tf_roll},   {"empty", tf_empty},
    {NULL, NULL},
};

static const tf_builtin_word native_console_words[] = {
    {"printf", tf_printf}, {"print", tf_print},
    {".", tf_dot},         {".s", tf_stack},
    {".S", tf_stack_source}, {"read-key", tf_key},
    {"read-line", tf_input}, {"clear", tf_clear},
    {NULL, NULL},
};

static const tf_builtin_word native_file_words[] = {
    {"load", tf_load},       {"read-file", tf_readf},
    {"write-file", tf_writef}, {"delete-file", tf_delf},
    {"read-lines", tf_readl}, {"file-exists?", tf_exists_q},
    {NULL, NULL},
};

static const tf_builtin_word native_comparison_words[] = {
    {"==", tf_eq}, {"!=", tf_ne}, {"<", tf_lt},
    {">", tf_gt}, {"<=", tf_le}, {">=", tf_ge},
    {NULL, NULL},
};

static const tf_builtin_word native_control_words[] = {
    {"exec", tf_exec},     {"i", tf_exec},
    {"if", tf_if},         {"ifelse", tf_ifelse},
    {"while", tf_while},   {"cond", tf_cond},
    {"try", tf_try},       {"error", tf_error},
    {NULL, NULL},
};

static const tf_builtin_word native_combinator_words[] = {
    {"app2", tf_app2},       {"infra", tf_infra},
    {"cleave", tf_cleave},   {"construct", tf_construct},
    {"replicate", tf_replicate}, {"times", tf_times},
    {"dip", tf_dip},         {"keep", tf_keep},
    {"bi", tf_bi},           {"linrec", tf_linrec},
    {"binrec", tf_binrec},   {"genrec", tf_genrec},
    {"treerec", tf_treerec},
    {NULL, NULL},
};

static const tf_builtin_word native_sequence_combinator_words[] = {
    {"each", tf_each},     {"map", tf_map},
    {"fold", tf_fold},     {"filter", tf_filter},
    {"some", tf_some},     {"all", tf_all},
    {"split", tf_split},   {"merge", tf_merge},
    {NULL, NULL},
};

/* Ordered collection words are grouped by concept for help output, not by the
 * C module that implements them. Shared words still dispatch by capability. */
static const tf_builtin_word native_sequence_words[] = {
    {">vector", tf_to_vector}, {">list", tf_to_list},
    {">string", tf_to_string}, {">deque", tf_to_deque},
    {"at", tf_at},           {"set-at", tf_set_at},
    {"slice", tf_slice},     {"take", tf_take},
    {"skip", tf_dropn},      {"len", tf_len},
    {"concat", tf_concat},   {"reverse", tf_reverse},
    {"split-mid", tf_splitmid}, {"range", tf_range},
    {"empty?", tf_empty_q},
    {NULL, NULL},
};

static const tf_builtin_word native_endpoint_words[] = {
    {"first", tf_first},     {"last", tf_last},
    {"rest", tf_rest},       {"uncons", tf_uncons},
    {"cons", tf_cons},       {"push-front", tf_push_front},
    {"push-back", tf_push_back}, {"pop-front", tf_pop_front},
    {"pop-back", tf_pop_back},
    {NULL, NULL},
};

static const tf_builtin_word native_sequence_algorithm_words[] = {
    {"contains?", tf_contains_q}, {"index-of", tf_indexof},
    {"unique", tf_unique},   {"sort", tf_sort},
    {NULL, NULL},
};

static const tf_builtin_word native_string_words[] = {
    {"join", tf_join},       {"trim", tf_trim},
    {"upper", tf_upper},     {"lower", tf_lower},
    {"char?", tf_char_q},    {">char", tf_to_char},
    {"char-code", tf_char_code},
    {"letter?", tf_letter_q},
    {"digit?", tf_digit_q},  {"alnum?", tf_alnum_q},
    {"space?", tf_space_q},  {"upper?", tf_upper_q},
    {"lower?", tf_lower_q},  {"punct?", tf_punct_q},
    {NULL, NULL},
};

static const tf_builtin_word native_map_set_words[] = {
    {">map", tf_to_map},     {">set", tf_to_set},
    {"has?", tf_has_q},      {"get", tf_get},
    {"get-or", tf_get_or},
    {"assoc", tf_assoc},     {"dissoc", tf_dissoc},
    {"keys", tf_keys},       {"values", tf_values},
    {"pairs", tf_pairs},     {"items", tf_items},
    {"insert", tf_insert},   {"remove", tf_remove},
    {NULL, NULL},
};

static const tf_builtin_word native_set_algebra_words[] = {
    {"union", tf_union},     {"intersection", tf_intersection},
    {"difference", tf_difference},
    {"symmetric-difference", tf_symmetric_difference},
    {"subset?", tf_subset_q}, {"proper-subset?", tf_proper_subset_q},
    {"superset?", tf_superset_q},
    {"proper-superset?", tf_proper_superset_q},
    {"disjoint?", tf_disjoint_q},
    {NULL, NULL},
};

static const tf_builtin_word native_pqueue_words[] = {
    {">pqueue", tf_to_pqueue}, {"pq-push", tf_pq_push},
    {"pq-peek", tf_pq_peek},
    {"pq-pop", tf_pq_pop},
    {NULL, NULL},
};

static const tf_builtin_word native_type_words[] = {
    {"type-of", tf_typeof},  {"bool?", tf_bool_q},
    {"int?", tf_int_q},      {"float?", tf_float_q},
    {"string?", tf_string_q}, {"symbol?", tf_symbol_q},
    {"vector?", tf_vector_q}, {"list?", tf_list_q},
    {"map?", tf_map_q},      {"set?", tf_set_q},
    {"deque?", tf_deque_q},  {"pqueue?", tf_pqueue_q},
    {"number?", tf_number_q}, {"sequence?", tf_sequence_q},
    {"callable?", tf_callable_q},
    {NULL, NULL},
};

static const tf_builtin_word native_dictionary_words[] = {
    {"def", tf_def},       {"word?", tf_word_q},
    {"var?", tf_var_q},   {"body", tf_body},
    {"intern", tf_intern}, {"name", tf_name},
    {"words", tf_words},   {"see", tf_see},
    {"doc", tf_doc},       {"search-words", tf_apropos},
    {"repr", tf_repr},
    {NULL, NULL},
};

static const tf_builtin_word native_system_words[] = {
    {"argc", tf_argc},       {"argv", tf_argv},
    {"env?", tf_env_q},
    {"get-env", tf_getenv},  {"set-env", tf_setenv},
    {"pwd", tf_pwd},         {"shell", tf_shell},
    {"exit", tf_exit},
    {NULL, NULL},
};

static const tf_builtin_word native_time_words[] = {
    {"sleep", tf_sleep},       {"unix-time", tf_unix_time},
    {"local-time", tf_local_time}, {"utc-time", tf_utc_time},
    {"cpu-time", tf_cpu_time}, {"monotonic-ns", tf_monotonic_ns},
    {NULL, NULL},
};

static const tf_builtin_group native_builtin_groups[] = {
    {"Stack", native_stack_words},
    {"Math", native_math_words},
    {"Logic / Bitwise", native_logic_words},
    {"Comparison", native_comparison_words},
    {"Control", native_control_words},
    {"Combinators", native_combinator_words},
    {"Sequence Combinators", native_sequence_combinator_words},
    {"Ordered Collections", native_sequence_words},
    {"Collection Endpoints", native_endpoint_words},
    {"Sequence Search / Ordering", native_sequence_algorithm_words},
    {"Strings / Characters", native_string_words},
    {"Maps / Sets", native_map_set_words},
    {"Set Algebra", native_set_algebra_words},
    {"Priority Queues", native_pqueue_words},
    {"Types", native_type_words},
    {"Definitions / Introspection", native_dictionary_words},
    {"Console", native_console_words},
    {"Files", native_file_words},
    {"Environment / Processes", native_system_words},
    {"Time", native_time_words},
};

static size_t builtin_word_count(void) {
    size_t count = 0;
    size_t group_count =
        sizeof(native_builtin_groups) / sizeof(native_builtin_groups[0]);
    for (size_t i = 0; i < group_count; i++) {
        for (size_t j = 0; native_builtin_groups[i].words[j].name; j++) count++;
    }
    return count;
}

static size_t word_table_capacity_for(size_t count) {
    size_t capacity = 8;
    while (count >= capacity * 7 / 10) capacity *= 2;
    return capacity;
}

const tf_builtin_group *tf_builtin_groups(size_t *count) {
    if (count) {
        *count = sizeof(native_builtin_groups) / sizeof(native_builtin_groups[0]);
    }
    return native_builtin_groups;
}

tf_ctx *tf_ctx_new(int argc, char **argv) {
    srand(time(NULL));
    tf_ctx *ctx = tf_xmalloc(sizeof(tf_ctx));
    ctx->data_stack = tf_obj_new_vector();
    size_t builtin_count = builtin_word_count();
    ctx->words.entry_capacity = builtin_count + 16;
    ctx->words.entries =
        tf_xmalloc(sizeof(tf_word) * ctx->words.entry_capacity);
    ctx->words.capacity = word_table_capacity_for(builtin_count);
    ctx->words.count = 0;
    ctx->words.buckets = tf_xcalloc(ctx->words.capacity, sizeof(size_t));
    ctx->call_stack = NULL;
    ctx->call_stack_len = 0;
    ctx->call_stack_cap = 0;
    ctx->argc = argc;
    ctx->argv = argv;
    ctx->error_suppression_depth = 0;
    ctx->error_reported = false;
    ctx->program_error = false;
    ctx->current_span = (tf_source_span){0};
    ctx->current_word = NULL;

    size_t group_count = 0;
    const tf_builtin_group *groups = tf_builtin_groups(&group_count);
    for (size_t i = 0; i < group_count; i++) {
        register_builtin_group(ctx, &groups[i]);
    }

    return ctx;
}

void tf_ctx_free(tf_ctx *ctx) {
    tf_obj_release(ctx->data_stack);
    for (size_t i = 0; i < ctx->words.count; i++) {
        tf_word *word = &ctx->words.entries[i];
        if (word->owns_name) free((char *)word->name);
        if (word->type == TF_WORD_USER) tf_obj_release(word->user_impl);
    }
    free(ctx->words.entries);
    free(ctx->words.buckets);
    while (ctx->call_stack_len > 0) tf_frame_pop(ctx, TF_OK);
    free(ctx->call_stack);
    free(ctx);
}

static tf_word *dict_insert_word(tf_ctx *ctx, const char *name, size_t name_len,
                                 bool copy_name) {
    if (ctx->words.count >= ctx->words.capacity * 0.7) {
        dict_resize(ctx);
    }

    unsigned long h = dict_hash(name, name_len);
    size_t idx = h % ctx->words.capacity;
    while (ctx->words.buckets[idx]) {
        idx = (idx + 1) % ctx->words.capacity;
    }

    if (ctx->words.count >= ctx->words.entry_capacity) {
        ctx->words.entry_capacity *= 2;
        ctx->words.entries =
            tf_xrealloc(ctx->words.entries,
                        sizeof(tf_word) * ctx->words.entry_capacity);
    }
    size_t entry_idx = ctx->words.count++;
    tf_word *f = &ctx->words.entries[entry_idx];
    if (copy_name) {
        char *owned_name = tf_xmalloc(name_len + 1);
        memcpy(owned_name, name, name_len);
        owned_name[name_len] = '\0';
        f->name = owned_name;
    } else {
        f->name = name;
    }
    f->name_len = name_len;
    f->owns_name = copy_name;
    f->type = TF_WORD_NATIVE;
    f->native_impl = NULL;
    ctx->words.buckets[idx] = entry_idx + 1;
    return f;
}

void tf_dict_set_native(tf_ctx *ctx, const char *name, tf_native_fn cb) {
    size_t name_len = strlen(name);
    tf_word *f = tf_dict_lookup_name(ctx, name, name_len);
    if (f) {  // overwrite if name is already taken
        if (f->type == TF_WORD_USER) tf_obj_release(f->user_impl);
    } else {  // allocate if name is not taken
        f = dict_insert_word(ctx, name, name_len, false);
    }
    f->type = TF_WORD_NATIVE;
    f->native_impl = cb;
}

void tf_dict_set_user(tf_ctx *ctx, tf_obj *name, tf_obj *uf) {
    tf_word *f = tf_dict_lookup(ctx, name);
    if (f) {
        if (f->type == TF_WORD_USER) { tf_obj_release(f->user_impl); }
    } else {
        f = dict_insert_word(ctx, name->str.ptr, name->str.len, true);
    }
    f->type = TF_WORD_USER;
    f->user_impl = uf;
    tf_obj_retain(uf);
}

tf_word *tf_dict_lookup(tf_ctx *ctx, tf_obj *name) {
    if (!name || name->type != TF_OBJ_TYPE_SYMBOL) return NULL;
    return tf_dict_lookup_name(ctx, name->str.ptr, name->str.len);
}

tf_word *tf_dict_lookup_name(tf_ctx *ctx, const char *name, size_t len) {
    if (ctx->words.capacity == 0) return NULL;
    unsigned long h = dict_hash(name, len);
    size_t idx = h % ctx->words.capacity;
    // linear probing
    while (ctx->words.buckets[idx]) {
        tf_word *word =
            &ctx->words.entries[ctx->words.buckets[idx] - 1];
        if (word->name_len == len && memcmp(word->name, name, len) == 0) {
            return word;
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
    tf_var_table *vars = &f->as.program.vars;
    tf_var *bindings = vars->vars ? vars->vars : &vars->inline_var;
    for (int i = (int)vars->len - 1; i >= 0; i--) {
        if (tf_obj_compare_string(bindings[i].name, name) == 0) {
            tf_obj_release(bindings[i].val);
            bindings[i].val = val;
            tf_obj_retain(val);
            return;
        }
    }

    // otherwise append new binding
    if (vars->len == 1 && !vars->vars) {
        vars->cap = TF_CAPTURE_INITIAL_CAP;
        vars->vars = tf_xmalloc(sizeof(tf_var) * vars->cap);
        vars->vars[0] = vars->inline_var;
        bindings = vars->vars;
    } else if (vars->vars && vars->len >= vars->cap) {
        vars->cap = vars->cap == 0 ? TF_CAPTURE_INITIAL_CAP : vars->cap * 2;
        vars->vars = tf_xrealloc(vars->vars, sizeof(tf_var) * vars->cap);
        bindings = vars->vars;
    }
    bindings[vars->len].name = name;
    bindings[vars->len].val = val;
    tf_obj_retain(name);
    tf_obj_retain(val);
    vars->len++;
}

tf_obj *tf_scope_lookup_var(tf_ctx *ctx, tf_obj *name) {
    for (int i = (int)ctx->call_stack_len - 1; i >= 0; i--) {
        tf_frame *f = &ctx->call_stack[i];
        if (f->kind != TF_FRAME_PROGRAM) continue;
        tf_var_table *vars = &f->as.program.vars;
        tf_var *bindings = vars->vars ? vars->vars : &vars->inline_var;
        for (int j = (int)vars->len - 1; j >= 0; j--) {
            if (tf_obj_compare_string(bindings[j].name, name) == 0) {
                return bindings[j].val;
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
    if (program->type != TF_OBJ_TYPE_VECTOR) {
        if (ctx->error_suppression_depth == 0) {
            tf_ctx_runtime_errorf(ctx, "attempted to execute non-vector object\n");
        }
        return TF_ERR;
    }

    /* Run until this invocation's root frame has been popped. Native words
     * that need to resume after user code use TF_FRAME_NATIVE continuations
     * instead of re-entering tf_vm_exec(). */
    size_t entry_depth = ctx->call_stack_len;
    ctx->error_reported = false;
    ctx->program_error = false;
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
            tf_ret cont_res =
                f->as.native.step(ctx, f->as.native.state, &done);
            if (cont_res != TF_OK) {
                if (cont_res == TF_ERR) {
                    if (ctx->error_suppression_depth == 0 && !ctx->error_reported) {
                        tf_ctx_runtime_errorf(ctx, "execution failed\n");
                    }
                    if (frame_handle_error(ctx, entry_depth, cont_res)) continue;
                } else {
                    frame_unwind_to(ctx, entry_depth, cont_res);
                }
                return cont_res;
            }
            if (done) tf_frame_pop(ctx, TF_OK);
            continue;
        }

        if (f->as.program.pc >= f->as.program.program->vector.len) {
            tf_frame_pop(ctx, TF_OK);
            continue;
        }

        tf_obj *o =
            f->as.program.program->vector.elem[f->as.program.pc++];
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
                if (call_res == TF_REPL_COMMAND) {
                    frame_unwind_to(ctx, entry_depth, TF_REPL_COMMAND);
                    return TF_REPL_COMMAND;
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
            for (int i = (int)o->vector.len - 1; i >= 0; i--) {
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
                scope_bind_var(ctx, o->vector.elem[i], val);
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
    return o && (o->type == TF_OBJ_TYPE_VECTOR || o->type == TF_OBJ_TYPE_SYMBOL);
}

tf_ret tf_vm_call_callable(tf_ctx *ctx, tf_obj *callable) {
    if (callable->type == TF_OBJ_TYPE_VECTOR) {
        tf_frame_push_program(ctx, callable);
        return TF_OK;
    }
    if (callable->type == TF_OBJ_TYPE_SYMBOL) {
        tf_word *word = tf_dict_lookup(ctx, callable);
        if (!word) {
            tf_ctx_runtime_errorf(ctx, "undefined word '%s'\n",
                                  callable->str.ptr);
            return TF_ERR;
        }
        return dict_call_resolved(ctx, word);
    }
    return TF_ERR;
}
