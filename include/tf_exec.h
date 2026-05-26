#ifndef TF_EXEC_H
#define TF_EXEC_H

#include "tf_obj.h"

typedef enum { TF_OK, TF_ERR, TF_INTERRUPTED } tf_ret;
typedef struct ctx tf_ctx;

typedef tf_ret (*tf_callback)(tf_ctx *ctx);
typedef tf_ret (*tf_native_frame_step)(tf_ctx *ctx, void *state, bool *done);
typedef void (*tf_native_frame_cleanup)(tf_ctx *ctx, void *state,
                                        tf_ret status);
typedef tf_ret (*tf_native_frame_error)(tf_ctx *ctx, void *state,
                                        tf_ret status, bool *handled);

typedef enum { TF_FUNC_TYPE_NATIVE, TF_FUNC_TYPE_USER } tf_func_type;
typedef enum { TF_FRAME_PROGRAM, TF_FRAME_NATIVE } tf_frame_kind;

// NOTE: name is stored as a heap-allocated tf_obj* rather than a plain char*
// so that it participates in the reference counting model — retain_obj() is
// called when the function is registered and release_obj() when it is
// overwritten or freed, preventing leaks and keeping ownership rules uniform
// across all string data in the interpreter.
typedef struct {
    tf_obj *name;
    tf_func_type type;
    union {
        tf_callback native_impl;
        tf_obj *user_impl;
    };
} tf_func;

// NOTE: use double pointer indirection for the hash table buckets so that
// each tf_func is allocated independently at a stable heap address; when the
// table resizes and the buckets array is reallocated, any tf_func* pointer
// returned by get_func() remains valid. Storing tf_func structs directly in
// the buckets array would invalidate such pointers on resize.
typedef struct {
    tf_func **buckets;
    size_t capacity;
    size_t count;
} tf_func_table;

typedef struct {
    tf_obj *name;
    tf_obj *val;
} tf_var;

typedef struct {
    tf_var *vars;
    size_t len;
    size_t cap;
} tf_var_table;

typedef struct {
    tf_frame_kind kind;
    tf_obj *prg;
    size_t pc;  // program counter
    tf_var_table vars;
    tf_native_frame_step step;
    tf_native_frame_cleanup cleanup;
    tf_native_frame_error on_error;
    void *state;
} tf_frame;

struct ctx {
    tf_obj *forth_stack;  // forth data stack
    tf_func_table functions;
    tf_frame *call_stack;  // funtions call stack
    size_t cstack_len;
    size_t cstack_cap;

    int argc;
    char **argv;
    size_t error_suppression_depth;
    bool error_reported;
};

size_t stack_len(tf_ctx *ctx);
void stack_push(tf_ctx *ctx, tf_obj *o);
tf_obj *stack_pop(tf_ctx *ctx);
tf_obj *stack_pop_type(tf_ctx *ctx, tf_type type);
tf_obj *stack_pop_callable(tf_ctx *ctx);
tf_obj *stack_peek(tf_ctx *ctx, size_t depth);

void frame_push(tf_ctx *ctx, tf_obj *prg);
void native_frame_push(tf_ctx *ctx, tf_native_frame_step step,
                       tf_native_frame_cleanup cleanup, void *state);
void native_frame_push_handler(tf_ctx *ctx, tf_native_frame_step step,
                               tf_native_frame_cleanup cleanup,
                               tf_native_frame_error on_error, void *state);
void frame_pop(tf_ctx *ctx);

tf_ctx *init_ctx(int argc, char **argv);
void free_ctx(tf_ctx *ctx);
tf_func *init_func(tf_ctx *ctx, tf_obj *name);
void set_native_func(tf_ctx *ctx, const char *name, tf_callback cb);
void set_user_func(tf_ctx *ctx, tf_obj *name, tf_obj *uf);
tf_func *get_func(tf_ctx *ctx, tf_obj *name);
tf_obj *tf_var_fetch(tf_ctx *ctx, tf_obj *name);

tf_ret exec(tf_ctx *ctx, tf_obj *prg);
tf_ret call_symbol(tf_ctx *ctx, tf_obj *symb);
bool tf_is_callable(tf_obj *o);
tf_ret tf_call_callable(tf_ctx *ctx, tf_obj *callable);

#endif  // TF_EXEC_H
