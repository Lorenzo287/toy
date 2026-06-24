#ifndef TF_EXEC_H
#define TF_EXEC_H

#include "tf_obj.h"

typedef enum { TF_OK, TF_ERR, TF_INTERRUPTED, TF_REPL_COMMAND } tf_ret;

typedef struct tf_ctx tf_ctx;
typedef tf_ret (*tf_native_fn)(tf_ctx *ctx);
typedef tf_ret (*tf_frame_step_fn)(tf_ctx *ctx, void *state, bool *done);
typedef void (*tf_frame_cleanup_fn)(tf_ctx *ctx, void *state, tf_ret status);
typedef tf_ret (*tf_frame_error_fn)(tf_ctx *ctx, void *state, tf_ret status,
                                    bool *handled);

/* Native registration metadata shared with interactive tooling. */
typedef struct {
    const char *name;
    tf_native_fn cb;
} tf_builtin_word;

typedef struct {
    const char *title;
    const tf_builtin_word *words;
} tf_builtin_group;

typedef enum { TF_WORD_NATIVE, TF_WORD_USER } tf_word_kind;
/*
 * Global dictionary entry.
 *
 * Native words point at C functions. User words point at Toy quotations. Names
 * are stored as refcounted symbol objects so dictionary ownership follows the
 * same rules as the rest of the runtime.
 */
typedef struct {
    const char *name;
    size_t name_len;
    bool owns_name;
    tf_word_kind type;
    union {
        tf_native_fn native_impl;
        tf_obj *user_impl;
    };
} tf_word;

/*
 * Open-addressed global word dictionary.
 *
 * Definitions live in a dense array; buckets store one-based entry indexes.
 * A tf_word* returned by tf_dict_lookup() is transient and must not be retained
 * across dictionary mutation.
 */
typedef struct {
    tf_word *entries;
    size_t entry_capacity;
    size_t *buckets;
    size_t capacity;
    size_t count;
} tf_word_table;

typedef struct {
    tf_obj *name;
    tf_obj *val;
} tf_var;

typedef struct {
    tf_var *vars;
    size_t len;
    size_t cap;
    tf_var inline_var;
} tf_var_table;

typedef enum { TF_FRAME_PROGRAM, TF_FRAME_NATIVE } tf_frame_kind;

typedef struct {
    tf_obj *program;
    size_t pc;
    tf_var_table vars;
} tf_program_frame;

typedef struct {
    tf_frame_step_fn step;
    tf_frame_cleanup_fn cleanup;
    tf_frame_error_fn on_error;
    void *state;
} tf_native_frame;

/*
 * Execution frame.
 *
 * Program frames evaluate Toy vectors with a program counter and dynamic capture
 * bindings. Native frames are continuations used by C words that need to resume
 * after scheduled Toy code finishes.
 */
typedef struct {
    tf_frame_kind kind;
    union {
        tf_program_frame program;
        tf_native_frame native;
    } as;
    tf_source_span call_site;
} tf_frame;

/*
 * Interpreter state shared by the VM and native words.
 */
struct tf_ctx {
    tf_obj *data_stack;
    tf_word_table words;
    tf_frame *call_stack;  // explicit execution stack
    size_t call_stack_len;
    size_t call_stack_cap;

    int argc;
    char **argv;
    size_t error_suppression_depth;
    bool error_reported;
    bool program_error;
    tf_source_span current_span;
    const char *current_word;
};

/* Data stack API used by native word implementations. */
size_t tf_stack_len(tf_ctx *ctx);
void tf_stack_push(tf_ctx *ctx, tf_obj *o);
tf_obj *tf_stack_pop(tf_ctx *ctx);
tf_obj *tf_stack_pop_type(tf_ctx *ctx, tf_type type);
tf_obj *tf_stack_pop_callable(tf_ctx *ctx);
tf_obj *tf_stack_peek(tf_ctx *ctx, size_t depth);

/*
 * Native-word validation helpers.
 *
 * Depth is counted from the top of the stack: depth 0 is the next value that
 * would be popped. Helpers report a ctx-aware diagnostic and return false on
 * failure; callers should then return TF_ERR without modifying the stack.
 */
const char *tf_type_name(tf_type type);
const char *tf_obj_type_name(tf_obj *o);
bool tf_ctx_require_stack(tf_ctx *ctx, size_t needed);
bool tf_ctx_require_type(tf_ctx *ctx, size_t depth, tf_type type);
bool tf_ctx_require_number(tf_ctx *ctx, size_t depth);
bool tf_ctx_require_sequence(tf_ctx *ctx, size_t depth);
bool tf_ctx_require_callable(tf_ctx *ctx, size_t depth);

/* Execution frame scheduling. Native words schedule work here, then return. */
void tf_frame_push_program(tf_ctx *ctx, tf_obj *program);
void tf_frame_push_native(tf_ctx *ctx, tf_frame_step_fn step,
                          tf_frame_cleanup_fn cleanup, void *state);
void tf_frame_push_native_handler(tf_ctx *ctx, tf_frame_step_fn step,
                                  tf_frame_cleanup_fn cleanup,
                                  tf_frame_error_fn on_error, void *state);
void tf_frame_pop(tf_ctx *ctx, tf_ret status);

/* Context lifecycle. */
tf_ctx *tf_ctx_new(int argc, char **argv);
void tf_ctx_free(tf_ctx *ctx);

/* Read-only native catalog, in presentation order. */
const tf_builtin_group *tf_builtin_groups(size_t *count);

/* Global word dictionary. */
void tf_dict_set_native(tf_ctx *ctx, const char *name, tf_native_fn cb);
void tf_dict_set_user(tf_ctx *ctx, tf_obj *name, tf_obj *uf);
tf_word *tf_dict_lookup(tf_ctx *ctx, tf_obj *name);
tf_word *tf_dict_lookup_name(tf_ctx *ctx, const char *name, size_t len);

/* Dynamic capture lookup across active program frames. */
tf_obj *tf_scope_lookup_var(tf_ctx *ctx, tf_obj *name);

/* VM entry points. */
tf_ret tf_vm_exec(tf_ctx *ctx, tf_obj *program);
bool tf_obj_is_callable(tf_obj *o);
tf_ret tf_vm_call_callable(tf_ctx *ctx, tf_obj *callable);
void tf_vm_handle_sigint(int sig);

/* Context-aware diagnostics used by the VM and native words. */
void tf_ctx_runtime_errorf(tf_ctx *ctx, const char *fmt, ...);
void tf_ctx_program_errorf(tf_ctx *ctx, const char *fmt, ...);

#endif  // TF_EXEC_H
