#include "tf_lib.h"
#include <string.h>
#include <stdlib.h>
#include "tf_exec.h"
#include "tf_alloc.h"
#include "tf_obj.h"

tf_ret tf_exec(tf_ctx *ctx) {
    if (!tf_ctx_require_callable(ctx, 0)) return TF_ERR;
    tf_obj *o = tf_stack_pop_callable(ctx);

    tf_ret res = tf_vm_call_callable(ctx, o);
    tf_obj_release(o);
    return res;
}

tf_ret tf_app2(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 3) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }

    tf_obj *callable = tf_stack_pop(ctx);
    tf_obj *b = tf_stack_pop(ctx);
    tf_obj *a = tf_stack_pop(ctx);

    tf_obj *synthetic = tf_obj_new_vector();
    tf_obj *exec_sym = tf_obj_new_symbol("exec", 4);

    tf_vector_push(synthetic, a);
    tf_vector_push(synthetic, callable);
    tf_obj_retain(callable);
    tf_vector_push(synthetic, exec_sym);
    tf_obj_retain(exec_sym);

    tf_vector_push(synthetic, b);
    tf_vector_push(synthetic, callable);
    tf_vector_push(synthetic, exec_sym);

    tf_frame_push_program(ctx, synthetic);
    tf_obj_release(synthetic);

    return TF_OK;
}

static void restore_stack_owned(tf_ctx *ctx, tf_obj **saved_stack,
                                size_t saved_len) {
    while (tf_stack_len(ctx) > 0) {
        tf_obj *o = tf_stack_pop(ctx);
        tf_obj_release(o);
    }
    for (size_t i = 0; i < saved_len; i++) tf_stack_push(ctx, saved_stack[i]);
}

static void restore_stack_copy(tf_ctx *ctx, tf_obj **saved_stack,
                               size_t saved_len) {
    while (tf_stack_len(ctx) > 0) {
        tf_obj *o = tf_stack_pop(ctx);
        tf_obj_release(o);
    }
    for (size_t i = 0; i < saved_len; i++) {
        tf_stack_push(ctx, saved_stack[i]);
        tf_obj_retain(saved_stack[i]);
    }
}

static void push_retained(tf_obj *vector, tf_obj *elem) {
    tf_obj_retain(elem);
    tf_vector_push(vector, elem);
}

// Sequence combinators treat strings as byte sequences. A string item is a
// one-byte string so the same callable protocol works for vectors, lists, and
// strings.
static bool is_sequence(tf_obj *o) {
    return o->type == TF_OBJ_TYPE_VECTOR || o->type == TF_OBJ_TYPE_LIST ||
           o->type == TF_OBJ_TYPE_STR;
}

static bool require_condition(tf_ctx *ctx, size_t depth) {
    if (!tf_ctx_require_stack(ctx, depth + 1)) return false;
    tf_obj *o = tf_stack_peek(ctx, depth);
    if (o->type == TF_OBJ_TYPE_BOOL || tf_obj_is_callable(o)) return true;

    tf_ctx_runtime_errorf(ctx,
                          "'%s' expected bool or callable at stack depth %zu, found %s\n",
                          ctx->current_word, depth, tf_obj_type_name(o));
    return false;
}

static bool require_callable_vector(tf_ctx *ctx, tf_obj *vector, const char *word) {
    if (vector->type != TF_OBJ_TYPE_VECTOR) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected vector, found %s\n", word,
                              tf_obj_type_name(vector));
        return false;
    }
    for (size_t i = 0; i < vector->vector.len; i++) {
        if (!tf_obj_is_callable(vector->vector.elem[i])) {
            tf_ctx_runtime_errorf(
                ctx, "'%s' expected vector item %zu to be callable, found %s\n",
                word, i, tf_obj_type_name(vector->vector.elem[i]));
            return false;
        }
    }
    return true;
}

static size_t sequence_len(tf_obj *seq) {
    if (seq->type == TF_OBJ_TYPE_VECTOR) return seq->vector.len;
    if (seq->type == TF_OBJ_TYPE_LIST) return seq->list.len;
    return seq->str.len;
}

static tf_obj *sequence_item_owned(tf_obj *seq, size_t idx) {
    if (seq->type == TF_OBJ_TYPE_VECTOR) {
        tf_obj *elem = seq->vector.elem[idx];
        tf_obj_retain(elem);
        return elem;
    }
    if (seq->type == TF_OBJ_TYPE_LIST) {
        tf_obj *elem = tf_list_get(seq, idx);
        tf_obj_retain(elem);
        return elem;
    }
    return tf_obj_new_string(seq->str.ptr + idx, 1);
}

static tf_obj *finish_vector_family(tf_obj **vector_result, bool list_result) {
    tf_obj *items = *vector_result;
    *vector_result = NULL;
    if (!list_result) return items;

    tf_obj *result = tf_list_from_vector(items);
    tf_obj_release(items);
    return result;
}

static bool is_char_string(tf_obj *o) {
    return o->type == TF_OBJ_TYPE_STR && o->str.len == 1;
}

typedef struct {
    char *ptr;
    size_t len;
    size_t cap;
} bytebuf;

static void bytebuf_init(bytebuf *buf, size_t cap) {
    buf->len = 0;
    buf->cap = cap > 0 ? cap : 1;
    buf->ptr = tf_xmalloc(buf->cap + 1);
    buf->ptr[0] = '\0';
}

static void bytebuf_append(bytebuf *buf, char c) {
    if (buf->len >= buf->cap) {
        buf->cap *= 2;
        buf->ptr = tf_xrealloc(buf->ptr, buf->cap + 1);
    }
    buf->ptr[buf->len++] = c;
    buf->ptr[buf->len] = '\0';
}

static tf_obj *bytebuf_to_string(bytebuf *buf) {
    return tf_obj_new_string(buf->ptr, buf->len);
}

static void save_stack_copy(tf_ctx *ctx, tf_obj ***saved_stack,
                            size_t *saved_len) {
    *saved_len = tf_stack_len(ctx);
    *saved_stack =
        *saved_len > 0 ? tf_xmalloc(sizeof(tf_obj *) * *saved_len) : NULL;
    for (size_t i = 0; i < *saved_len; i++) {
        (*saved_stack)[i] = tf_stack_peek(ctx, *saved_len - 1 - i);
        tf_obj_retain((*saved_stack)[i]);
    }
}

static void release_stack_copy(tf_obj **saved_stack, size_t saved_len) {
    for (size_t i = 0; i < saved_len; i++) tf_obj_release(saved_stack[i]);
    free(saved_stack);
}

static void clear_stack(tf_ctx *ctx) {
    while (tf_stack_len(ctx) > 0) {
        tf_obj *o = tf_stack_pop(ctx);
        tf_obj_release(o);
    }
}

static tf_ret collect_outputs(tf_ctx *ctx, size_t base_len,
                              tf_obj *outputs) {
    size_t len = tf_stack_len(ctx);
    if (len < base_len) return TF_ERR;
    for (size_t i = base_len; i < len; i++) {
        push_retained(outputs, ctx->data_stack->vector.elem[i]);
    }
    return TF_OK;
}

typedef struct {
    tf_obj *body;
    int remaining;
} times_state;

static tf_ret times_step(tf_ctx *ctx, void *state, bool *done) {
    times_state *s = state;
    if (s->remaining <= 0) {
        *done = true;
        return TF_OK;
    }

    s->remaining--;
    *done = false;
    return tf_vm_call_callable(ctx, s->body);
}

static void times_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)ctx;
    (void)status;
    times_state *s = state;
    tf_obj_release(s->body);
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj *data;
    size_t index;
} each_state;

static tf_ret each_step(tf_ctx *ctx, void *state, bool *done) {
    each_state *s = state;
    size_t len = sequence_len(s->data);
    if (s->index >= len) {
        *done = true;
        return TF_OK;
    }

    tf_stack_push(ctx, sequence_item_owned(s->data, s->index++));
    *done = false;
    return tf_vm_call_callable(ctx, s->body);
}

static void each_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)ctx;
    (void)status;
    each_state *s = state;
    tf_obj_release(s->body);
    tf_obj_release(s->data);
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj *saved;
    tf_obj **saved_stack;
    size_t saved_len;
    bool scheduled;
} dip_state;

static tf_ret dip_step(tf_ctx *ctx, void *state, bool *done) {
    dip_state *s = state;
    if (!s->scheduled) {
        s->scheduled = true;
        *done = false;
        return tf_vm_call_callable(ctx, s->body);
    }

    tf_stack_push(ctx, s->saved);
    s->saved = NULL;
    *done = true;
    return TF_OK;
}

