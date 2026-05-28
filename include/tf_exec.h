#ifndef TF_EXEC_H
#define TF_EXEC_H

#include "tf_obj.h"

typedef enum { TF_OK, TF_ERR, TF_INTERRUPTED } tf_ret;

typedef struct tf_ctx tf_ctx;
typedef tf_ret (*tf_native_fn)(tf_ctx *ctx);
typedef tf_ret (*tf_frame_step_fn)(tf_ctx *ctx, void *state, bool *done);
typedef void (*tf_frame_cleanup_fn)(tf_ctx *ctx, void *state, tf_ret status);
typedef tf_ret (*tf_frame_error_fn)(tf_ctx *ctx, void *state, tf_ret status,
                                    bool *handled);

typedef enum { TF_WORD_NATIVE, TF_WORD_USER } tf_word_kind;
/*
 * Global dictionary entry.
 *
 * Native words point at C functions. User words point at Toy quotations. Names
 * are stored as refcounted symbol objects so dictionary ownership follows the
 * same rules as the rest of the runtime.
 */
typedef struct {
    tf_obj *name;
    tf_word_kind type;
    union {
        tf_native_fn native_impl;
        tf_obj *user_impl;
    };
} tf_word;

/*
 * Open-addressed global word dictionary.
 *
 * Entries are individually allocated so a tf_word* returned by
 * tf_dict_lookup() remains valid when the bucket array resizes.
 */
typedef struct {
    tf_word **buckets;
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
} tf_var_table;

typedef enum { TF_FRAME_PROGRAM, TF_FRAME_NATIVE } tf_frame_kind;
/*
 * Execution frame.
 *
 * Program frames evaluate Toy lists with a program counter and dynamic capture
 * bindings. Native frames are continuations used by C words that need to resume
 * after scheduled Toy code finishes.
 */
typedef struct {
    tf_frame_kind kind;
    tf_obj *program;
    size_t pc;  // program counter
    tf_var_table vars;
    tf_frame_step_fn step;
    tf_frame_cleanup_fn cleanup;
    tf_frame_error_fn on_error;
    void *state;
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
};

/* Data stack API used by native word implementations. */
size_t tf_stack_len(tf_ctx *ctx);
void tf_stack_push(tf_ctx *ctx, tf_obj *o);
tf_obj *tf_stack_pop(tf_ctx *ctx);
tf_obj *tf_stack_pop_type(tf_ctx *ctx, tf_type type);
tf_obj *tf_stack_pop_callable(tf_ctx *ctx);
tf_obj *tf_stack_peek(tf_ctx *ctx, size_t depth);

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

/* Global word dictionary. */
void tf_dict_set_native(tf_ctx *ctx, const char *name, tf_native_fn cb);
void tf_dict_set_user(tf_ctx *ctx, tf_obj *name, tf_obj *uf);
tf_word *tf_dict_lookup(tf_ctx *ctx, tf_obj *name);

/* Dynamic capture lookup across active program frames. */
tf_obj *tf_scope_lookup_var(tf_ctx *ctx, tf_obj *name);

/* VM entry points. */
tf_ret tf_vm_exec(tf_ctx *ctx, tf_obj *program);
bool tf_obj_is_callable(tf_obj *o);
tf_ret tf_vm_call_callable(tf_ctx *ctx, tf_obj *callable);
void tf_vm_handle_sigint(int sig);

#endif  // TF_EXEC_H
