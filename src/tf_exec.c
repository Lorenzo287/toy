#include "tf_exec.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "tf_alloc.h"
#include "tf_console.h"
#include "tf_lib.h"  // IWYU pragma: keep
#include <signal.h>

#define TF_CALL_STACK_INITIAL_CAP 8
#define TF_CAPTURE_INITIAL_CAP 4
#define TF_ERROR_STACK_LIMIT 8
#define TF_ERROR_FRAME_LIMIT 8

_Static_assert((TF_WORD_LOOKUP_CACHE_CAP & (TF_WORD_LOOKUP_CACHE_CAP - 1)) == 0,
               "word lookup cache capacity must be a power of two");

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

static bool frame_is_program(tf_frame_kind kind) {
    return kind != TF_FRAME_NATIVE;
}

static void frame_push_program_kind(tf_ctx *ctx, tf_obj *program,
                                    tf_frame_kind kind) {
    ensure_call_stack_slot(ctx);
    ctx->call_stack[ctx->call_stack_len].kind = kind;
    ctx->call_stack[ctx->call_stack_len].as.program.program = program;
    ctx->call_stack[ctx->call_stack_len].as.program.pc = 0;
    ctx->call_stack[ctx->call_stack_len].as.program.vars.vars = NULL;
    ctx->call_stack[ctx->call_stack_len].as.program.vars.len = 0;
    ctx->call_stack[ctx->call_stack_len].as.program.vars.cap = 0;
    ctx->call_stack[ctx->call_stack_len].call_site = ctx->current_span;
    tf_obj_retain(program);
    ctx->call_stack_len++;
}