static void dip_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    dip_state *s = state;
    if (status != TF_OK) {
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
        if (s->saved) {
            tf_stack_push(ctx, s->saved);
            s->saved = NULL;
        }
    }
    release_stack_copy(s->saved_stack, s->saved_len);
    tf_obj_release(s->body);
    if (s->saved) tf_obj_release(s->saved);
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj *saved;
    tf_obj **saved_stack;
    size_t base_len;
    bool scheduled;
} keep_state;

static tf_ret keep_step(tf_ctx *ctx, void *state, bool *done) {
    keep_state *s = state;
    if (!s->scheduled) {
        s->scheduled = true;
        tf_obj_retain(s->saved);
        tf_stack_push(ctx, s->saved);
        *done = false;
        return tf_vm_call_callable(ctx, s->body);
    }

    if (tf_stack_len(ctx) < s->base_len) return TF_ERR;

    size_t out_len = tf_stack_len(ctx) - s->base_len;
    tf_obj **outputs = out_len > 0 ? tf_xmalloc(sizeof(tf_obj *) * out_len) : NULL;
    for (size_t i = out_len; i > 0; i--) outputs[i - 1] = tf_stack_pop(ctx);

    tf_stack_push(ctx, s->saved);
    s->saved = NULL;
    for (size_t i = 0; i < out_len; i++) tf_stack_push(ctx, outputs[i]);

    free(outputs);
    *done = true;
    return TF_OK;
}

static void keep_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    keep_state *s = state;
    if (status != TF_OK) {
        restore_stack_copy(ctx, s->saved_stack, s->base_len);
        if (s->saved) {
            tf_stack_push(ctx, s->saved);
            s->saved = NULL;
        }
    }
    release_stack_copy(s->saved_stack, s->base_len);
    tf_obj_release(s->body);
    if (s->saved) tf_obj_release(s->saved);
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj *data;
    tf_obj **saved_stack;
    size_t saved_len;
    size_t index;
    bool awaiting_body;
} fold_state;

static tf_ret fold_step(tf_ctx *ctx, void *state, bool *done) {
    fold_state *s = state;

    if (s->awaiting_body) {
        if (tf_stack_len(ctx) != s->saved_len + 1) return TF_ERR;
        s->awaiting_body = false;
    }

    size_t len = sequence_len(s->data);
    if (s->index >= len) {
        *done = true;
        return TF_OK;
    }

    tf_stack_push(ctx, sequence_item_owned(s->data, s->index++));
    s->awaiting_body = true;
    *done = false;
    return tf_vm_call_callable(ctx, s->body);
}

static void fold_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    fold_state *s = state;
    if (status != TF_OK) {
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    release_stack_copy(s->saved_stack, s->saved_len);
    tf_obj_release(s->body);
    tf_obj_release(s->data);
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj *data;
    tf_obj **saved_stack;
    size_t saved_len;
    size_t index;
    bool awaiting_body;
    bool string_result;
    bool list_result;
    tf_obj *vector_result;
    bytebuf str_result;
} map_state;

static tf_ret map_step(tf_ctx *ctx, void *state, bool *done) {
    map_state *s = state;

    if (s->awaiting_body) {
        if (tf_stack_len(ctx) != s->saved_len + 1) return TF_ERR;

        tf_obj *mapped = tf_stack_pop(ctx);
        if (s->string_result) {
            if (!is_char_string(mapped)) {
                tf_obj_release(mapped);
                return TF_ERR;
            }
            bytebuf_append(&s->str_result, mapped->str.ptr[0]);
            tf_obj_release(mapped);
        } else {
            tf_vector_push(s->vector_result, mapped);
        }
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
        s->awaiting_body = false;
    }

    size_t len = sequence_len(s->data);
    if (s->index >= len) {
        if (s->string_result) {
            tf_stack_push(ctx, bytebuf_to_string(&s->str_result));
            free(s->str_result.ptr);
            s->str_result.ptr = NULL;
        } else {
            tf_stack_push(ctx,
                          finish_vector_family(&s->vector_result,
                                               s->list_result));
        }
        *done = true;
        return TF_OK;
    }

    tf_stack_push(ctx, sequence_item_owned(s->data, s->index++));
    s->awaiting_body = true;
    *done = false;
    return tf_vm_call_callable(ctx, s->body);
}

static void map_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    map_state *s = state;
    if (status != TF_OK) {
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    release_stack_copy(s->saved_stack, s->saved_len);
    tf_obj_release(s->body);
    tf_obj_release(s->data);
    if (s->string_result) {
        free(s->str_result.ptr);
    } else if (s->vector_result) {
        tf_obj_release(s->vector_result);
    }
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj **saved_stack;
    size_t saved_len;
    int remaining;
    bool awaiting_body;
    tf_obj *result;
} replicate_state;

static tf_ret replicate_step(tf_ctx *ctx, void *state, bool *done) {
    replicate_state *s = state;

    if (s->awaiting_body) {
        if (tf_stack_len(ctx) != s->saved_len + 1) return TF_ERR;
        tf_obj *item = tf_stack_pop(ctx);
        tf_vector_push(s->result, item);
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
        s->awaiting_body = false;
    }

    if (s->remaining <= 0) {
        tf_stack_push(ctx, s->result);
        s->result = NULL;
        *done = true;
        return TF_OK;
    }

    s->remaining--;
    s->awaiting_body = true;
    *done = false;
    return tf_vm_call_callable(ctx, s->body);
}

static void replicate_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    replicate_state *s = state;
    if (status != TF_OK) {
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    release_stack_copy(s->saved_stack, s->saved_len);
    tf_obj_release(s->body);
    if (s->result) tf_obj_release(s->result);
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj *data;
    tf_obj **saved_stack;
    size_t saved_len;
    bool scheduled;
    tf_obj *result;
} infra_state;

static tf_ret infra_step(tf_ctx *ctx, void *state, bool *done) {
    infra_state *s = state;
    if (!s->scheduled) {
        s->scheduled = true;
        *done = false;
        return tf_vm_call_callable(ctx, s->body);
    }

    s->result = tf_obj_new_vector();
    tf_ret res = collect_outputs(ctx, 0, s->result);
    if (res != TF_OK) return res;

    restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    tf_stack_push(ctx, s->result);
    s->result = NULL;
    *done = true;
    return TF_OK;
}

static void infra_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    infra_state *s = state;
    if (status != TF_OK) {
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    release_stack_copy(s->saved_stack, s->saved_len);
    tf_obj_release(s->body);
    tf_obj_release(s->data);
    if (s->result) tf_obj_release(s->result);
    free(s);
}

typedef struct {
    tf_obj *branches;
    tf_obj *value;
    tf_obj **saved_stack;
    size_t saved_len;
    tf_obj *outputs;
    size_t index;
    bool awaiting_branch;
    bool construct_result;
} cleave_state;

static tf_ret cleave_step(tf_ctx *ctx, void *state, bool *done) {
    cleave_state *s = state;

    if (s->awaiting_branch) {
        tf_ret res = collect_outputs(ctx, s->saved_len, s->outputs);
        if (res != TF_OK) return res;
        s->awaiting_branch = false;
        s->index++;
    }

    if (s->index >= s->branches->vector.len) {
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
        if (s->construct_result) {
            tf_stack_push(ctx, s->outputs);
            s->outputs = NULL;
        } else {
            for (size_t i = 0; i < s->outputs->vector.len; i++) {
                tf_stack_push(ctx, s->outputs->vector.elem[i]);
                tf_obj_retain(s->outputs->vector.elem[i]);
            }
        }
        *done = true;
        return TF_OK;
    }

    tf_obj *branch = s->branches->vector.elem[s->index];
    if (!tf_obj_is_callable(branch)) return TF_ERR;

    restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    tf_stack_push(ctx, s->value);
    tf_obj_retain(s->value);
    s->awaiting_branch = true;
    *done = false;
    return tf_vm_call_callable(ctx, branch);
}

static void cleave_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    cleave_state *s = state;
    if (status != TF_OK) {
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    release_stack_copy(s->saved_stack, s->saved_len);
    tf_obj_release(s->branches);
    tf_obj_release(s->value);
    if (s->outputs) tf_obj_release(s->outputs);
    free(s);
}

typedef enum {
    TF_BI_RUN_LEFT,
    TF_BI_AFTER_LEFT,
    TF_BI_RUN_RIGHT,
    TF_BI_AFTER_RIGHT
} bi_stage;

typedef struct {
    tf_obj *left;
    tf_obj *right;
    tf_obj *saved;
    tf_obj **saved_stack;
    size_t base_len;
    bi_stage stage;
    tf_obj **left_outputs;
    size_t left_out_len;
} bi_state;

static tf_ret bi_step(tf_ctx *ctx, void *state, bool *done) {
    bi_state *s = state;

    if (s->stage == TF_BI_RUN_LEFT) {
        tf_obj_retain(s->saved);
        tf_stack_push(ctx, s->saved);
        s->stage = TF_BI_AFTER_LEFT;
        *done = false;
        return tf_vm_call_callable(ctx, s->left);
    }

    if (s->stage == TF_BI_AFTER_LEFT) {
        if (tf_stack_len(ctx) < s->base_len) return TF_ERR;
        s->left_out_len = tf_stack_len(ctx) - s->base_len;
        s->left_outputs =
            s->left_out_len > 0
                ? tf_xmalloc(sizeof(tf_obj *) * s->left_out_len)
                : NULL;
        for (size_t i = s->left_out_len; i > 0; i--) {
            s->left_outputs[i - 1] = tf_stack_pop(ctx);
        }

        tf_obj_retain(s->saved);
        tf_stack_push(ctx, s->saved);
        s->stage = TF_BI_AFTER_RIGHT;
        *done = false;
        return tf_vm_call_callable(ctx, s->right);
    }

    if (tf_stack_len(ctx) < s->base_len) return TF_ERR;
    size_t right_out_len = tf_stack_len(ctx) - s->base_len;
    tf_obj **right_outputs =
        right_out_len > 0 ? tf_xmalloc(sizeof(tf_obj *) * right_out_len) : NULL;
    for (size_t i = right_out_len; i > 0; i--) {
        right_outputs[i - 1] = tf_stack_pop(ctx);
    }

    for (size_t i = 0; i < s->left_out_len; i++) {
        tf_stack_push(ctx, s->left_outputs[i]);
    }
    free(s->left_outputs);
    s->left_outputs = NULL;
    s->left_out_len = 0;

    for (size_t i = 0; i < right_out_len; i++) tf_stack_push(ctx, right_outputs[i]);
    free(right_outputs);

    *done = true;
    return TF_OK;
}

static void bi_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    bi_state *s = state;
    if (status != TF_OK) {
        restore_stack_copy(ctx, s->saved_stack, s->base_len);
        if (s->saved) {
            tf_stack_push(ctx, s->saved);
            s->saved = NULL;
        }
    }
    for (size_t i = 0; i < s->left_out_len; i++) {
        tf_obj_release(s->left_outputs[i]);
    }
    free(s->left_outputs);
    release_stack_copy(s->saved_stack, s->base_len);
    tf_obj_release(s->left);
    tf_obj_release(s->right);
    if (s->saved) tf_obj_release(s->saved);
    free(s);
}

typedef enum { TF_TRY_START, TF_TRY_BODY, TF_TRY_HANDLER } try_stage;

typedef struct {
    tf_obj *body;
    tf_obj *handler;
    tf_obj **saved_stack;
    size_t saved_len;
    try_stage stage;
    bool suppressing_errors;
} try_state;

static tf_ret try_step(tf_ctx *ctx, void *state, bool *done) {
    try_state *s = state;
    if (s->stage == TF_TRY_START) {
        ctx->error_suppression_depth++;
        s->suppressing_errors = true;
        s->stage = TF_TRY_BODY;
        *done = false;
        return tf_vm_call_callable(ctx, s->body);
    }

    if (s->stage == TF_TRY_BODY) {
        if (s->suppressing_errors) {
            ctx->error_suppression_depth--;
            s->suppressing_errors = false;
        }
        *done = true;
        return TF_OK;
    }

    *done = true;
    return TF_OK;
}

static tf_ret try_error(tf_ctx *ctx, void *state, tf_ret status,
                           bool *handled) {
    try_state *s = state;
    *handled = false;
    if (status != TF_ERR || s->stage != TF_TRY_BODY) return TF_OK;

    if (s->suppressing_errors) {
        ctx->error_suppression_depth--;
        s->suppressing_errors = false;
    }
    restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    s->stage = TF_TRY_HANDLER;

    tf_ret res = tf_vm_call_callable(ctx, s->handler);
    if (res == TF_OK) *handled = true;
    return res;
}

static void try_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    try_state *s = state;
    if (s->suppressing_errors) {
        ctx->error_suppression_depth--;
        s->suppressing_errors = false;
    }
    if (status != TF_OK && s->stage == TF_TRY_BODY) {
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    release_stack_copy(s->saved_stack, s->saved_len);
    tf_obj_release(s->body);
    tf_obj_release(s->handler);
    free(s);
}

typedef enum {
    TF_PRED_IDLE,
    TF_PRED_AWAITING
} predicate_eval_phase;

typedef struct {
    predicate_eval_phase phase;
    tf_obj **saved_stack;
    size_t saved_len;
} predicate_eval;

static void predicate_eval_init(predicate_eval *eval) {
    eval->phase = TF_PRED_IDLE;
    eval->saved_stack = NULL;
    eval->saved_len = 0;
}

static void predicate_eval_clear(predicate_eval *eval) {
    eval->phase = TF_PRED_IDLE;
    eval->saved_stack = NULL;
    eval->saved_len = 0;
}

static tf_ret predicate_eval_step(tf_ctx *ctx, predicate_eval *eval,
                                     tf_obj *pred, bool allow_bool,
                                     tf_obj **inputs, size_t input_len,
                                     bool *ready, bool *pred_val) {
    *ready = false;

    if (eval->phase == TF_PRED_IDLE) {
        if (pred->type == TF_OBJ_TYPE_BOOL) {
            if (!allow_bool || input_len != 0) return TF_ERR;
            *pred_val = pred->b;
            *ready = true;
            return TF_OK;
        }

        if (!tf_obj_is_callable(pred)) return TF_ERR;

        save_stack_copy(ctx, &eval->saved_stack, &eval->saved_len);
        for (size_t i = 0; i < input_len; i++) {
            tf_stack_push(ctx, inputs[i]);
            tf_obj_retain(inputs[i]);
        }

        eval->phase = TF_PRED_AWAITING;
        return tf_vm_call_callable(ctx, pred);
    }

    tf_obj *bool_res = tf_stack_pop_type(ctx, TF_OBJ_TYPE_BOOL);
    if (!bool_res) {
        restore_stack_owned(ctx, eval->saved_stack, eval->saved_len);
        free(eval->saved_stack);
        predicate_eval_clear(eval);
        return TF_ERR;
    }

    *pred_val = bool_res->b;
    tf_obj_release(bool_res);

    restore_stack_owned(ctx, eval->saved_stack, eval->saved_len);
    free(eval->saved_stack);
    predicate_eval_clear(eval);
    *ready = true;
    return TF_OK;
}

static void predicate_eval_cleanup(tf_ctx *ctx, predicate_eval *eval) {
    if (eval->phase == TF_PRED_AWAITING && eval->saved_stack) {
        restore_stack_owned(ctx, eval->saved_stack, eval->saved_len);
        free(eval->saved_stack);
        predicate_eval_clear(eval);
    }
}

typedef enum { TF_IF_COND, TF_IF_BODY } if_stage;

typedef struct {
    tf_obj *cond;
    tf_obj *then_b;
    tf_obj *else_b;
    bool has_else;
    if_stage stage;
    predicate_eval pred_eval;
} if_state;

static tf_ret if_step(tf_ctx *ctx, void *state, bool *done) {
    if_state *s = state;
    if (s->stage == TF_IF_COND) {
        bool ready = false;
        bool cond_val = false;
        tf_ret res = predicate_eval_step(ctx, &s->pred_eval, s->cond, true,
                                            NULL, 0, &ready, &cond_val);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        tf_obj *body = cond_val ? s->then_b : s->else_b;
        if (!body) {
            *done = true;
            return TF_OK;
        }
        s->stage = TF_IF_BODY;
        *done = false;
        return tf_vm_call_callable(ctx, body);
    }

    *done = true;
    return TF_OK;
}

static void if_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    if_state *s = state;
    predicate_eval_cleanup(ctx, &s->pred_eval);
    tf_obj_release(s->cond);
    tf_obj_release(s->then_b);
    if (s->has_else) tf_obj_release(s->else_b);
    free(s);
}

typedef enum { TF_WHILE_COND, TF_WHILE_BODY } while_stage;

typedef struct {
    tf_obj *cond;
    tf_obj *body;
    while_stage stage;
    predicate_eval pred_eval;
} while_state;