void tf_frame_push_program(tf_ctx *ctx, tf_obj *program) {
    frame_push_program_kind(ctx, program, TF_FRAME_PROGRAM);
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

    if (frame_is_program(f->kind)) {
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

static void ctx_print_error_stack(tf_ctx *ctx, const char *color) {
    size_t len = tf_stack_len(ctx);
    size_t start = len > TF_ERROR_STACK_LIMIT ? len - TF_ERROR_STACK_LIMIT : 0;

    fprintf(stderr, "  stack %s<%zu>%s", tf_console_clr(color), len, 
			tf_console_clr(TF_CLR_RESET));
    if (start > 0) fprintf(stderr, " ...");
    for (size_t i = start; i < len; i++) {
        fprintf(stderr, " ");
        tf_obj_fprint_display(stderr, tf_stack_peek(ctx, len - 1 - i));
    }
    fprintf(stderr, "\n");
}

static size_t ctx_program_frame_count(tf_ctx *ctx) {
    size_t program_count = 0;
    size_t frame_count = tf_debug_frame_count(ctx);
    for (size_t depth = 0; depth < frame_count; depth++) {
        tf_debug_frame_info frame;
        if (tf_debug_get_frame(ctx, depth, &frame) &&
            frame.kind == TF_FRAME_PROGRAM) {
            program_count++;
        }
    }
    return program_count;
}

static void ctx_print_error_frames(tf_ctx *ctx) {
    size_t program_count = ctx_program_frame_count(ctx);
    if (program_count <= 1) return;

    size_t printed = 0;
    size_t frame_count = tf_debug_frame_count(ctx);
    for (size_t depth = 0;
         depth < frame_count && printed < TF_ERROR_FRAME_LIMIT; depth++) {
        tf_debug_frame_info frame;
        if (!tf_debug_get_frame(ctx, depth, &frame) ||
            frame.kind != TF_FRAME_PROGRAM) {
            continue;
        }

        tf_source_span location = printed == 0 ? ctx_best_span(ctx)
                                                : frame.location;
        fprintf(stderr, "  in %s",
                frame.word_name ? frame.word_name : "<program>");
        if (location.source) {
            fprintf(stderr, " at %s:%zu:%zu",
                    source_basename(tf_source_file_name(location.source)),
                    (size_t)location.line, (size_t)location.col);
        }
        fprintf(stderr, "\n");
        printed++;
    }

    if (program_count > printed) {
        fprintf(stderr, "  ... %zu more frame%s\n", program_count - printed,
                program_count - printed == 1 ? "" : "s");
    }
}

static void ctx_print_error_context(tf_ctx *ctx, const char* color) {
    ctx_print_error_stack(ctx, color);
    ctx_print_error_frames(ctx);
}

static const char *current_word_name(tf_ctx *ctx) {
    return ctx->current_word ? ctx->current_word : "<native>";
}

void tf_ctx_clear_error(tf_ctx *ctx) {
    if (!ctx) return;
    free(ctx->last_error);
    ctx->last_error = NULL;
}

void tf_ctx_set_error(tf_ctx *ctx, const char *message) {
    if (!ctx) return;
    tf_ctx_clear_error(ctx);
    if (message) ctx->last_error = tf_xstrdup(message);
}

const char *tf_ctx_last_error(tf_ctx *ctx) {
    return ctx ? ctx->last_error : NULL;
}

static void ctx_store_error_vf(tf_ctx *ctx, const char *fmt, va_list args) {
    va_list count_args;
    va_copy(count_args, args);
    int length = vsnprintf(NULL, 0, fmt, count_args);
    va_end(count_args);
    if (length < 0) return;

    char *message = tf_xmalloc((size_t)length + 1);
    va_list write_args;
    va_copy(write_args, args);
    vsnprintf(message, (size_t)length + 1, fmt, write_args);
    va_end(write_args);
    while (length > 0 &&
           (message[length - 1] == '\n' || message[length - 1] == '\r')) {
        message[--length] = '\0';
    }

    tf_ctx_clear_error(ctx);
    ctx->last_error = message;
}

static void ctx_diagnostic_vf(tf_ctx *ctx, const char *label, const char *color,
                              const char *fmt, va_list args) {
    ctx_store_error_vf(ctx, fmt, args);
    fprintf(stderr, "%s%s:%s ", tf_console_clr(color), label,
            tf_console_clr(TF_CLR_RESET));
    vfprintf(stderr, fmt, args);

    tf_source_span span = ctx_best_span(ctx);
    if (span.source) {
        fprintf(stderr, "  at %s:%zu:%zu\n",
                source_basename(tf_source_file_name(span.source)),
                (size_t)span.line, (size_t)span.col);
    }
    ctx_print_error_context(ctx, color);
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
    case TF_OBJ_TYPE_CALL:
        return "call";
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

#include "tf_builtins.inc"

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
    srand((unsigned int)time(NULL));
    tf_ctx *ctx = tf_xmalloc(sizeof(tf_ctx));
    ctx->data_stack = tf_obj_new_vector();
    size_t builtin_count = builtin_word_count();
    ctx->words.entry_capacity = builtin_count + 16;
    ctx->words.entries =
        tf_xmalloc(sizeof(tf_word) * ctx->words.entry_capacity);
    ctx->words.capacity = word_table_capacity_for(builtin_count);
    ctx->words.count = 0;
    ctx->words.buckets = tf_xcalloc(ctx->words.capacity, sizeof(size_t));
    memset(ctx->words.lookup_cache, 0, sizeof(ctx->words.lookup_cache));
    ctx->call_stack = NULL;
    ctx->call_stack_len = 0;
    ctx->call_stack_cap = 0;
    ctx->argc = argc;
    ctx->argv = argv;
    ctx->error_suppression_depth = 0;
    ctx->error_reported = false;
    ctx->program_error = false;
    ctx->suppress_repl_status = false;
    ctx->interrupted = 0;
    ctx->last_error = NULL;
    ctx->current_span = (tf_source_span){0};
    ctx->current_word = NULL;
    ctx->debug_hook = NULL;
    ctx->debug_userdata = NULL;

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
    free(ctx->last_error);
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
    if (!name || (name->type != TF_OBJ_TYPE_SYMBOL &&
                  name->type != TF_OBJ_TYPE_CALL)) {
        return NULL;
    }

    uintptr_t object_key = (uintptr_t)name;
    uintptr_t mixed_key = object_key >> 4;
    mixed_key ^= mixed_key >> 7;
    mixed_key ^= mixed_key >> 13;
    size_t slot = mixed_key & (TF_WORD_LOOKUP_CACHE_CAP - 1);
    uintptr_t cached_key = ctx->words.lookup_cache[slot].key;
    size_t entry_index = ctx->words.lookup_cache[slot].entry_index;
    if (cached_key == object_key && entry_index < ctx->words.count) {
        tf_word *word = &ctx->words.entries[entry_index];
        if (word->name_len == name->str.len &&
            memcmp(word->name, name->str.ptr, name->str.len) == 0) {
            return word;
        }
    }

    tf_word *word = tf_dict_lookup_name(ctx, name->str.ptr, name->str.len);
    if (word) {
        ctx->words.lookup_cache[slot].key = object_key;
        ctx->words.lookup_cache[slot].entry_index =
            (size_t)(word - ctx->words.entries);
    } else {
        ctx->words.lookup_cache[slot].key = 0;
    }
    return word;
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
    if (!frame_is_program(f->kind)) return;

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
        if (!frame_is_program(f->kind)) continue;
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

void tf_ctx_interrupt(tf_ctx *ctx) {
    if (ctx) ctx->interrupted = 1;
}

/*
 * Dispatch a dictionary entry that has already been resolved by tf_dict_lookup().
 * User words schedule program frames; native words run immediately and may
 * schedule continuations before returning.
 */
static tf_ret dict_call_resolved(tf_ctx *ctx, tf_word *word) {
    if (word->type == TF_WORD_USER) {
        frame_push_program_kind(ctx, word->user_impl, TF_FRAME_PROGRAM_USER);
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
    tf_ctx_clear_error(ctx);
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
    bool debug_continuing = false;
    ctx->error_reported = false;
    ctx->program_error = false;
    frame_push_program_kind(ctx, program, TF_FRAME_PROGRAM_ROOT);

    while (ctx->call_stack_len > entry_depth) {
        if (ctx->interrupted) {
            frame_unwind_to(ctx, entry_depth, TF_INTERRUPTED);
            ctx->interrupted = 0;  // reset for next run
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

        size_t pc = f->as.program.pc;
        tf_obj *o = f->as.program.program->vector.elem[pc];
        ctx->current_span = o->span;
        if (!debug_continuing && ctx->debug_hook) {
            tf_debug_event event = {o, o->span, pc, ctx->call_stack_len};
            tf_debug_action action =
                ctx->debug_hook(ctx, &event, ctx->debug_userdata);
            if (action == TF_DEBUG_ABORT) {
                frame_unwind_to(ctx, entry_depth, TF_INTERRUPTED);
                ctx->interrupted = 0;
                return TF_INTERRUPTED;
            }
            if (action == TF_DEBUG_CONTINUE) debug_continuing = true;
        }
        f->as.program.pc++;
        switch (o->type) {
        case TF_OBJ_TYPE_CALL: {
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
            if (call_res == TF_EXIT_REQUESTED) {
                frame_unwind_to(ctx, entry_depth, TF_EXIT_REQUESTED);
                return TF_EXIT_REQUESTED;
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
            break;
        }
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
    return o && (o->type == TF_OBJ_TYPE_VECTOR ||
                 o->type == TF_OBJ_TYPE_SYMBOL ||
                 o->type == TF_OBJ_TYPE_CALL);
}

tf_ret tf_vm_call_callable(tf_ctx *ctx, tf_obj *callable) {
    if (callable->type == TF_OBJ_TYPE_VECTOR) {
        tf_frame_push_program(ctx, callable);
        return TF_OK;
    }
    if (callable->type == TF_OBJ_TYPE_SYMBOL ||
        callable->type == TF_OBJ_TYPE_CALL) {
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

void tf_debug_set_hook(tf_ctx *ctx, tf_debug_hook_fn hook, void *userdata) {
    ctx->debug_hook = hook;
    ctx->debug_userdata = userdata;
}

size_t tf_debug_frame_count(tf_ctx *ctx) {
    return ctx->call_stack_len;
}

static const char *debug_user_word_name(tf_ctx *ctx, size_t frame_index,
                                        tf_frame *frame) {
    if (frame_index > 0) {
        tf_frame *caller = &ctx->call_stack[frame_index - 1];
        if (frame_is_program(caller->kind) && caller->as.program.pc > 0) {
            tf_obj *instruction = caller->as.program.program->vector.elem[
                caller->as.program.pc - 1];
            if (instruction->type == TF_OBJ_TYPE_CALL) {
                return instruction->str.ptr;
            }
        }
    }
    for (size_t i = 0; i < ctx->words.count; i++) {
        tf_word *word = &ctx->words.entries[i];
        if (word->type == TF_WORD_USER &&
            word->user_impl == frame->as.program.program) {
            return word->name;
        }
    }
    return "<user word>";
}

bool tf_debug_get_frame(tf_ctx *ctx, size_t depth,
                        tf_debug_frame_info *info) {
    if (!info || depth >= ctx->call_stack_len) return false;
    size_t frame_index = ctx->call_stack_len - 1 - depth;
    tf_frame *frame = &ctx->call_stack[frame_index];
    info->kind = frame_is_program(frame->kind) ? TF_FRAME_PROGRAM
                                               : TF_FRAME_NATIVE;
    info->call_site = frame->call_site;
    info->location = frame->call_site;
    info->word_name = NULL;
    info->pc = 0;
    info->program_len = 0;
    if (frame_is_program(frame->kind)) {
        if (frame->kind == TF_FRAME_PROGRAM_ROOT) {
            info->word_name = "<program>";
        } else if (frame->kind == TF_FRAME_PROGRAM_USER) {
            info->word_name = debug_user_word_name(ctx, frame_index, frame);
        } else {
            info->word_name = "<quotation>";
        }
        info->pc = frame->as.program.pc;
        info->program_len = frame->as.program.program->vector.len;
        if (depth == 0 && info->pc < info->program_len) {
            info->location =
                frame->as.program.program->vector.elem[info->pc]->span;
        } else if (info->pc > 0) {
            info->location =
                frame->as.program.program->vector.elem[info->pc - 1]->span;
        }
    }
    return true;
}

static tf_var_table *debug_frame_vars(tf_ctx *ctx, size_t depth) {
    if (depth >= ctx->call_stack_len) return NULL;
    size_t frame_index = ctx->call_stack_len - 1 - depth;
    tf_frame *frame = &ctx->call_stack[frame_index];
    if (!frame_is_program(frame->kind)) return NULL;
    return &frame->as.program.vars;
}

size_t tf_debug_capture_count(tf_ctx *ctx, size_t frame_depth) {
    tf_var_table *vars = debug_frame_vars(ctx, frame_depth);
    return vars ? vars->len : 0;
}

bool tf_debug_get_capture(tf_ctx *ctx, size_t frame_depth, size_t index,
                          tf_debug_capture_info *info) {
    tf_var_table *vars = debug_frame_vars(ctx, frame_depth);
    if (!info || !vars || index >= vars->len) return false;
    tf_var *bindings = vars->vars ? vars->vars : &vars->inline_var;
    info->name = bindings[index].name->str.ptr;
    info->value = bindings[index].val;
    return true;
}

bool tf_debug_lookup_capture(tf_ctx *ctx, const char *name, size_t name_len,
                             tf_debug_capture_info *info) {
    if (!name || !info) return false;
    for (size_t depth = 0; depth < ctx->call_stack_len; depth++) {
        tf_var_table *vars = debug_frame_vars(ctx, depth);
        if (!vars) continue;
        tf_var *bindings = vars->vars ? vars->vars : &vars->inline_var;
        for (size_t i = vars->len; i > 0; i--) {
            tf_obj *binding_name = bindings[i - 1].name;
            if (binding_name->str.len == name_len &&
                memcmp(binding_name->str.ptr, name, name_len) == 0) {
                info->name = binding_name->str.ptr;
                info->value = bindings[i - 1].val;
                return true;
            }
        }
    }
    return false;
}

size_t tf_debug_word_count(tf_ctx *ctx) {
    return ctx->words.count;
}

static void debug_fill_word_info(tf_word *word, tf_debug_word_info *info) {
    info->name = word->name;
    info->user_defined = word->type == TF_WORD_USER;
    info->body = info->user_defined ? word->user_impl : NULL;
}

bool tf_debug_get_word(tf_ctx *ctx, size_t index, tf_debug_word_info *info) {
    if (!info || index >= ctx->words.count) return false;
    debug_fill_word_info(&ctx->words.entries[index], info);
    return true;
}

bool tf_debug_find_word(tf_ctx *ctx, const char *name, size_t name_len,
                        tf_debug_word_info *info) {
    if (!name || !info) return false;
    tf_word *word = tf_dict_lookup_name(ctx, name, name_len);
    if (!word) return false;
    debug_fill_word_info(word, info);
    return true;
}