static tf_ret while_step(tf_ctx *ctx, void *state, bool *done) {
    while_state *s = state;
    if (s->stage == TF_WHILE_COND) {
        bool ready = false;
        bool continue_loop = false;
        tf_ret res = predicate_eval_step(ctx, &s->pred_eval, s->cond, false,
                                            NULL, 0, &ready, &continue_loop);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }
        if (!continue_loop) {
            *done = true;
            return TF_OK;
        }
        s->stage = TF_WHILE_BODY;
        *done = false;
        return tf_vm_call_callable(ctx, s->body);
    }

    s->stage = TF_WHILE_COND;
    *done = false;
    return TF_OK;
}

static void while_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    while_state *s = state;
    predicate_eval_cleanup(ctx, &s->pred_eval);
    tf_obj_release(s->cond);
    tf_obj_release(s->body);
    free(s);
}

typedef enum { TF_COND_PRED, TF_COND_BODY } cond_stage;

typedef struct {
    tf_obj *clauses;
    size_t index;
    cond_stage stage;
    predicate_eval pred_eval;
} cond_state;

static tf_ret cond_step(tf_ctx *ctx, void *state, bool *done) {
    cond_state *s = state;
    if (s->stage == TF_COND_BODY) {
        *done = true;
        return TF_OK;
    }

    while (s->index < s->clauses->vector.len) {
        tf_obj *clause = s->clauses->vector.elem[s->index];
        if (clause->type != TF_OBJ_TYPE_VECTOR || clause->vector.len != 2) {
            return TF_ERR;
        }

        tf_obj *pred = clause->vector.elem[0];
        tf_obj *body = clause->vector.elem[1];
        if (!tf_obj_is_callable(body)) return TF_ERR;

        bool ready = false;
        bool cond_val = false;
        tf_ret res = predicate_eval_step(ctx, &s->pred_eval, pred, true,
                                            NULL, 0, &ready, &cond_val);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        if (cond_val) {
            s->stage = TF_COND_BODY;
            *done = false;
            return tf_vm_call_callable(ctx, body);
        }

        s->index++;
    }

    *done = true;
    return TF_OK;
}

static void cond_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    cond_state *s = state;
    predicate_eval_cleanup(ctx, &s->pred_eval);
    tf_obj_release(s->clauses);
    free(s);
}

typedef struct {
    tf_obj *pred;
    tf_obj *seq;
    size_t index;
    tf_obj *current;
    bool string_result;
    bool list_result;
    tf_obj *vector_result;
    bytebuf str_result;
    predicate_eval pred_eval;
} filter_state;

static tf_ret filter_step(tf_ctx *ctx, void *state, bool *done) {
    filter_state *s = state;
    size_t len = sequence_len(s->seq);

    while (s->index < len || s->current) {
        if (!s->current) {
            s->current = sequence_item_owned(s->seq, s->index++);
        }

        tf_obj *inputs[] = { s->current };
        bool ready = false;
        bool keep = false;
        tf_ret res = predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, inputs, 1, &ready, &keep);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        if (keep) {
            if (s->string_result) {
                bytebuf_append(&s->str_result, s->current->str.ptr[0]);
                tf_obj_release(s->current);
            } else {
                tf_vector_push(s->vector_result, s->current);
            }
        } else {
            tf_obj_release(s->current);
        }
        s->current = NULL;
    }

    if (s->string_result) {
        tf_stack_push(ctx, bytebuf_to_string(&s->str_result));
        free(s->str_result.ptr);
        s->str_result.ptr = NULL;
    } else {
        tf_stack_push(ctx,
                      finish_vector_family(&s->vector_result,
                                           s->list_result));
    }
    *done = true;
    return TF_OK;
}

static void filter_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    filter_state *s = state;
    predicate_eval_cleanup(ctx, &s->pred_eval);
    tf_obj_release(s->pred);
    tf_obj_release(s->seq);
    if (s->current) tf_obj_release(s->current);
    if (s->string_result) {
        free(s->str_result.ptr);
    } else if (s->vector_result) {
        tf_obj_release(s->vector_result);
    }
    free(s);
}

typedef enum { TF_QUANT_SOME, TF_QUANT_ALL } quantifier_kind;

typedef struct {
    tf_obj *pred;
    tf_obj *seq;
    size_t index;
    tf_obj *current;
    quantifier_kind kind;
    predicate_eval pred_eval;
} quantifier_state;

static tf_ret quantifier_step(tf_ctx *ctx, void *state, bool *done) {
    quantifier_state *s = state;
    size_t len = sequence_len(s->seq);

    while (s->index < len || s->current) {
        if (!s->current) {
            s->current = sequence_item_owned(s->seq, s->index++);
        }

        tf_obj *inputs[] = { s->current };
        bool ready = false;
        bool pred_val = false;
        tf_ret res = predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, inputs, 1, &ready,
                                            &pred_val);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        tf_obj_release(s->current);
        s->current = NULL;

        if (s->kind == TF_QUANT_SOME && pred_val) {
            tf_stack_push(ctx, tf_obj_new_bool(true));
            *done = true;
            return TF_OK;
        }
        if (s->kind == TF_QUANT_ALL && !pred_val) {
            tf_stack_push(ctx, tf_obj_new_bool(false));
            *done = true;
            return TF_OK;
        }
    }

    tf_stack_push(ctx, tf_obj_new_bool(s->kind == TF_QUANT_ALL));
    *done = true;
    return TF_OK;
}

static void quantifier_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    quantifier_state *s = state;
    predicate_eval_cleanup(ctx, &s->pred_eval);
    tf_obj_release(s->pred);
    tf_obj_release(s->seq);
    if (s->current) tf_obj_release(s->current);
    free(s);
}

typedef struct {
    tf_obj *pred;
    tf_obj *seq;
    size_t index;
    tf_obj *current;
    bool string_result;
    bool list_result;
    tf_obj *true_list;
    tf_obj *false_list;
    bytebuf true_str;
    bytebuf false_str;
    predicate_eval pred_eval;
} split_state;

static tf_ret split_step(tf_ctx *ctx, void *state, bool *done) {
    split_state *s = state;
    size_t len = sequence_len(s->seq);

    while (s->index < len || s->current) {
        if (!s->current) {
            s->current = sequence_item_owned(s->seq, s->index++);
        }

        tf_obj *inputs[] = { s->current };
        bool ready = false;
        bool pred_val = false;
        tf_ret res = predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, inputs, 1, &ready,
                                            &pred_val);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        if (s->string_result) {
            bytebuf_append(pred_val ? &s->true_str : &s->false_str,
                              s->current->str.ptr[0]);
            tf_obj_release(s->current);
        } else {
            tf_vector_push(pred_val ? s->true_list : s->false_list, s->current);
        }
        s->current = NULL;
    }

    if (s->string_result) {
        tf_stack_push(ctx, bytebuf_to_string(&s->true_str));
        tf_stack_push(ctx, bytebuf_to_string(&s->false_str));
        free(s->true_str.ptr);
        free(s->false_str.ptr);
        s->true_str.ptr = NULL;
        s->false_str.ptr = NULL;
    } else {
        tf_stack_push(ctx,
                      finish_vector_family(&s->true_list, s->list_result));
        tf_stack_push(ctx,
                      finish_vector_family(&s->false_list, s->list_result));
    }
    *done = true;
    return TF_OK;
}

static void split_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    split_state *s = state;
    predicate_eval_cleanup(ctx, &s->pred_eval);
    tf_obj_release(s->pred);
    tf_obj_release(s->seq);
    if (s->current) tf_obj_release(s->current);
    if (s->string_result) {
        free(s->true_str.ptr);
        free(s->false_str.ptr);
    } else {
        if (s->true_list) tf_obj_release(s->true_list);
        if (s->false_list) tf_obj_release(s->false_list);
    }
    free(s);
}

typedef struct {
    tf_obj *pred;
    tf_obj *l1;
    tf_obj *l2;
    size_t i1;
    size_t i2;
    tf_obj *o1;
    tf_obj *o2;
    bool string_result;
    bool list_result;
    tf_obj *vector_result;
    bytebuf str_result;
    predicate_eval pred_eval;
} merge_state;

static void merge_take_left(merge_state *s) {
    if (s->string_result) {
        bytebuf_append(&s->str_result, s->o1->str.ptr[0]);
        tf_obj_release(s->o1);
    } else {
        tf_vector_push(s->vector_result, s->o1);
    }
    tf_obj_release(s->o2);
    s->o1 = NULL;
    s->o2 = NULL;
    s->i1++;
}

static void merge_take_right(merge_state *s) {
    if (s->string_result) {
        bytebuf_append(&s->str_result, s->o2->str.ptr[0]);
        tf_obj_release(s->o2);
    } else {
        tf_vector_push(s->vector_result, s->o2);
    }
    tf_obj_release(s->o1);
    s->o1 = NULL;
    s->o2 = NULL;
    s->i2++;
}

static tf_ret merge_step(tf_ctx *ctx, void *state, bool *done) {
    merge_state *s = state;
    size_t l1_len = sequence_len(s->l1);
    size_t l2_len = sequence_len(s->l2);

    while ((s->i1 < l1_len && s->i2 < l2_len) || s->o1 || s->o2) {
        if (!s->o1 && !s->o2) {
            s->o1 = sequence_item_owned(s->l1, s->i1);
            s->o2 = sequence_item_owned(s->l2, s->i2);
        }

        tf_obj *inputs[] = { s->o1, s->o2 };
        bool ready = false;
        bool take_left = false;
        tf_ret res = predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, inputs, 2, &ready,
                                            &take_left);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        if (take_left) {
            merge_take_left(s);
        } else {
            merge_take_right(s);
        }
    }

    while (s->i1 < l1_len) {
        if (s->string_result) {
            bytebuf_append(&s->str_result, s->l1->str.ptr[s->i1]);
        } else {
            tf_vector_push(s->vector_result,
                           sequence_item_owned(s->l1, s->i1));
        }
        s->i1++;
    }
    while (s->i2 < l2_len) {
        if (s->string_result) {
            bytebuf_append(&s->str_result, s->l2->str.ptr[s->i2]);
        } else {
            tf_vector_push(s->vector_result,
                           sequence_item_owned(s->l2, s->i2));
        }
        s->i2++;
    }

    if (s->string_result) {
        tf_stack_push(ctx, bytebuf_to_string(&s->str_result));
        free(s->str_result.ptr);
        s->str_result.ptr = NULL;
    } else {
        tf_stack_push(ctx,
                      finish_vector_family(&s->vector_result,
                                           s->list_result));
    }
    *done = true;
    return TF_OK;
}

static void merge_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    merge_state *s = state;
    predicate_eval_cleanup(ctx, &s->pred_eval);
    tf_obj_release(s->pred);
    tf_obj_release(s->l1);
    tf_obj_release(s->l2);
    if (s->o1) tf_obj_release(s->o1);
    if (s->o2) tf_obj_release(s->o2);
    if (s->string_result) {
        free(s->str_result.ptr);
    } else if (s->vector_result) {
        tf_obj_release(s->vector_result);
    }
    free(s);
}

typedef enum {
    TF_LINREC_PRED,
    TF_LINREC_THEN,
    TF_LINREC_REC1,
    TF_LINREC_CONT
} linrec_stage;

typedef struct {
    tf_obj *pred;
    tf_obj *then_b;
    tf_obj *rec1;
    tf_obj *rec2;
    linrec_stage stage;
    predicate_eval pred_eval;
} linrec_state;

static tf_ret linrec_step(tf_ctx *ctx, void *state, bool *done) {
    linrec_state *s = state;
    if (s->stage == TF_LINREC_PRED) {
        bool ready = false;
        bool is_done = false;
        tf_ret res = predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, NULL, 0, &ready, &is_done);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        s->stage = is_done ? TF_LINREC_THEN : TF_LINREC_REC1;
        *done = false;
        return tf_vm_call_callable(ctx, is_done ? s->then_b : s->rec1);
    }

    if (s->stage == TF_LINREC_THEN || s->stage == TF_LINREC_CONT) {
        *done = true;
        return TF_OK;
    }

    tf_obj *cont = tf_obj_new_vector();
    tf_obj *linrec_sym = tf_obj_new_symbol("linrec", 6);
    tf_obj *exec_sym = tf_obj_new_symbol("exec", 4);

    push_retained(cont, s->pred);
    push_retained(cont, s->then_b);
    push_retained(cont, s->rec1);
    push_retained(cont, s->rec2);
    tf_vector_push(cont, linrec_sym);
    push_retained(cont, s->rec2);
    tf_vector_push(cont, exec_sym);

    tf_frame_push_program(ctx, cont);
    tf_obj_release(cont);
    s->stage = TF_LINREC_CONT;
    *done = false;
    return TF_OK;
}

static void linrec_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    linrec_state *s = state;
    predicate_eval_cleanup(ctx, &s->pred_eval);
    tf_obj_release(s->pred);
    tf_obj_release(s->then_b);
    tf_obj_release(s->rec1);
    tf_obj_release(s->rec2);
    free(s);
}

typedef enum {
    TF_BINREC_PRED,
    TF_BINREC_THEN,
    TF_BINREC_REC1,
    TF_BINREC_CONT
} binrec_stage;

typedef struct {
    tf_obj *pred;
    tf_obj *then_b;
    tf_obj *rec1;
    tf_obj *rec2;
    binrec_stage stage;
    predicate_eval pred_eval;
} binrec_state;

static tf_ret binrec_step(tf_ctx *ctx, void *state, bool *done) {
    binrec_state *s = state;
    if (s->stage == TF_BINREC_PRED) {
        bool ready = false;
        bool is_done = false;
        tf_ret res = predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, NULL, 0, &ready, &is_done);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        s->stage = is_done ? TF_BINREC_THEN : TF_BINREC_REC1;
        *done = false;
        return tf_vm_call_callable(ctx, is_done ? s->then_b : s->rec1);
    }

    if (s->stage == TF_BINREC_THEN || s->stage == TF_BINREC_CONT) {
        *done = true;
        return TF_OK;
    }

    tf_obj *rec_call = tf_obj_new_vector();
    tf_obj *binrec_sym = tf_obj_new_symbol("binrec", 6);
    push_retained(rec_call, s->pred);
    push_retained(rec_call, s->then_b);
    push_retained(rec_call, s->rec1);
    push_retained(rec_call, s->rec2);
    tf_vector_push(rec_call, binrec_sym);

    tf_obj *cont = tf_obj_new_vector();
    tf_obj *dip_sym = tf_obj_new_symbol("dip", 3);
    tf_obj *exec_rec_sym = tf_obj_new_symbol("exec", 4);
    tf_obj *exec_combine_sym = tf_obj_new_symbol("exec", 4);

    push_retained(cont, rec_call);
    tf_vector_push(cont, dip_sym);
    push_retained(cont, rec_call);
    tf_vector_push(cont, exec_rec_sym);
    push_retained(cont, s->rec2);
    tf_vector_push(cont, exec_combine_sym);

    tf_frame_push_program(ctx, cont);
    tf_obj_release(cont);
    tf_obj_release(rec_call);
    s->stage = TF_BINREC_CONT;
    *done = false;
    return TF_OK;
}

static void binrec_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    binrec_state *s = state;
    predicate_eval_cleanup(ctx, &s->pred_eval);
    tf_obj_release(s->pred);
    tf_obj_release(s->then_b);
    tf_obj_release(s->rec1);
    tf_obj_release(s->rec2);
    free(s);
}

typedef enum {
    TF_GENREC_PRED,
    TF_GENREC_THEN,
    TF_GENREC_BEFORE,
    TF_GENREC_CONT
} genrec_stage;

typedef struct {
    tf_obj *pred;
    tf_obj *then_b;
    tf_obj *before;
    tf_obj *after;
    genrec_stage stage;
    predicate_eval pred_eval;
} genrec_state;

static tf_ret genrec_step(tf_ctx *ctx, void *state, bool *done) {
    genrec_state *s = state;
    if (s->stage == TF_GENREC_PRED) {
        bool ready = false;
        bool is_done = false;
        tf_ret res = predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, NULL, 0, &ready, &is_done);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        s->stage = is_done ? TF_GENREC_THEN : TF_GENREC_BEFORE;
        *done = false;
        return tf_vm_call_callable(ctx, is_done ? s->then_b : s->before);
    }

    if (s->stage == TF_GENREC_THEN || s->stage == TF_GENREC_CONT) {
        *done = true;
        return TF_OK;
    }

    tf_obj *cont = tf_obj_new_vector();
    tf_obj *genrec_sym = tf_obj_new_symbol("genrec", 6);
    tf_obj *exec_sym = tf_obj_new_symbol("exec", 4);

    push_retained(cont, s->pred);
    push_retained(cont, s->then_b);
    push_retained(cont, s->before);
    push_retained(cont, s->after);
    tf_vector_push(cont, genrec_sym);
    push_retained(cont, s->after);
    tf_vector_push(cont, exec_sym);

    tf_frame_push_program(ctx, cont);
    tf_obj_release(cont);
    s->stage = TF_GENREC_CONT;
    *done = false;
    return TF_OK;
}

static void genrec_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    genrec_state *s = state;
    predicate_eval_cleanup(ctx, &s->pred_eval);
    tf_obj_release(s->pred);
    tf_obj_release(s->then_b);
    tf_obj_release(s->before);
    tf_obj_release(s->after);
    free(s);
}

typedef enum {
    TF_TREEREC_START,
    TF_TREEREC_LEAF_BODY,
    TF_TREEREC_CHILD_LOOP,
    TF_TREEREC_CHILD_DONE,
    TF_TREEREC_NODE_BODY
} treerec_stage;

typedef struct {
    tf_obj *tree;
    tf_obj *leaf;
    tf_obj *node;
    tf_obj *mapped;
    size_t index;
    tf_obj **saved_stack;
    size_t saved_len;
    treerec_stage stage;
} treerec_state;

static void treerec_push_owned(tf_ctx *ctx, tf_obj *tree, tf_obj *leaf,
                                  tf_obj *node);

static tf_ret treerec_finish_single(tf_ctx *ctx, treerec_state *s,
                                       bool *done) {
    if (tf_stack_len(ctx) != s->saved_len + 1) return TF_ERR;
    tf_obj *result = tf_stack_pop(ctx);
    restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    tf_stack_push(ctx, result);
    *done = true;
    return TF_OK;
}

static tf_ret treerec_step(tf_ctx *ctx, void *state, bool *done) {
    treerec_state *s = state;

    if (s->stage == TF_TREEREC_START) {
        if (s->tree->type != TF_OBJ_TYPE_VECTOR) {
            tf_stack_push(ctx, s->tree);
            tf_obj_retain(s->tree);
            s->stage = TF_TREEREC_LEAF_BODY;
            *done = false;
            return tf_vm_call_callable(ctx, s->leaf);
        }

        s->mapped = tf_obj_new_vector();
        s->index = 0;
        s->stage = TF_TREEREC_CHILD_LOOP;
    }

    if (s->stage == TF_TREEREC_LEAF_BODY) {
        return treerec_finish_single(ctx, s, done);
    }

    if (s->stage == TF_TREEREC_CHILD_DONE) {
        if (tf_stack_len(ctx) != s->saved_len + 1) return TF_ERR;
        tf_obj *child_result = tf_stack_pop(ctx);
        tf_vector_push(s->mapped, child_result);
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
        s->index++;
        s->stage = TF_TREEREC_CHILD_LOOP;
        *done = false;
        return TF_OK;
    }

    if (s->stage == TF_TREEREC_CHILD_LOOP) {
        if (s->index < s->tree->vector.len) {
            tf_obj *child = s->tree->vector.elem[s->index];
            tf_obj_retain(child);
            tf_obj_retain(s->leaf);
            tf_obj_retain(s->node);
            treerec_push_owned(ctx, child, s->leaf, s->node);
            s->stage = TF_TREEREC_CHILD_DONE;
            *done = false;
            return TF_OK;
        }

        tf_stack_push(ctx, s->mapped);
        tf_obj_retain(s->mapped);
        s->stage = TF_TREEREC_NODE_BODY;
        *done = false;
        return tf_vm_call_callable(ctx, s->node);
    }

    return treerec_finish_single(ctx, s, done);
}

static void treerec_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    treerec_state *s = state;
    if (status != TF_OK) {
        restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    release_stack_copy(s->saved_stack, s->saved_len);
    tf_obj_release(s->tree);
    tf_obj_release(s->leaf);
    tf_obj_release(s->node);
    if (s->mapped) tf_obj_release(s->mapped);
    free(s);
}

static void treerec_push_owned(tf_ctx *ctx, tf_obj *tree, tf_obj *leaf,
                                  tf_obj *node) {
    treerec_state *state = tf_xmalloc(sizeof(*state));
    state->tree = tree;
    state->leaf = leaf;
    state->node = node;
    state->mapped = NULL;
    state->index = 0;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->stage = TF_TREEREC_START;
    save_stack_copy(ctx, &state->saved_stack, &state->saved_len);
    tf_frame_push_native(ctx, treerec_step, treerec_cleanup, state);
}

tf_ret tf_error(tf_ctx *ctx) {
    if (tf_stack_len(ctx) > 0) {
        tf_obj *msg = tf_stack_peek(ctx, 0);
        if (msg->type == TF_OBJ_TYPE_STR) {
            msg = tf_stack_pop(ctx);
            if (ctx->error_suppression_depth == 0) {
                tf_ctx_program_errorf(ctx, "%s\n", msg->str.ptr);
                ctx->error_reported = true;
            }
            tf_obj_release(msg);
            return TF_ERR;
        }
    }

    if (ctx->error_suppression_depth == 0) {
        tf_ctx_program_errorf(ctx, "error\n");
        ctx->error_reported = true;
    }
    return TF_ERR;
}

tf_ret tf_try(tf_ctx *ctx) {
    if (!tf_ctx_require_callable(ctx, 1) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *handler = tf_stack_peek(ctx, 0);
    tf_obj *body = tf_stack_peek(ctx, 1);

    handler = tf_stack_pop(ctx);
    body = tf_stack_pop(ctx);

    try_state *state = tf_xmalloc(sizeof(*state));
    state->body = body;
    state->handler = handler;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->stage = TF_TRY_START;
    state->suppressing_errors = false;
    save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    tf_frame_push_native_handler(ctx, try_step, try_cleanup, try_error,
                              state);
    return TF_OK;
}

tf_ret tf_if(tf_ctx *ctx) {
    if (!require_condition(ctx, 1) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *body = tf_stack_peek(ctx, 0);
    tf_obj *cond = tf_stack_peek(ctx, 1);

    body = tf_stack_pop(ctx);
    cond = tf_stack_pop(ctx);

    if_state *state = tf_xmalloc(sizeof(*state));
    state->cond = cond;
    state->then_b = body;
    state->else_b = NULL;
    state->has_else = false;
    state->stage = TF_IF_COND;
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, if_step, if_cleanup, state);
    return TF_OK;
}

tf_ret tf_infra(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_VECTOR) ||
        !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *body = tf_stack_peek(ctx, 0);
    tf_obj *data = tf_stack_peek(ctx, 1);

    body = tf_stack_pop(ctx);
    data = tf_stack_pop(ctx);

    infra_state *state = tf_xmalloc(sizeof(*state));
    state->body = body;
    state->data = data;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->scheduled = false;
    state->result = NULL;
    save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    clear_stack(ctx);
    for (size_t i = 0; i < data->vector.len; i++) {
        tf_stack_push(ctx, data->vector.elem[i]);
        tf_obj_retain(data->vector.elem[i]);
    }

    tf_frame_push_native(ctx, infra_step, infra_cleanup, state);
    return TF_OK;
}

tf_ret tf_cond(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_VECTOR)) return TF_ERR;
    tf_obj *clauses = tf_stack_pop_type(ctx, TF_OBJ_TYPE_VECTOR);

    cond_state *state = tf_xmalloc(sizeof(*state));
    state->clauses = clauses;
    state->index = 0;
    state->stage = TF_COND_PRED;
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, cond_step, cond_cleanup, state);
    return TF_OK;
}

static tf_ret cleave_or_construct(tf_ctx *ctx, bool construct_result) {
    if (!tf_ctx_require_stack(ctx, 2) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_VECTOR)) {
        return TF_ERR;
    }
    tf_obj *branches = tf_stack_peek(ctx, 0);
    if (!require_callable_vector(ctx, branches,
                                 construct_result ? "construct" : "cleave")) {
        return TF_ERR;
    }

    branches = tf_stack_pop(ctx);
    tf_obj *value = tf_stack_pop(ctx);

    cleave_state *state = tf_xmalloc(sizeof(*state));
    state->branches = branches;
    state->value = value;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->outputs = tf_obj_new_vector();
    state->index = 0;
    state->awaiting_branch = false;
    state->construct_result = construct_result;
    save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    tf_frame_push_native(ctx, cleave_step, cleave_cleanup, state);
    return TF_OK;
}

tf_ret tf_cleave(tf_ctx *ctx) {
    return cleave_or_construct(ctx, false);
}

tf_ret tf_construct(tf_ctx *ctx) {
    return cleave_or_construct(ctx, true);
}

tf_ret tf_ifelse(tf_ctx *ctx) {
    if (!require_condition(ctx, 2) || !tf_ctx_require_callable(ctx, 1) ||
        !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *else_b = tf_stack_peek(ctx, 0);
    tf_obj *then_b = tf_stack_peek(ctx, 1);
    tf_obj *cond = tf_stack_peek(ctx, 2);

    else_b = tf_stack_pop(ctx);
    then_b = tf_stack_pop(ctx);
    cond = tf_stack_pop(ctx);

    if_state *state = tf_xmalloc(sizeof(*state));
    state->cond = cond;
    state->then_b = then_b;
    state->else_b = else_b;
    state->has_else = true;
    state->stage = TF_IF_COND;
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, if_step, if_cleanup, state);
    return TF_OK;
}

tf_ret tf_while(tf_ctx *ctx) {
    if (!tf_ctx_require_callable(ctx, 1) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *body = tf_stack_peek(ctx, 0);
    tf_obj *cond = tf_stack_peek(ctx, 1);

    body = tf_stack_pop(ctx);
    cond = tf_stack_pop(ctx);

    while_state *state = tf_xmalloc(sizeof(*state));
    state->cond = cond;
    state->body = body;
    state->stage = TF_WHILE_COND;
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, while_step, while_cleanup, state);
    return TF_OK;
}

tf_ret tf_dip(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *body = tf_stack_peek(ctx, 0);

    body = tf_stack_pop(ctx);
    tf_obj *saved = tf_stack_pop(ctx);

    dip_state *state = tf_xmalloc(sizeof(*state));
    state->body = body;
    state->saved = saved;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->scheduled = false;
    save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    tf_frame_push_native(ctx, dip_step, dip_cleanup, state);
    return TF_OK;
}

tf_ret tf_keep(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *body = tf_stack_peek(ctx, 0);

    body = tf_stack_pop(ctx);
    tf_obj *saved = tf_stack_pop(ctx);

    keep_state *state = tf_xmalloc(sizeof(*state));
    state->body = body;
    state->saved = saved;
    state->saved_stack = NULL;
    state->base_len = 0;
    state->scheduled = false;
    save_stack_copy(ctx, &state->saved_stack, &state->base_len);

    tf_frame_push_native(ctx, keep_step, keep_cleanup, state);
    return TF_OK;
}

tf_ret tf_bi(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 3) || !tf_ctx_require_callable(ctx, 1) ||
        !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *right = tf_stack_peek(ctx, 0);
    tf_obj *left = tf_stack_peek(ctx, 1);

    right = tf_stack_pop(ctx);
    left = tf_stack_pop(ctx);
    tf_obj *saved = tf_stack_pop(ctx);

    bi_state *state = tf_xmalloc(sizeof(*state));
    state->left = left;
    state->right = right;
    state->saved = saved;
    state->saved_stack = NULL;
    state->base_len = 0;
    state->stage = TF_BI_RUN_LEFT;
    state->left_outputs = NULL;
    state->left_out_len = 0;
    save_stack_copy(ctx, &state->saved_stack, &state->base_len);

    tf_frame_push_native(ctx, bi_step, bi_cleanup, state);
    return TF_OK;
}

tf_ret tf_linrec(tf_ctx *ctx) {
    if (!tf_ctx_require_callable(ctx, 3) || !tf_ctx_require_callable(ctx, 2) ||
        !tf_ctx_require_callable(ctx, 1) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *rec2 = tf_stack_peek(ctx, 0);
    tf_obj *rec1 = tf_stack_peek(ctx, 1);
    tf_obj *then_b = tf_stack_peek(ctx, 2);
    tf_obj *pred = tf_stack_peek(ctx, 3);

    rec2 = tf_stack_pop(ctx);
    rec1 = tf_stack_pop(ctx);
    then_b = tf_stack_pop(ctx);
    pred = tf_stack_pop(ctx);

    linrec_state *state = tf_xmalloc(sizeof(*state));
    state->pred = pred;
    state->then_b = then_b;
    state->rec1 = rec1;
    state->rec2 = rec2;
    state->stage = TF_LINREC_PRED;
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, linrec_step, linrec_cleanup, state);
    return TF_OK;
}

tf_ret tf_binrec(tf_ctx *ctx) {
    if (!tf_ctx_require_callable(ctx, 3) || !tf_ctx_require_callable(ctx, 2) ||
        !tf_ctx_require_callable(ctx, 1) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *rec2 = tf_stack_peek(ctx, 0);
    tf_obj *rec1 = tf_stack_peek(ctx, 1);
    tf_obj *then_b = tf_stack_peek(ctx, 2);
    tf_obj *pred = tf_stack_peek(ctx, 3);

    rec2 = tf_stack_pop(ctx);
    rec1 = tf_stack_pop(ctx);
    then_b = tf_stack_pop(ctx);
    pred = tf_stack_pop(ctx);

    binrec_state *state = tf_xmalloc(sizeof(*state));
    state->pred = pred;
    state->then_b = then_b;
    state->rec1 = rec1;
    state->rec2 = rec2;
    state->stage = TF_BINREC_PRED;
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, binrec_step, binrec_cleanup, state);
    return TF_OK;
}

static tf_ret pop_rec_parts(tf_ctx *ctx, tf_obj **pred, tf_obj **then_b,
                               tf_obj **before, tf_obj **after) {
    if (tf_stack_len(ctx) >= 4 && tf_obj_is_callable(tf_stack_peek(ctx, 0)) &&
        tf_obj_is_callable(tf_stack_peek(ctx, 1)) &&
        tf_obj_is_callable(tf_stack_peek(ctx, 2)) &&
        tf_obj_is_callable(tf_stack_peek(ctx, 3))) {
        *after = tf_stack_pop(ctx);
        *before = tf_stack_pop(ctx);
        *then_b = tf_stack_pop(ctx);
        *pred = tf_stack_pop(ctx);
        return TF_OK;
    }

    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *parts = tf_stack_peek(ctx, 0);
    if (parts->type != TF_OBJ_TYPE_VECTOR || parts->vector.len != 4) {
        tf_ctx_runtime_errorf(ctx,
                              "'%s' expected four callables or a vector of four callables\n",
                              ctx->current_word);
        return TF_ERR;
    }
    for (size_t i = 0; i < 4; i++) {
        if (!tf_obj_is_callable(parts->vector.elem[i])) {
            tf_ctx_runtime_errorf(
                ctx, "'%s' expected recursion part %zu to be callable, found %s\n",
                ctx->current_word, i, tf_obj_type_name(parts->vector.elem[i]));
            return TF_ERR;
        }
    }

    parts = tf_stack_pop(ctx);
    *pred = parts->vector.elem[0];
    *then_b = parts->vector.elem[1];
    *before = parts->vector.elem[2];
    *after = parts->vector.elem[3];
    tf_obj_retain(*pred);
    tf_obj_retain(*then_b);
    tf_obj_retain(*before);
    tf_obj_retain(*after);
    tf_obj_release(parts);
    return TF_OK;
}

tf_ret tf_genrec(tf_ctx *ctx) {
    tf_obj *pred = NULL;
    tf_obj *then_b = NULL;
    tf_obj *before = NULL;
    tf_obj *after = NULL;
    tf_ret res = pop_rec_parts(ctx, &pred, &then_b, &before, &after);
    if (res != TF_OK) return res;

    genrec_state *state = tf_xmalloc(sizeof(*state));
    state->pred = pred;
    state->then_b = then_b;
    state->before = before;
    state->after = after;
    state->stage = TF_GENREC_PRED;
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, genrec_step, genrec_cleanup, state);
    return TF_OK;
}

tf_ret tf_treerec(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 3) || !tf_ctx_require_callable(ctx, 1) ||
        !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *node = tf_stack_peek(ctx, 0);
    tf_obj *leaf = tf_stack_peek(ctx, 1);

    node = tf_stack_pop(ctx);
    leaf = tf_stack_pop(ctx);
    tf_obj *tree = tf_stack_pop(ctx);

    treerec_push_owned(ctx, tree, leaf, node);
    return TF_OK;
}

tf_ret tf_replicate(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_INT) ||
        !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *body = tf_stack_peek(ctx, 0);
    tf_obj *n_obj = tf_stack_peek(ctx, 1);
    body = tf_stack_pop(ctx);
    n_obj = tf_stack_pop(ctx);

    int n = n_obj->i;
    tf_obj_release(n_obj);

    if (n < 0) {
        tf_obj_release(body);
        tf_ctx_runtime_errorf(ctx, "'%s' count must be non-negative\n",
                              ctx->current_word);
        return TF_ERR;
    }

    replicate_state *state = tf_xmalloc(sizeof(*state));
    state->body = body;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->remaining = n;
    state->awaiting_body = false;
    state->result = tf_obj_new_vector();
    save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    tf_frame_push_native(ctx, replicate_step, replicate_cleanup, state);
    return TF_OK;
}

tf_ret tf_times(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_INT) ||
        !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *body = tf_stack_peek(ctx, 0);
    tf_obj *n_obj = tf_stack_peek(ctx, 1);

    body = tf_stack_pop(ctx);
    n_obj = tf_stack_pop(ctx);

    int n = n_obj->i;
    if (n < 0) {
        tf_obj_release(body);
        tf_obj_release(n_obj);
        tf_ctx_runtime_errorf(ctx, "'%s' count must be non-negative\n",
                              ctx->current_word);
        return TF_ERR;
    }

    tf_obj_release(n_obj);
    if (n == 0) {
        tf_obj_release(body);
        return TF_OK;
    }

    times_state *state = tf_xmalloc(sizeof(*state));
    state->body = body;
    state->remaining = n;
    tf_frame_push_native(ctx, times_step, times_cleanup, state);
    return TF_OK;
}

tf_ret tf_each(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 1) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *body = tf_stack_peek(ctx, 0);
    tf_obj *data = tf_stack_peek(ctx, 1);

    body = tf_stack_pop(ctx);
    data = tf_stack_pop(ctx);

    each_state *state = tf_xmalloc(sizeof(*state));
    state->body = body;
    state->data = data;
    state->index = 0;
    tf_frame_push_native(ctx, each_step, each_cleanup, state);
    return TF_OK;
}

tf_ret tf_map(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 1) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *body = tf_stack_peek(ctx, 0);
    tf_obj *data = tf_stack_peek(ctx, 1);

    body = tf_stack_pop(ctx);
    data = tf_stack_pop(ctx);

    map_state *state = tf_xmalloc(sizeof(*state));
    state->body = body;
    state->data = data;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->index = 0;
    state->awaiting_body = false;
    state->string_result = data->type == TF_OBJ_TYPE_STR;
    state->list_result = data->type == TF_OBJ_TYPE_LIST;
    state->vector_result = state->string_result ? NULL : tf_obj_new_vector();
    state->str_result.ptr = NULL;
    state->str_result.len = 0;
    state->str_result.cap = 0;
    if (state->string_result) bytebuf_init(&state->str_result, data->str.len);
    save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    tf_frame_push_native(ctx, map_step, map_cleanup, state);
    return TF_OK;
}

tf_ret tf_fold(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 3) || !tf_ctx_require_sequence(ctx, 1) ||
        !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *body = tf_stack_peek(ctx, 0);
    tf_obj *data = tf_stack_peek(ctx, 1);

    body = tf_stack_pop(ctx);
    data = tf_stack_pop(ctx);
    tf_obj *acc = tf_stack_pop(ctx);

    fold_state *state = tf_xmalloc(sizeof(*state));
    state->body = body;
    state->data = data;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->index = 0;
    state->awaiting_body = false;
    save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    tf_stack_push(ctx, acc);
    tf_frame_push_native(ctx, fold_step, fold_cleanup, state);
    return TF_OK;
}

tf_ret tf_split(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *pred = tf_stack_peek(ctx, 0);
    tf_obj *seq = tf_stack_peek(ctx, 1);

    if (pred->type == TF_OBJ_TYPE_STR && seq->type == TF_OBJ_TYPE_STR) {
        return tf_split_string(ctx);
    }

    if (!tf_obj_is_callable(pred) || !is_sequence(seq)) {
        if (!is_sequence(seq)) {
            tf_ctx_runtime_errorf(ctx, "'%s' expected sequence at stack depth 1, found %s\n",
                                  ctx->current_word, tf_obj_type_name(seq));
        } else {
            tf_ctx_runtime_errorf(ctx, "'%s' expected callable or string separator at stack depth 0, found %s\n",
                                  ctx->current_word, tf_obj_type_name(pred));
        }
        return TF_ERR;
    }

    pred = tf_stack_pop(ctx);
    seq = tf_stack_pop(ctx);

    split_state *state = tf_xmalloc(sizeof(*state));
    state->pred = pred;
    state->seq = seq;
    state->index = 0;
    state->current = NULL;
    state->string_result = seq->type == TF_OBJ_TYPE_STR;
    state->list_result = seq->type == TF_OBJ_TYPE_LIST;
    state->true_list = state->string_result ? NULL : tf_obj_new_vector();
    state->false_list = state->string_result ? NULL : tf_obj_new_vector();
    state->true_str.ptr = NULL;
    state->true_str.len = 0;
    state->true_str.cap = 0;
    state->false_str.ptr = NULL;
    state->false_str.len = 0;
    state->false_str.cap = 0;
    if (state->string_result) {
        bytebuf_init(&state->true_str, seq->str.len);
        bytebuf_init(&state->false_str, seq->str.len);
    }
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, split_step, split_cleanup, state);
    return TF_OK;
}

tf_ret tf_merge(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 3) || !tf_ctx_require_callable(ctx, 0) ||
        !tf_ctx_require_sequence(ctx, 2)) {
        return TF_ERR;
    }
    tf_obj *pred = tf_stack_peek(ctx, 0);
    tf_obj *l2 = tf_stack_peek(ctx, 1);
    tf_obj *l1 = tf_stack_peek(ctx, 2);
    if (!is_sequence(l2)) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected sequence at stack depth 1, found %s\n",
                              ctx->current_word, tf_obj_type_name(l2));
        return TF_ERR;
    }
    if (l1->type != l2->type) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected matching sequence types, found %s and %s\n",
                              ctx->current_word, tf_obj_type_name(l1),
                              tf_obj_type_name(l2));
        return TF_ERR;
    }
    pred = tf_stack_pop(ctx);
    l2 = tf_stack_pop(ctx);
    l1 = tf_stack_pop(ctx);

    merge_state *state = tf_xmalloc(sizeof(*state));
    state->pred = pred;
    state->l1 = l1;
    state->l2 = l2;
    state->i1 = 0;
    state->i2 = 0;
    state->o1 = NULL;
    state->o2 = NULL;
    state->string_result = l1->type == TF_OBJ_TYPE_STR;
    state->list_result = l1->type == TF_OBJ_TYPE_LIST;
    state->vector_result = state->string_result ? NULL : tf_obj_new_vector();
    state->str_result.ptr = NULL;
    state->str_result.len = 0;
    state->str_result.cap = 0;
    if (state->string_result) {
        bytebuf_init(&state->str_result, l1->str.len + l2->str.len);
    }
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, merge_step, merge_cleanup, state);
    return TF_OK;
}

tf_ret tf_filter(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 1) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *pred = tf_stack_peek(ctx, 0);
    tf_obj *seq = tf_stack_peek(ctx, 1);

    pred = tf_stack_pop(ctx);
    seq = tf_stack_pop(ctx);

    filter_state *state = tf_xmalloc(sizeof(*state));
    state->pred = pred;
    state->seq = seq;
    state->index = 0;
    state->current = NULL;
    state->string_result = seq->type == TF_OBJ_TYPE_STR;
    state->list_result = seq->type == TF_OBJ_TYPE_LIST;
    state->vector_result = state->string_result ? NULL : tf_obj_new_vector();
    state->str_result.ptr = NULL;
    state->str_result.len = 0;
    state->str_result.cap = 0;
    if (state->string_result) bytebuf_init(&state->str_result, seq->str.len);
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, filter_step, filter_cleanup, state);
    return TF_OK;
}

tf_ret tf_some(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 1) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *pred = tf_stack_peek(ctx, 0);
    tf_obj *seq = tf_stack_peek(ctx, 1);

    pred = tf_stack_pop(ctx);
    seq = tf_stack_pop(ctx);

    quantifier_state *state = tf_xmalloc(sizeof(*state));
    state->pred = pred;
    state->seq = seq;
    state->index = 0;
    state->current = NULL;
    state->kind = TF_QUANT_SOME;
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, quantifier_step, quantifier_cleanup, state);
    return TF_OK;
}

tf_ret tf_all(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 1) || !tf_ctx_require_callable(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *pred = tf_stack_peek(ctx, 0);
    tf_obj *seq = tf_stack_peek(ctx, 1);

    pred = tf_stack_pop(ctx);
    seq = tf_stack_pop(ctx);

    quantifier_state *state = tf_xmalloc(sizeof(*state));
    state->pred = pred;
    state->seq = seq;
    state->index = 0;
    state->current = NULL;
    state->kind = TF_QUANT_ALL;
    predicate_eval_init(&state->pred_eval);

    tf_frame_push_native(ctx, quantifier_step, quantifier_cleanup, state);
    return TF_OK;
}
