#include "tf_lib.h"
#include <string.h>
#include <stdlib.h>
#include "tf_exec.h"
#include "tf_alloc.h"
#include "tf_console.h"
#include "tf_obj.h"

tf_ret tf_exec(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop_callable(ctx);
    if (!o) return TF_ERR;

    tf_ret res = tf_call_callable(ctx, o);
    release_obj(o);
    return res;
}

tf_ret tf_app2(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *top = stack_peek(ctx, 0);
    if (!tf_is_callable(top)) return TF_ERR;

    tf_obj *prg = stack_pop(ctx);
    tf_obj *b = stack_pop(ctx);
    tf_obj *a = stack_pop(ctx);

    tf_obj *synthetic = init_list_obj();
    tf_obj *exec_sym = create_symbol_obj("exec", 4);

    push_obj(synthetic, a);
    push_obj(synthetic, prg);
    retain_obj(prg);
    push_obj(synthetic, exec_sym);
    retain_obj(exec_sym);

    push_obj(synthetic, b);
    push_obj(synthetic, prg);
    push_obj(synthetic, exec_sym);

    frame_push(ctx, synthetic);
    release_obj(synthetic);

    return TF_OK;
}

static void tf_restore_stack_owned(tf_ctx *ctx, tf_obj **saved_stack,
                                   size_t saved_len) {
    while (stack_len(ctx) > 0) {
        tf_obj *o = stack_pop(ctx);
        release_obj(o);
    }
    for (size_t i = 0; i < saved_len; i++) stack_push(ctx, saved_stack[i]);
}

static void tf_restore_stack_copy(tf_ctx *ctx, tf_obj **saved_stack,
                                  size_t saved_len) {
    while (stack_len(ctx) > 0) {
        tf_obj *o = stack_pop(ctx);
        release_obj(o);
    }
    for (size_t i = 0; i < saved_len; i++) {
        stack_push(ctx, saved_stack[i]);
        retain_obj(saved_stack[i]);
    }
}

static void push_retained(tf_obj *list, tf_obj *elem) {
    retain_obj(elem);
    push_obj(list, elem);
}

// Sequence combinators treat strings as byte sequences. A string item is a
// one-byte string so the same callable protocol works for lists and strings.
static bool tf_is_sequence(tf_obj *o) {
    return o->type == TF_OBJ_TYPE_LIST || o->type == TF_OBJ_TYPE_STR;
}

static size_t tf_sequence_len(tf_obj *seq) {
    return seq->type == TF_OBJ_TYPE_LIST ? seq->list.len : seq->str.len;
}

static tf_obj *tf_sequence_item_owned(tf_obj *seq, size_t idx) {
    if (seq->type == TF_OBJ_TYPE_LIST) {
        tf_obj *elem = seq->list.elem[idx];
        retain_obj(elem);
        return elem;
    }
    return create_string_obj(seq->str.ptr + idx, 1);
}

static bool tf_is_char_string(tf_obj *o) {
    return o->type == TF_OBJ_TYPE_STR && o->str.len == 1;
}

typedef struct {
    char *ptr;
    size_t len;
    size_t cap;
} tf_bytebuf;

static void tf_bytebuf_init(tf_bytebuf *buf, size_t cap) {
    buf->len = 0;
    buf->cap = cap > 0 ? cap : 1;
    buf->ptr = xmalloc(buf->cap + 1);
    buf->ptr[0] = '\0';
}

static void tf_bytebuf_append(tf_bytebuf *buf, char c) {
    if (buf->len >= buf->cap) {
        buf->cap *= 2;
        buf->ptr = xrealloc(buf->ptr, buf->cap + 1);
    }
    buf->ptr[buf->len++] = c;
    buf->ptr[buf->len] = '\0';
}

static tf_obj *tf_bytebuf_to_string(tf_bytebuf *buf) {
    return create_string_obj(buf->ptr, buf->len);
}

static void tf_save_stack_copy(tf_ctx *ctx, tf_obj ***saved_stack,
                               size_t *saved_len) {
    *saved_len = stack_len(ctx);
    *saved_stack =
        *saved_len > 0 ? xmalloc(sizeof(tf_obj *) * *saved_len) : NULL;
    for (size_t i = 0; i < *saved_len; i++) {
        (*saved_stack)[i] = stack_peek(ctx, *saved_len - 1 - i);
        retain_obj((*saved_stack)[i]);
    }
}

static void tf_release_stack_copy(tf_obj **saved_stack, size_t saved_len) {
    for (size_t i = 0; i < saved_len; i++) release_obj(saved_stack[i]);
    free(saved_stack);
}

static void tf_clear_stack(tf_ctx *ctx) {
    while (stack_len(ctx) > 0) {
        tf_obj *o = stack_pop(ctx);
        release_obj(o);
    }
}

static tf_ret tf_collect_outputs(tf_ctx *ctx, size_t base_len,
                                 tf_obj *outputs) {
    size_t len = stack_len(ctx);
    if (len < base_len) return TF_ERR;
    for (size_t i = base_len; i < len; i++) {
        push_retained(outputs, ctx->forth_stack->list.elem[i]);
    }
    return TF_OK;
}

typedef struct {
    tf_obj *body;
    int remaining;
} tf_times_state;

static tf_ret tf_times_step(tf_ctx *ctx, void *state, bool *done) {
    tf_times_state *s = state;
    if (s->remaining <= 0) {
        *done = true;
        return TF_OK;
    }

    s->remaining--;
    *done = false;
    return tf_call_callable(ctx, s->body);
}

static void tf_times_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)ctx;
    (void)status;
    tf_times_state *s = state;
    release_obj(s->body);
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj *data;
    size_t index;
} tf_each_state;

static tf_ret tf_each_step(tf_ctx *ctx, void *state, bool *done) {
    tf_each_state *s = state;
    size_t len = tf_sequence_len(s->data);
    if (s->index >= len) {
        *done = true;
        return TF_OK;
    }

    stack_push(ctx, tf_sequence_item_owned(s->data, s->index++));
    *done = false;
    return tf_call_callable(ctx, s->body);
}

static void tf_each_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)ctx;
    (void)status;
    tf_each_state *s = state;
    release_obj(s->body);
    release_obj(s->data);
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj *saved;
    tf_obj **saved_stack;
    size_t saved_len;
    bool scheduled;
} tf_dip_state;

static tf_ret tf_dip_step(tf_ctx *ctx, void *state, bool *done) {
    tf_dip_state *s = state;
    if (!s->scheduled) {
        s->scheduled = true;
        *done = false;
        return tf_call_callable(ctx, s->body);
    }

    stack_push(ctx, s->saved);
    s->saved = NULL;
    *done = true;
    return TF_OK;
}

static void tf_dip_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    tf_dip_state *s = state;
    if (status != TF_OK) {
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
        if (s->saved) {
            stack_push(ctx, s->saved);
            s->saved = NULL;
        }
    }
    tf_release_stack_copy(s->saved_stack, s->saved_len);
    release_obj(s->body);
    if (s->saved) release_obj(s->saved);
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj *saved;
    tf_obj **saved_stack;
    size_t base_len;
    bool scheduled;
} tf_keep_state;

static tf_ret tf_keep_step(tf_ctx *ctx, void *state, bool *done) {
    tf_keep_state *s = state;
    if (!s->scheduled) {
        s->scheduled = true;
        retain_obj(s->saved);
        stack_push(ctx, s->saved);
        *done = false;
        return tf_call_callable(ctx, s->body);
    }

    if (stack_len(ctx) < s->base_len) return TF_ERR;

    size_t out_len = stack_len(ctx) - s->base_len;
    tf_obj **outputs = out_len > 0 ? xmalloc(sizeof(tf_obj *) * out_len) : NULL;
    for (size_t i = out_len; i > 0; i--) outputs[i - 1] = stack_pop(ctx);

    stack_push(ctx, s->saved);
    s->saved = NULL;
    for (size_t i = 0; i < out_len; i++) stack_push(ctx, outputs[i]);

    free(outputs);
    *done = true;
    return TF_OK;
}

static void tf_keep_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    tf_keep_state *s = state;
    if (status != TF_OK) {
        tf_restore_stack_copy(ctx, s->saved_stack, s->base_len);
        if (s->saved) {
            stack_push(ctx, s->saved);
            s->saved = NULL;
        }
    }
    tf_release_stack_copy(s->saved_stack, s->base_len);
    release_obj(s->body);
    if (s->saved) release_obj(s->saved);
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj *data;
    tf_obj **saved_stack;
    size_t saved_len;
    size_t index;
    bool awaiting_body;
} tf_fold_state;

static tf_ret tf_fold_step(tf_ctx *ctx, void *state, bool *done) {
    tf_fold_state *s = state;

    if (s->awaiting_body) {
        if (stack_len(ctx) != s->saved_len + 1) return TF_ERR;
        s->awaiting_body = false;
    }

    size_t len = tf_sequence_len(s->data);
    if (s->index >= len) {
        *done = true;
        return TF_OK;
    }

    stack_push(ctx, tf_sequence_item_owned(s->data, s->index++));
    s->awaiting_body = true;
    *done = false;
    return tf_call_callable(ctx, s->body);
}

static void tf_fold_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    tf_fold_state *s = state;
    if (status != TF_OK) {
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    tf_release_stack_copy(s->saved_stack, s->saved_len);
    release_obj(s->body);
    release_obj(s->data);
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
    tf_obj *list_result;
    tf_bytebuf str_result;
} tf_map_state;

static tf_ret tf_map_step(tf_ctx *ctx, void *state, bool *done) {
    tf_map_state *s = state;

    if (s->awaiting_body) {
        if (stack_len(ctx) != s->saved_len + 1) return TF_ERR;

        tf_obj *mapped = stack_pop(ctx);
        if (s->string_result) {
            if (!tf_is_char_string(mapped)) {
                release_obj(mapped);
                return TF_ERR;
            }
            tf_bytebuf_append(&s->str_result, mapped->str.ptr[0]);
            release_obj(mapped);
        } else {
            push_obj(s->list_result, mapped);
        }
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
        s->awaiting_body = false;
    }

    size_t len = tf_sequence_len(s->data);
    if (s->index >= len) {
        if (s->string_result) {
            stack_push(ctx, tf_bytebuf_to_string(&s->str_result));
            free(s->str_result.ptr);
            s->str_result.ptr = NULL;
        } else {
            stack_push(ctx, s->list_result);
            s->list_result = NULL;
        }
        *done = true;
        return TF_OK;
    }

    stack_push(ctx, tf_sequence_item_owned(s->data, s->index++));
    s->awaiting_body = true;
    *done = false;
    return tf_call_callable(ctx, s->body);
}

static void tf_map_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    tf_map_state *s = state;
    if (status != TF_OK) {
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    tf_release_stack_copy(s->saved_stack, s->saved_len);
    release_obj(s->body);
    release_obj(s->data);
    if (s->string_result) {
        free(s->str_result.ptr);
    } else if (s->list_result) {
        release_obj(s->list_result);
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
} tf_replicate_state;

static tf_ret tf_replicate_step(tf_ctx *ctx, void *state, bool *done) {
    tf_replicate_state *s = state;

    if (s->awaiting_body) {
        if (stack_len(ctx) != s->saved_len + 1) return TF_ERR;
        tf_obj *item = stack_pop(ctx);
        push_obj(s->result, item);
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
        s->awaiting_body = false;
    }

    if (s->remaining <= 0) {
        stack_push(ctx, s->result);
        s->result = NULL;
        *done = true;
        return TF_OK;
    }

    s->remaining--;
    s->awaiting_body = true;
    *done = false;
    return tf_call_callable(ctx, s->body);
}

static void tf_replicate_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    tf_replicate_state *s = state;
    if (status != TF_OK) {
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    tf_release_stack_copy(s->saved_stack, s->saved_len);
    release_obj(s->body);
    if (s->result) release_obj(s->result);
    free(s);
}

typedef struct {
    tf_obj *body;
    tf_obj *data;
    tf_obj **saved_stack;
    size_t saved_len;
    bool scheduled;
    tf_obj *result;
} tf_infra_state;

static tf_ret tf_infra_step(tf_ctx *ctx, void *state, bool *done) {
    tf_infra_state *s = state;
    if (!s->scheduled) {
        s->scheduled = true;
        *done = false;
        return tf_call_callable(ctx, s->body);
    }

    s->result = init_list_obj();
    tf_ret res = tf_collect_outputs(ctx, 0, s->result);
    if (res != TF_OK) return res;

    tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    stack_push(ctx, s->result);
    s->result = NULL;
    *done = true;
    return TF_OK;
}

static void tf_infra_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    tf_infra_state *s = state;
    if (status != TF_OK) {
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    tf_release_stack_copy(s->saved_stack, s->saved_len);
    release_obj(s->body);
    release_obj(s->data);
    if (s->result) release_obj(s->result);
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
} tf_cleave_state;

static tf_ret tf_cleave_step(tf_ctx *ctx, void *state, bool *done) {
    tf_cleave_state *s = state;

    if (s->awaiting_branch) {
        tf_ret res = tf_collect_outputs(ctx, s->saved_len, s->outputs);
        if (res != TF_OK) return res;
        s->awaiting_branch = false;
        s->index++;
    }

    if (s->index >= s->branches->list.len) {
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
        if (s->construct_result) {
            stack_push(ctx, s->outputs);
            s->outputs = NULL;
        } else {
            for (size_t i = 0; i < s->outputs->list.len; i++) {
                stack_push(ctx, s->outputs->list.elem[i]);
                retain_obj(s->outputs->list.elem[i]);
            }
        }
        *done = true;
        return TF_OK;
    }

    tf_obj *branch = s->branches->list.elem[s->index];
    if (!tf_is_callable(branch)) return TF_ERR;

    tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    stack_push(ctx, s->value);
    retain_obj(s->value);
    s->awaiting_branch = true;
    *done = false;
    return tf_call_callable(ctx, branch);
}

static void tf_cleave_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    tf_cleave_state *s = state;
    if (status != TF_OK) {
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    tf_release_stack_copy(s->saved_stack, s->saved_len);
    release_obj(s->branches);
    release_obj(s->value);
    if (s->outputs) release_obj(s->outputs);
    free(s);
}

typedef enum {
    TF_BI_RUN_LEFT,
    TF_BI_AFTER_LEFT,
    TF_BI_RUN_RIGHT,
    TF_BI_AFTER_RIGHT
} tf_bi_stage;

typedef struct {
    tf_obj *left;
    tf_obj *right;
    tf_obj *saved;
    tf_obj **saved_stack;
    size_t base_len;
    tf_bi_stage stage;
    tf_obj **left_outputs;
    size_t left_out_len;
} tf_bi_state;

static tf_ret tf_bi_step(tf_ctx *ctx, void *state, bool *done) {
    tf_bi_state *s = state;

    if (s->stage == TF_BI_RUN_LEFT) {
        retain_obj(s->saved);
        stack_push(ctx, s->saved);
        s->stage = TF_BI_AFTER_LEFT;
        *done = false;
        return tf_call_callable(ctx, s->left);
    }

    if (s->stage == TF_BI_AFTER_LEFT) {
        if (stack_len(ctx) < s->base_len) return TF_ERR;
        s->left_out_len = stack_len(ctx) - s->base_len;
        s->left_outputs =
            s->left_out_len > 0
                ? xmalloc(sizeof(tf_obj *) * s->left_out_len)
                : NULL;
        for (size_t i = s->left_out_len; i > 0; i--) {
            s->left_outputs[i - 1] = stack_pop(ctx);
        }

        retain_obj(s->saved);
        stack_push(ctx, s->saved);
        s->stage = TF_BI_AFTER_RIGHT;
        *done = false;
        return tf_call_callable(ctx, s->right);
    }

    if (stack_len(ctx) < s->base_len) return TF_ERR;
    size_t right_out_len = stack_len(ctx) - s->base_len;
    tf_obj **right_outputs =
        right_out_len > 0 ? xmalloc(sizeof(tf_obj *) * right_out_len) : NULL;
    for (size_t i = right_out_len; i > 0; i--) {
        right_outputs[i - 1] = stack_pop(ctx);
    }

    for (size_t i = 0; i < s->left_out_len; i++) {
        stack_push(ctx, s->left_outputs[i]);
    }
    free(s->left_outputs);
    s->left_outputs = NULL;
    s->left_out_len = 0;

    for (size_t i = 0; i < right_out_len; i++) stack_push(ctx, right_outputs[i]);
    free(right_outputs);

    *done = true;
    return TF_OK;
}

static void tf_bi_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    tf_bi_state *s = state;
    if (status != TF_OK) {
        tf_restore_stack_copy(ctx, s->saved_stack, s->base_len);
        if (s->saved) {
            stack_push(ctx, s->saved);
            s->saved = NULL;
        }
    }
    for (size_t i = 0; i < s->left_out_len; i++) {
        release_obj(s->left_outputs[i]);
    }
    free(s->left_outputs);
    tf_release_stack_copy(s->saved_stack, s->base_len);
    release_obj(s->left);
    release_obj(s->right);
    if (s->saved) release_obj(s->saved);
    free(s);
}

typedef enum { TF_TRY_START, TF_TRY_BODY, TF_TRY_HANDLER } tf_try_stage;

typedef struct {
    tf_obj *body;
    tf_obj *handler;
    tf_obj **saved_stack;
    size_t saved_len;
    tf_try_stage stage;
    bool suppressing_errors;
} tf_try_state;

static tf_ret tf_try_step(tf_ctx *ctx, void *state, bool *done) {
    tf_try_state *s = state;
    if (s->stage == TF_TRY_START) {
        ctx->error_suppression_depth++;
        s->suppressing_errors = true;
        s->stage = TF_TRY_BODY;
        *done = false;
        return tf_call_callable(ctx, s->body);
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

static tf_ret tf_try_error(tf_ctx *ctx, void *state, tf_ret status,
                           bool *handled) {
    tf_try_state *s = state;
    *handled = false;
    if (status != TF_ERR || s->stage != TF_TRY_BODY) return TF_OK;

    if (s->suppressing_errors) {
        ctx->error_suppression_depth--;
        s->suppressing_errors = false;
    }
    tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    s->stage = TF_TRY_HANDLER;

    tf_ret res = tf_call_callable(ctx, s->handler);
    if (res == TF_OK) *handled = true;
    return res;
}

static void tf_try_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    tf_try_state *s = state;
    if (s->suppressing_errors) {
        ctx->error_suppression_depth--;
        s->suppressing_errors = false;
    }
    if (status != TF_OK && s->stage == TF_TRY_BODY) {
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    tf_release_stack_copy(s->saved_stack, s->saved_len);
    release_obj(s->body);
    release_obj(s->handler);
    free(s);
}

typedef enum {
    TF_PRED_IDLE,
    TF_PRED_AWAITING
} tf_predicate_eval_phase;

typedef struct {
    tf_predicate_eval_phase phase;
    tf_obj **saved_stack;
    size_t saved_len;
} tf_predicate_eval;

static void tf_predicate_eval_init(tf_predicate_eval *eval) {
    eval->phase = TF_PRED_IDLE;
    eval->saved_stack = NULL;
    eval->saved_len = 0;
}

static void tf_predicate_eval_clear(tf_predicate_eval *eval) {
    eval->phase = TF_PRED_IDLE;
    eval->saved_stack = NULL;
    eval->saved_len = 0;
}

static tf_ret tf_predicate_eval_step(tf_ctx *ctx, tf_predicate_eval *eval,
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

        if (!tf_is_callable(pred)) return TF_ERR;

        tf_save_stack_copy(ctx, &eval->saved_stack, &eval->saved_len);
        for (size_t i = 0; i < input_len; i++) {
            stack_push(ctx, inputs[i]);
            retain_obj(inputs[i]);
        }

        eval->phase = TF_PRED_AWAITING;
        return tf_call_callable(ctx, pred);
    }

    tf_obj *bool_res = stack_pop_type(ctx, TF_OBJ_TYPE_BOOL);
    if (!bool_res) {
        tf_restore_stack_owned(ctx, eval->saved_stack, eval->saved_len);
        free(eval->saved_stack);
        tf_predicate_eval_clear(eval);
        return TF_ERR;
    }

    *pred_val = bool_res->b;
    release_obj(bool_res);

    tf_restore_stack_owned(ctx, eval->saved_stack, eval->saved_len);
    free(eval->saved_stack);
    tf_predicate_eval_clear(eval);
    *ready = true;
    return TF_OK;
}

static void tf_predicate_eval_cleanup(tf_ctx *ctx, tf_predicate_eval *eval) {
    if (eval->phase == TF_PRED_AWAITING && eval->saved_stack) {
        tf_restore_stack_owned(ctx, eval->saved_stack, eval->saved_len);
        free(eval->saved_stack);
        tf_predicate_eval_clear(eval);
    }
}

typedef enum { TF_IF_COND, TF_IF_BODY } tf_if_stage;

typedef struct {
    tf_obj *cond;
    tf_obj *then_b;
    tf_obj *else_b;
    bool has_else;
    tf_if_stage stage;
    tf_predicate_eval pred_eval;
} tf_if_state;

static tf_ret tf_if_step(tf_ctx *ctx, void *state, bool *done) {
    tf_if_state *s = state;
    if (s->stage == TF_IF_COND) {
        bool ready = false;
        bool cond_val = false;
        tf_ret res = tf_predicate_eval_step(ctx, &s->pred_eval, s->cond, true,
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
        return tf_call_callable(ctx, body);
    }

    *done = true;
    return TF_OK;
}

static void tf_if_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    tf_if_state *s = state;
    tf_predicate_eval_cleanup(ctx, &s->pred_eval);
    release_obj(s->cond);
    release_obj(s->then_b);
    if (s->has_else) release_obj(s->else_b);
    free(s);
}

typedef enum { TF_WHILE_COND, TF_WHILE_BODY } tf_while_stage;

typedef struct {
    tf_obj *cond;
    tf_obj *body;
    tf_while_stage stage;
    tf_predicate_eval pred_eval;
} tf_while_state;

static tf_ret tf_while_step(tf_ctx *ctx, void *state, bool *done) {
    tf_while_state *s = state;
    if (s->stage == TF_WHILE_COND) {
        bool ready = false;
        bool continue_loop = false;
        tf_ret res = tf_predicate_eval_step(ctx, &s->pred_eval, s->cond, false,
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
        return tf_call_callable(ctx, s->body);
    }

    s->stage = TF_WHILE_COND;
    *done = false;
    return TF_OK;
}

static void tf_while_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    tf_while_state *s = state;
    tf_predicate_eval_cleanup(ctx, &s->pred_eval);
    release_obj(s->cond);
    release_obj(s->body);
    free(s);
}

typedef enum { TF_COND_PRED, TF_COND_BODY } tf_cond_stage;

typedef struct {
    tf_obj *clauses;
    size_t index;
    tf_cond_stage stage;
    tf_predicate_eval pred_eval;
} tf_cond_state;

static tf_ret tf_cond_step(tf_ctx *ctx, void *state, bool *done) {
    tf_cond_state *s = state;
    if (s->stage == TF_COND_BODY) {
        *done = true;
        return TF_OK;
    }

    while (s->index < s->clauses->list.len) {
        tf_obj *clause = s->clauses->list.elem[s->index];
        if (clause->type != TF_OBJ_TYPE_LIST || clause->list.len != 2) {
            return TF_ERR;
        }

        tf_obj *pred = clause->list.elem[0];
        tf_obj *body = clause->list.elem[1];
        if (!tf_is_callable(body)) return TF_ERR;

        bool ready = false;
        bool cond_val = false;
        tf_ret res = tf_predicate_eval_step(ctx, &s->pred_eval, pred, true,
                                            NULL, 0, &ready, &cond_val);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        if (cond_val) {
            s->stage = TF_COND_BODY;
            *done = false;
            return tf_call_callable(ctx, body);
        }

        s->index++;
    }

    *done = true;
    return TF_OK;
}

static void tf_cond_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    tf_cond_state *s = state;
    tf_predicate_eval_cleanup(ctx, &s->pred_eval);
    release_obj(s->clauses);
    free(s);
}

typedef struct {
    tf_obj *pred;
    tf_obj *seq;
    size_t index;
    tf_obj *current;
    bool string_result;
    tf_obj *list_result;
    tf_bytebuf str_result;
    tf_predicate_eval pred_eval;
} tf_filter_state;

static tf_ret tf_filter_step(tf_ctx *ctx, void *state, bool *done) {
    tf_filter_state *s = state;
    size_t len = tf_sequence_len(s->seq);

    while (s->index < len || s->current) {
        if (!s->current) {
            s->current = tf_sequence_item_owned(s->seq, s->index++);
        }

        tf_obj *inputs[] = { s->current };
        bool ready = false;
        bool keep = false;
        tf_ret res = tf_predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, inputs, 1, &ready, &keep);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        if (keep) {
            if (s->string_result) {
                tf_bytebuf_append(&s->str_result, s->current->str.ptr[0]);
                release_obj(s->current);
            } else {
                push_obj(s->list_result, s->current);
            }
        } else {
            release_obj(s->current);
        }
        s->current = NULL;
    }

    if (s->string_result) {
        stack_push(ctx, tf_bytebuf_to_string(&s->str_result));
        free(s->str_result.ptr);
        s->str_result.ptr = NULL;
    } else {
        stack_push(ctx, s->list_result);
        s->list_result = NULL;
    }
    *done = true;
    return TF_OK;
}

static void tf_filter_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    tf_filter_state *s = state;
    tf_predicate_eval_cleanup(ctx, &s->pred_eval);
    release_obj(s->pred);
    release_obj(s->seq);
    if (s->current) release_obj(s->current);
    if (s->string_result) {
        free(s->str_result.ptr);
    } else if (s->list_result) {
        release_obj(s->list_result);
    }
    free(s);
}

typedef enum { TF_QUANT_SOME, TF_QUANT_ALL } tf_quantifier_kind;

typedef struct {
    tf_obj *pred;
    tf_obj *seq;
    size_t index;
    tf_obj *current;
    tf_quantifier_kind kind;
    tf_predicate_eval pred_eval;
} tf_quantifier_state;

static tf_ret tf_quantifier_step(tf_ctx *ctx, void *state, bool *done) {
    tf_quantifier_state *s = state;
    size_t len = tf_sequence_len(s->seq);

    while (s->index < len || s->current) {
        if (!s->current) {
            s->current = tf_sequence_item_owned(s->seq, s->index++);
        }

        tf_obj *inputs[] = { s->current };
        bool ready = false;
        bool pred_val = false;
        tf_ret res = tf_predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, inputs, 1, &ready,
                                            &pred_val);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        release_obj(s->current);
        s->current = NULL;

        if (s->kind == TF_QUANT_SOME && pred_val) {
            stack_push(ctx, create_bool_obj(true));
            *done = true;
            return TF_OK;
        }
        if (s->kind == TF_QUANT_ALL && !pred_val) {
            stack_push(ctx, create_bool_obj(false));
            *done = true;
            return TF_OK;
        }
    }

    stack_push(ctx, create_bool_obj(s->kind == TF_QUANT_ALL));
    *done = true;
    return TF_OK;
}

static void tf_quantifier_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    tf_quantifier_state *s = state;
    tf_predicate_eval_cleanup(ctx, &s->pred_eval);
    release_obj(s->pred);
    release_obj(s->seq);
    if (s->current) release_obj(s->current);
    free(s);
}

typedef struct {
    tf_obj *pred;
    tf_obj *seq;
    size_t index;
    tf_obj *current;
    bool string_result;
    tf_obj *true_list;
    tf_obj *false_list;
    tf_bytebuf true_str;
    tf_bytebuf false_str;
    tf_predicate_eval pred_eval;
} tf_split_state;

static tf_ret tf_split_step(tf_ctx *ctx, void *state, bool *done) {
    tf_split_state *s = state;
    size_t len = tf_sequence_len(s->seq);

    while (s->index < len || s->current) {
        if (!s->current) {
            s->current = tf_sequence_item_owned(s->seq, s->index++);
        }

        tf_obj *inputs[] = { s->current };
        bool ready = false;
        bool pred_val = false;
        tf_ret res = tf_predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, inputs, 1, &ready,
                                            &pred_val);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        if (s->string_result) {
            tf_bytebuf_append(pred_val ? &s->true_str : &s->false_str,
                              s->current->str.ptr[0]);
            release_obj(s->current);
        } else {
            push_obj(pred_val ? s->true_list : s->false_list, s->current);
        }
        s->current = NULL;
    }

    if (s->string_result) {
        stack_push(ctx, tf_bytebuf_to_string(&s->true_str));
        stack_push(ctx, tf_bytebuf_to_string(&s->false_str));
        free(s->true_str.ptr);
        free(s->false_str.ptr);
        s->true_str.ptr = NULL;
        s->false_str.ptr = NULL;
    } else {
        stack_push(ctx, s->true_list);
        stack_push(ctx, s->false_list);
        s->true_list = NULL;
        s->false_list = NULL;
    }
    *done = true;
    return TF_OK;
}

static void tf_split_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    tf_split_state *s = state;
    tf_predicate_eval_cleanup(ctx, &s->pred_eval);
    release_obj(s->pred);
    release_obj(s->seq);
    if (s->current) release_obj(s->current);
    if (s->string_result) {
        free(s->true_str.ptr);
        free(s->false_str.ptr);
    } else {
        if (s->true_list) release_obj(s->true_list);
        if (s->false_list) release_obj(s->false_list);
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
    tf_obj *list_result;
    tf_bytebuf str_result;
    tf_predicate_eval pred_eval;
} tf_merge_state;

static void tf_merge_take_left(tf_merge_state *s) {
    if (s->string_result) {
        tf_bytebuf_append(&s->str_result, s->o1->str.ptr[0]);
        release_obj(s->o1);
    } else {
        push_obj(s->list_result, s->o1);
    }
    release_obj(s->o2);
    s->o1 = NULL;
    s->o2 = NULL;
    s->i1++;
}

static void tf_merge_take_right(tf_merge_state *s) {
    if (s->string_result) {
        tf_bytebuf_append(&s->str_result, s->o2->str.ptr[0]);
        release_obj(s->o2);
    } else {
        push_obj(s->list_result, s->o2);
    }
    release_obj(s->o1);
    s->o1 = NULL;
    s->o2 = NULL;
    s->i2++;
}

static tf_ret tf_merge_step(tf_ctx *ctx, void *state, bool *done) {
    tf_merge_state *s = state;
    size_t l1_len = tf_sequence_len(s->l1);
    size_t l2_len = tf_sequence_len(s->l2);

    while ((s->i1 < l1_len && s->i2 < l2_len) || s->o1 || s->o2) {
        if (!s->o1 && !s->o2) {
            s->o1 = tf_sequence_item_owned(s->l1, s->i1);
            s->o2 = tf_sequence_item_owned(s->l2, s->i2);
        }

        tf_obj *inputs[] = { s->o1, s->o2 };
        bool ready = false;
        bool take_left = false;
        tf_ret res = tf_predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, inputs, 2, &ready,
                                            &take_left);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        if (take_left) {
            tf_merge_take_left(s);
        } else {
            tf_merge_take_right(s);
        }
    }

    while (s->i1 < l1_len) {
        if (s->string_result) {
            tf_bytebuf_append(&s->str_result, s->l1->str.ptr[s->i1]);
        } else {
            push_retained(s->list_result, s->l1->list.elem[s->i1]);
        }
        s->i1++;
    }
    while (s->i2 < l2_len) {
        if (s->string_result) {
            tf_bytebuf_append(&s->str_result, s->l2->str.ptr[s->i2]);
        } else {
            push_retained(s->list_result, s->l2->list.elem[s->i2]);
        }
        s->i2++;
    }

    if (s->string_result) {
        stack_push(ctx, tf_bytebuf_to_string(&s->str_result));
        free(s->str_result.ptr);
        s->str_result.ptr = NULL;
    } else {
        stack_push(ctx, s->list_result);
        s->list_result = NULL;
    }
    *done = true;
    return TF_OK;
}

static void tf_merge_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    tf_merge_state *s = state;
    tf_predicate_eval_cleanup(ctx, &s->pred_eval);
    release_obj(s->pred);
    release_obj(s->l1);
    release_obj(s->l2);
    if (s->o1) release_obj(s->o1);
    if (s->o2) release_obj(s->o2);
    if (s->string_result) {
        free(s->str_result.ptr);
    } else if (s->list_result) {
        release_obj(s->list_result);
    }
    free(s);
}

typedef enum {
    TF_LINREC_PRED,
    TF_LINREC_THEN,
    TF_LINREC_REC1,
    TF_LINREC_CONT
} tf_linrec_stage;

typedef struct {
    tf_obj *pred;
    tf_obj *then_b;
    tf_obj *rec1;
    tf_obj *rec2;
    tf_linrec_stage stage;
    tf_predicate_eval pred_eval;
} tf_linrec_state;

static tf_ret tf_linrec_step(tf_ctx *ctx, void *state, bool *done) {
    tf_linrec_state *s = state;
    if (s->stage == TF_LINREC_PRED) {
        bool ready = false;
        bool is_done = false;
        tf_ret res = tf_predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, NULL, 0, &ready, &is_done);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        s->stage = is_done ? TF_LINREC_THEN : TF_LINREC_REC1;
        *done = false;
        return tf_call_callable(ctx, is_done ? s->then_b : s->rec1);
    }

    if (s->stage == TF_LINREC_THEN || s->stage == TF_LINREC_CONT) {
        *done = true;
        return TF_OK;
    }

    tf_obj *cont = init_list_obj();
    tf_obj *linrec_sym = create_symbol_obj("linrec", 6);
    tf_obj *exec_sym = create_symbol_obj("exec", 4);

    push_retained(cont, s->pred);
    push_retained(cont, s->then_b);
    push_retained(cont, s->rec1);
    push_retained(cont, s->rec2);
    push_obj(cont, linrec_sym);
    push_retained(cont, s->rec2);
    push_obj(cont, exec_sym);

    frame_push(ctx, cont);
    release_obj(cont);
    s->stage = TF_LINREC_CONT;
    *done = false;
    return TF_OK;
}

static void tf_linrec_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    tf_linrec_state *s = state;
    tf_predicate_eval_cleanup(ctx, &s->pred_eval);
    release_obj(s->pred);
    release_obj(s->then_b);
    release_obj(s->rec1);
    release_obj(s->rec2);
    free(s);
}

typedef enum {
    TF_BINREC_PRED,
    TF_BINREC_THEN,
    TF_BINREC_REC1,
    TF_BINREC_CONT
} tf_binrec_stage;

typedef struct {
    tf_obj *pred;
    tf_obj *then_b;
    tf_obj *rec1;
    tf_obj *rec2;
    tf_binrec_stage stage;
    tf_predicate_eval pred_eval;
} tf_binrec_state;

static tf_ret tf_binrec_step(tf_ctx *ctx, void *state, bool *done) {
    tf_binrec_state *s = state;
    if (s->stage == TF_BINREC_PRED) {
        bool ready = false;
        bool is_done = false;
        tf_ret res = tf_predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, NULL, 0, &ready, &is_done);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        s->stage = is_done ? TF_BINREC_THEN : TF_BINREC_REC1;
        *done = false;
        return tf_call_callable(ctx, is_done ? s->then_b : s->rec1);
    }

    if (s->stage == TF_BINREC_THEN || s->stage == TF_BINREC_CONT) {
        *done = true;
        return TF_OK;
    }

    tf_obj *rec_call = init_list_obj();
    tf_obj *binrec_sym = create_symbol_obj("binrec", 6);
    push_retained(rec_call, s->pred);
    push_retained(rec_call, s->then_b);
    push_retained(rec_call, s->rec1);
    push_retained(rec_call, s->rec2);
    push_obj(rec_call, binrec_sym);

    tf_obj *cont = init_list_obj();
    tf_obj *dip_sym = create_symbol_obj("dip", 3);
    tf_obj *exec_rec_sym = create_symbol_obj("exec", 4);
    tf_obj *exec_combine_sym = create_symbol_obj("exec", 4);

    push_retained(cont, rec_call);
    push_obj(cont, dip_sym);
    push_retained(cont, rec_call);
    push_obj(cont, exec_rec_sym);
    push_retained(cont, s->rec2);
    push_obj(cont, exec_combine_sym);

    frame_push(ctx, cont);
    release_obj(cont);
    release_obj(rec_call);
    s->stage = TF_BINREC_CONT;
    *done = false;
    return TF_OK;
}

static void tf_binrec_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    tf_binrec_state *s = state;
    tf_predicate_eval_cleanup(ctx, &s->pred_eval);
    release_obj(s->pred);
    release_obj(s->then_b);
    release_obj(s->rec1);
    release_obj(s->rec2);
    free(s);
}

typedef enum {
    TF_GENREC_PRED,
    TF_GENREC_THEN,
    TF_GENREC_BEFORE,
    TF_GENREC_CONT
} tf_genrec_stage;

typedef struct {
    tf_obj *pred;
    tf_obj *then_b;
    tf_obj *before;
    tf_obj *after;
    tf_genrec_stage stage;
    tf_predicate_eval pred_eval;
} tf_genrec_state;

static tf_ret tf_genrec_step(tf_ctx *ctx, void *state, bool *done) {
    tf_genrec_state *s = state;
    if (s->stage == TF_GENREC_PRED) {
        bool ready = false;
        bool is_done = false;
        tf_ret res = tf_predicate_eval_step(ctx, &s->pred_eval, s->pred,
                                            false, NULL, 0, &ready, &is_done);
        if (res != TF_OK || !ready) {
            *done = false;
            return res;
        }

        s->stage = is_done ? TF_GENREC_THEN : TF_GENREC_BEFORE;
        *done = false;
        return tf_call_callable(ctx, is_done ? s->then_b : s->before);
    }

    if (s->stage == TF_GENREC_THEN || s->stage == TF_GENREC_CONT) {
        *done = true;
        return TF_OK;
    }

    tf_obj *cont = init_list_obj();
    tf_obj *genrec_sym = create_symbol_obj("genrec", 6);
    tf_obj *exec_sym = create_symbol_obj("exec", 4);

    push_retained(cont, s->pred);
    push_retained(cont, s->then_b);
    push_retained(cont, s->before);
    push_retained(cont, s->after);
    push_obj(cont, genrec_sym);
    push_retained(cont, s->after);
    push_obj(cont, exec_sym);

    frame_push(ctx, cont);
    release_obj(cont);
    s->stage = TF_GENREC_CONT;
    *done = false;
    return TF_OK;
}

static void tf_genrec_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    (void)status;
    tf_genrec_state *s = state;
    tf_predicate_eval_cleanup(ctx, &s->pred_eval);
    release_obj(s->pred);
    release_obj(s->then_b);
    release_obj(s->before);
    release_obj(s->after);
    free(s);
}

typedef enum {
    TF_TREEREC_START,
    TF_TREEREC_LEAF_BODY,
    TF_TREEREC_CHILD_LOOP,
    TF_TREEREC_CHILD_DONE,
    TF_TREEREC_NODE_BODY
} tf_treerec_stage;

typedef struct {
    tf_obj *tree;
    tf_obj *leaf;
    tf_obj *node;
    tf_obj *mapped;
    size_t index;
    tf_obj **saved_stack;
    size_t saved_len;
    tf_treerec_stage stage;
} tf_treerec_state;

static void tf_treerec_push_owned(tf_ctx *ctx, tf_obj *tree, tf_obj *leaf,
                                  tf_obj *node);

static tf_ret tf_treerec_finish_single(tf_ctx *ctx, tf_treerec_state *s,
                                       bool *done) {
    if (stack_len(ctx) != s->saved_len + 1) return TF_ERR;
    tf_obj *result = stack_pop(ctx);
    tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    stack_push(ctx, result);
    *done = true;
    return TF_OK;
}

static tf_ret tf_treerec_step(tf_ctx *ctx, void *state, bool *done) {
    tf_treerec_state *s = state;

    if (s->stage == TF_TREEREC_START) {
        if (s->tree->type != TF_OBJ_TYPE_LIST) {
            stack_push(ctx, s->tree);
            retain_obj(s->tree);
            s->stage = TF_TREEREC_LEAF_BODY;
            *done = false;
            return tf_call_callable(ctx, s->leaf);
        }

        s->mapped = init_list_obj();
        s->index = 0;
        s->stage = TF_TREEREC_CHILD_LOOP;
    }

    if (s->stage == TF_TREEREC_LEAF_BODY) {
        return tf_treerec_finish_single(ctx, s, done);
    }

    if (s->stage == TF_TREEREC_CHILD_DONE) {
        if (stack_len(ctx) != s->saved_len + 1) return TF_ERR;
        tf_obj *child_result = stack_pop(ctx);
        push_obj(s->mapped, child_result);
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
        s->index++;
        s->stage = TF_TREEREC_CHILD_LOOP;
        *done = false;
        return TF_OK;
    }

    if (s->stage == TF_TREEREC_CHILD_LOOP) {
        if (s->index < s->tree->list.len) {
            tf_obj *child = s->tree->list.elem[s->index];
            retain_obj(child);
            retain_obj(s->leaf);
            retain_obj(s->node);
            tf_treerec_push_owned(ctx, child, s->leaf, s->node);
            s->stage = TF_TREEREC_CHILD_DONE;
            *done = false;
            return TF_OK;
        }

        stack_push(ctx, s->mapped);
        retain_obj(s->mapped);
        s->stage = TF_TREEREC_NODE_BODY;
        *done = false;
        return tf_call_callable(ctx, s->node);
    }

    return tf_treerec_finish_single(ctx, s, done);
}

static void tf_treerec_cleanup(tf_ctx *ctx, void *state, tf_ret status) {
    tf_treerec_state *s = state;
    if (status != TF_OK) {
        tf_restore_stack_copy(ctx, s->saved_stack, s->saved_len);
    }
    tf_release_stack_copy(s->saved_stack, s->saved_len);
    release_obj(s->tree);
    release_obj(s->leaf);
    release_obj(s->node);
    if (s->mapped) release_obj(s->mapped);
    free(s);
}

static void tf_treerec_push_owned(tf_ctx *ctx, tf_obj *tree, tf_obj *leaf,
                                  tf_obj *node) {
    tf_treerec_state *state = xmalloc(sizeof(*state));
    state->tree = tree;
    state->leaf = leaf;
    state->node = node;
    state->mapped = NULL;
    state->index = 0;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->stage = TF_TREEREC_START;
    tf_save_stack_copy(ctx, &state->saved_stack, &state->saved_len);
    native_frame_push(ctx, tf_treerec_step, tf_treerec_cleanup, state);
}

tf_ret tf_error(tf_ctx *ctx) {
    if (stack_len(ctx) > 0) {
        tf_obj *msg = stack_peek(ctx, 0);
        if (msg->type == TF_OBJ_TYPE_STR) {
            msg = stack_pop(ctx);
            if (ctx->error_suppression_depth == 0) {
                tf_console_program_errorf("%s\n", msg->str.ptr);
                ctx->error_reported = true;
            }
            release_obj(msg);
            return TF_ERR;
        }
    }

    if (ctx->error_suppression_depth == 0) {
        tf_console_program_errorf("error\n");
        ctx->error_reported = true;
    }
    return TF_ERR;
}

tf_ret tf_try(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *handler = stack_peek(ctx, 0);
    tf_obj *body = stack_peek(ctx, 1);
    if (!tf_is_callable(body) || !tf_is_callable(handler)) return TF_ERR;

    handler = stack_pop(ctx);
    body = stack_pop(ctx);

    tf_try_state *state = xmalloc(sizeof(*state));
    state->body = body;
    state->handler = handler;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->stage = TF_TRY_START;
    state->suppressing_errors = false;
    tf_save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    native_frame_push_handler(ctx, tf_try_step, tf_try_cleanup, tf_try_error,
                              state);
    return TF_OK;
}

tf_ret tf_if(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *cond = stack_peek(ctx, 1);
    if (!tf_is_callable(body)) return TF_ERR;
    if (cond->type != TF_OBJ_TYPE_BOOL && !tf_is_callable(cond)) return TF_ERR;

    body = stack_pop(ctx);
    cond = stack_pop(ctx);

    tf_if_state *state = xmalloc(sizeof(*state));
    state->cond = cond;
    state->then_b = body;
    state->else_b = NULL;
    state->has_else = false;
    state->stage = TF_IF_COND;
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_if_step, tf_if_cleanup, state);
    return TF_OK;
}

tf_ret tf_infra(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *data = stack_peek(ctx, 1);
    if (!tf_is_callable(body) || data->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    data = stack_pop(ctx);

    tf_infra_state *state = xmalloc(sizeof(*state));
    state->body = body;
    state->data = data;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->scheduled = false;
    state->result = NULL;
    tf_save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    tf_clear_stack(ctx);
    for (size_t i = 0; i < data->list.len; i++) {
        stack_push(ctx, data->list.elem[i]);
        retain_obj(data->list.elem[i]);
    }

    native_frame_push(ctx, tf_infra_step, tf_infra_cleanup, state);
    return TF_OK;
}

tf_ret tf_cond(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *clauses = stack_pop_type(ctx, TF_OBJ_TYPE_LIST);
    if (!clauses) return TF_ERR;

    tf_cond_state *state = xmalloc(sizeof(*state));
    state->clauses = clauses;
    state->index = 0;
    state->stage = TF_COND_PRED;
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_cond_step, tf_cond_cleanup, state);
    return TF_OK;
}

static tf_ret tf_cleave_or_construct(tf_ctx *ctx, bool construct_result) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *branches = stack_peek(ctx, 0);
    if (branches->type != TF_OBJ_TYPE_LIST) return TF_ERR;

    branches = stack_pop(ctx);
    tf_obj *value = stack_pop(ctx);

    tf_cleave_state *state = xmalloc(sizeof(*state));
    state->branches = branches;
    state->value = value;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->outputs = init_list_obj();
    state->index = 0;
    state->awaiting_branch = false;
    state->construct_result = construct_result;
    tf_save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    native_frame_push(ctx, tf_cleave_step, tf_cleave_cleanup, state);
    return TF_OK;
}

tf_ret tf_cleave(tf_ctx *ctx) {
    return tf_cleave_or_construct(ctx, false);
}

tf_ret tf_construct(tf_ctx *ctx) {
    return tf_cleave_or_construct(ctx, true);
}

tf_ret tf_ifelse(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *else_b = stack_peek(ctx, 0);
    tf_obj *then_b = stack_peek(ctx, 1);
    tf_obj *cond = stack_peek(ctx, 2);
    if (!tf_is_callable(else_b) || !tf_is_callable(then_b)) return TF_ERR;
    if (cond->type != TF_OBJ_TYPE_BOOL && !tf_is_callable(cond)) return TF_ERR;

    else_b = stack_pop(ctx);
    then_b = stack_pop(ctx);
    cond = stack_pop(ctx);

    tf_if_state *state = xmalloc(sizeof(*state));
    state->cond = cond;
    state->then_b = then_b;
    state->else_b = else_b;
    state->has_else = true;
    state->stage = TF_IF_COND;
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_if_step, tf_if_cleanup, state);
    return TF_OK;
}

tf_ret tf_while(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *cond = stack_peek(ctx, 1);
    if (!tf_is_callable(body) || !tf_is_callable(cond)) return TF_ERR;

    body = stack_pop(ctx);
    cond = stack_pop(ctx);

    tf_while_state *state = xmalloc(sizeof(*state));
    state->cond = cond;
    state->body = body;
    state->stage = TF_WHILE_COND;
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_while_step, tf_while_cleanup, state);
    return TF_OK;
}

tf_ret tf_dip(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    if (!tf_is_callable(body)) return TF_ERR;

    body = stack_pop(ctx);
    tf_obj *saved = stack_pop(ctx);

    tf_dip_state *state = xmalloc(sizeof(*state));
    state->body = body;
    state->saved = saved;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->scheduled = false;
    tf_save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    native_frame_push(ctx, tf_dip_step, tf_dip_cleanup, state);
    return TF_OK;
}

tf_ret tf_keep(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    if (!tf_is_callable(body)) return TF_ERR;

    body = stack_pop(ctx);
    tf_obj *saved = stack_pop(ctx);

    tf_keep_state *state = xmalloc(sizeof(*state));
    state->body = body;
    state->saved = saved;
    state->saved_stack = NULL;
    state->base_len = 0;
    state->scheduled = false;
    tf_save_stack_copy(ctx, &state->saved_stack, &state->base_len);

    native_frame_push(ctx, tf_keep_step, tf_keep_cleanup, state);
    return TF_OK;
}

tf_ret tf_bi(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *right = stack_peek(ctx, 0);
    tf_obj *left = stack_peek(ctx, 1);
    if (!tf_is_callable(left) || !tf_is_callable(right)) return TF_ERR;

    right = stack_pop(ctx);
    left = stack_pop(ctx);
    tf_obj *saved = stack_pop(ctx);

    tf_bi_state *state = xmalloc(sizeof(*state));
    state->left = left;
    state->right = right;
    state->saved = saved;
    state->saved_stack = NULL;
    state->base_len = 0;
    state->stage = TF_BI_RUN_LEFT;
    state->left_outputs = NULL;
    state->left_out_len = 0;
    tf_save_stack_copy(ctx, &state->saved_stack, &state->base_len);

    native_frame_push(ctx, tf_bi_step, tf_bi_cleanup, state);
    return TF_OK;
}

tf_ret tf_linrec(tf_ctx *ctx) {
    if (stack_len(ctx) < 4) return TF_ERR;
    tf_obj *rec2 = stack_peek(ctx, 0);
    tf_obj *rec1 = stack_peek(ctx, 1);
    tf_obj *then_b = stack_peek(ctx, 2);
    tf_obj *pred = stack_peek(ctx, 3);
    if (!tf_is_callable(pred) || !tf_is_callable(then_b) ||
        !tf_is_callable(rec1) || !tf_is_callable(rec2)) {
        return TF_ERR;
    }

    rec2 = stack_pop(ctx);
    rec1 = stack_pop(ctx);
    then_b = stack_pop(ctx);
    pred = stack_pop(ctx);

    tf_linrec_state *state = xmalloc(sizeof(*state));
    state->pred = pred;
    state->then_b = then_b;
    state->rec1 = rec1;
    state->rec2 = rec2;
    state->stage = TF_LINREC_PRED;
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_linrec_step, tf_linrec_cleanup, state);
    return TF_OK;
}

tf_ret tf_binrec(tf_ctx *ctx) {
    if (stack_len(ctx) < 4) return TF_ERR;
    tf_obj *rec2 = stack_peek(ctx, 0);
    tf_obj *rec1 = stack_peek(ctx, 1);
    tf_obj *then_b = stack_peek(ctx, 2);
    tf_obj *pred = stack_peek(ctx, 3);
    if (!tf_is_callable(pred) || !tf_is_callable(then_b) ||
        !tf_is_callable(rec1) || !tf_is_callable(rec2)) {
        return TF_ERR;
    }

    rec2 = stack_pop(ctx);
    rec1 = stack_pop(ctx);
    then_b = stack_pop(ctx);
    pred = stack_pop(ctx);

    tf_binrec_state *state = xmalloc(sizeof(*state));
    state->pred = pred;
    state->then_b = then_b;
    state->rec1 = rec1;
    state->rec2 = rec2;
    state->stage = TF_BINREC_PRED;
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_binrec_step, tf_binrec_cleanup, state);
    return TF_OK;
}

static tf_ret tf_pop_rec_parts(tf_ctx *ctx, tf_obj **pred, tf_obj **then_b,
                               tf_obj **before, tf_obj **after) {
    if (stack_len(ctx) >= 4 && tf_is_callable(stack_peek(ctx, 0)) &&
        tf_is_callable(stack_peek(ctx, 1)) &&
        tf_is_callable(stack_peek(ctx, 2)) &&
        tf_is_callable(stack_peek(ctx, 3))) {
        *after = stack_pop(ctx);
        *before = stack_pop(ctx);
        *then_b = stack_pop(ctx);
        *pred = stack_pop(ctx);
        return TF_OK;
    }

    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *parts = stack_peek(ctx, 0);
    if (parts->type != TF_OBJ_TYPE_LIST || parts->list.len != 4) return TF_ERR;
    for (size_t i = 0; i < 4; i++) {
        if (!tf_is_callable(parts->list.elem[i])) return TF_ERR;
    }

    parts = stack_pop(ctx);
    *pred = parts->list.elem[0];
    *then_b = parts->list.elem[1];
    *before = parts->list.elem[2];
    *after = parts->list.elem[3];
    retain_obj(*pred);
    retain_obj(*then_b);
    retain_obj(*before);
    retain_obj(*after);
    release_obj(parts);
    return TF_OK;
}

tf_ret tf_genrec(tf_ctx *ctx) {
    tf_obj *pred = NULL;
    tf_obj *then_b = NULL;
    tf_obj *before = NULL;
    tf_obj *after = NULL;
    tf_ret res = tf_pop_rec_parts(ctx, &pred, &then_b, &before, &after);
    if (res != TF_OK) return res;

    tf_genrec_state *state = xmalloc(sizeof(*state));
    state->pred = pred;
    state->then_b = then_b;
    state->before = before;
    state->after = after;
    state->stage = TF_GENREC_PRED;
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_genrec_step, tf_genrec_cleanup, state);
    return TF_OK;
}

tf_ret tf_treerec(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *node = stack_peek(ctx, 0);
    tf_obj *leaf = stack_peek(ctx, 1);
    if (!tf_is_callable(node) || !tf_is_callable(leaf)) {
        return TF_ERR;
    }

    node = stack_pop(ctx);
    leaf = stack_pop(ctx);
    tf_obj *tree = stack_pop(ctx);

    tf_treerec_push_owned(ctx, tree, leaf, node);
    return TF_OK;
}

tf_ret tf_replicate(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *n_obj = stack_peek(ctx, 1);
    if (!tf_is_callable(body) || n_obj->type != TF_OBJ_TYPE_INT) {
        return TF_ERR;
    }
    body = stack_pop(ctx);
    n_obj = stack_pop(ctx);

    int n = n_obj->i;
    release_obj(n_obj);

    if (n < 0) {
        release_obj(body);
        return TF_ERR;
    }

    tf_replicate_state *state = xmalloc(sizeof(*state));
    state->body = body;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->remaining = n;
    state->awaiting_body = false;
    state->result = init_list_obj();
    tf_save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    native_frame_push(ctx, tf_replicate_step, tf_replicate_cleanup, state);
    return TF_OK;
}

tf_ret tf_times(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *n_obj = stack_peek(ctx, 1);
    if (!tf_is_callable(body) || n_obj->type != TF_OBJ_TYPE_INT) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    n_obj = stack_pop(ctx);

    int n = n_obj->i;
    if (n < 0) {
        release_obj(body);
        release_obj(n_obj);
        return TF_ERR;
    }

    release_obj(n_obj);
    if (n == 0) {
        release_obj(body);
        return TF_OK;
    }

    tf_times_state *state = xmalloc(sizeof(*state));
    state->body = body;
    state->remaining = n;
    native_frame_push(ctx, tf_times_step, tf_times_cleanup, state);
    return TF_OK;
}

tf_ret tf_each(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *data = stack_peek(ctx, 1);
    if (!tf_is_callable(body) || !tf_is_sequence(data)) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    data = stack_pop(ctx);

    tf_each_state *state = xmalloc(sizeof(*state));
    state->body = body;
    state->data = data;
    state->index = 0;
    native_frame_push(ctx, tf_each_step, tf_each_cleanup, state);
    return TF_OK;
}

tf_ret tf_map(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *data = stack_peek(ctx, 1);
    if (!tf_is_callable(body) || !tf_is_sequence(data)) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    data = stack_pop(ctx);

    tf_map_state *state = xmalloc(sizeof(*state));
    state->body = body;
    state->data = data;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->index = 0;
    state->awaiting_body = false;
    state->string_result = data->type == TF_OBJ_TYPE_STR;
    state->list_result = state->string_result ? NULL : init_list_obj();
    state->str_result.ptr = NULL;
    state->str_result.len = 0;
    state->str_result.cap = 0;
    if (state->string_result) tf_bytebuf_init(&state->str_result, data->str.len);
    tf_save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    native_frame_push(ctx, tf_map_step, tf_map_cleanup, state);
    return TF_OK;
}

tf_ret tf_fold(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *data = stack_peek(ctx, 1);
    if (!tf_is_callable(body) || !tf_is_sequence(data)) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    data = stack_pop(ctx);
    tf_obj *acc = stack_pop(ctx);

    tf_fold_state *state = xmalloc(sizeof(*state));
    state->body = body;
    state->data = data;
    state->saved_stack = NULL;
    state->saved_len = 0;
    state->index = 0;
    state->awaiting_body = false;
    tf_save_stack_copy(ctx, &state->saved_stack, &state->saved_len);

    stack_push(ctx, acc);
    native_frame_push(ctx, tf_fold_step, tf_fold_cleanup, state);
    return TF_OK;
}

tf_ret tf_split(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *seq = stack_peek(ctx, 1);

    if (pred->type == TF_OBJ_TYPE_STR && seq->type == TF_OBJ_TYPE_STR) {
        return tf_split_string(ctx);
    }

    if (!tf_is_callable(pred) || !tf_is_sequence(seq)) {
        return TF_ERR;
    }

    pred = stack_pop(ctx);
    seq = stack_pop(ctx);

    tf_split_state *state = xmalloc(sizeof(*state));
    state->pred = pred;
    state->seq = seq;
    state->index = 0;
    state->current = NULL;
    state->string_result = seq->type == TF_OBJ_TYPE_STR;
    state->true_list = state->string_result ? NULL : init_list_obj();
    state->false_list = state->string_result ? NULL : init_list_obj();
    state->true_str.ptr = NULL;
    state->true_str.len = 0;
    state->true_str.cap = 0;
    state->false_str.ptr = NULL;
    state->false_str.len = 0;
    state->false_str.cap = 0;
    if (state->string_result) {
        tf_bytebuf_init(&state->true_str, seq->str.len);
        tf_bytebuf_init(&state->false_str, seq->str.len);
    }
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_split_step, tf_split_cleanup, state);
    return TF_OK;
}

tf_ret tf_merge(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *l2 = stack_peek(ctx, 1);
    tf_obj *l1 = stack_peek(ctx, 2);
    if (!tf_is_callable(pred) || !tf_is_sequence(l1) ||
        l1->type != l2->type) {
        return TF_ERR;
    }
    pred = stack_pop(ctx);
    l2 = stack_pop(ctx);
    l1 = stack_pop(ctx);

    tf_merge_state *state = xmalloc(sizeof(*state));
    state->pred = pred;
    state->l1 = l1;
    state->l2 = l2;
    state->i1 = 0;
    state->i2 = 0;
    state->o1 = NULL;
    state->o2 = NULL;
    state->string_result = l1->type == TF_OBJ_TYPE_STR;
    state->list_result = state->string_result ? NULL : init_list_obj();
    state->str_result.ptr = NULL;
    state->str_result.len = 0;
    state->str_result.cap = 0;
    if (state->string_result) {
        tf_bytebuf_init(&state->str_result, l1->str.len + l2->str.len);
    }
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_merge_step, tf_merge_cleanup, state);
    return TF_OK;
}

tf_ret tf_filter(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *seq = stack_peek(ctx, 1);
    if (!tf_is_callable(pred) || !tf_is_sequence(seq)) {
        return TF_ERR;
    }

    pred = stack_pop(ctx);
    seq = stack_pop(ctx);

    tf_filter_state *state = xmalloc(sizeof(*state));
    state->pred = pred;
    state->seq = seq;
    state->index = 0;
    state->current = NULL;
    state->string_result = seq->type == TF_OBJ_TYPE_STR;
    state->list_result = state->string_result ? NULL : init_list_obj();
    state->str_result.ptr = NULL;
    state->str_result.len = 0;
    state->str_result.cap = 0;
    if (state->string_result) tf_bytebuf_init(&state->str_result, seq->str.len);
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_filter_step, tf_filter_cleanup, state);
    return TF_OK;
}

tf_ret tf_some(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *seq = stack_peek(ctx, 1);
    if (!tf_is_callable(pred) || !tf_is_sequence(seq)) {
        return TF_ERR;
    }

    pred = stack_pop(ctx);
    seq = stack_pop(ctx);

    tf_quantifier_state *state = xmalloc(sizeof(*state));
    state->pred = pred;
    state->seq = seq;
    state->index = 0;
    state->current = NULL;
    state->kind = TF_QUANT_SOME;
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_quantifier_step, tf_quantifier_cleanup, state);
    return TF_OK;
}

tf_ret tf_all(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *seq = stack_peek(ctx, 1);
    if (!tf_is_callable(pred) || !tf_is_sequence(seq)) {
        return TF_ERR;
    }

    pred = stack_pop(ctx);
    seq = stack_pop(ctx);

    tf_quantifier_state *state = xmalloc(sizeof(*state));
    state->pred = pred;
    state->seq = seq;
    state->index = 0;
    state->current = NULL;
    state->kind = TF_QUANT_ALL;
    tf_predicate_eval_init(&state->pred_eval);

    native_frame_push(ctx, tf_quantifier_step, tf_quantifier_cleanup, state);
    return TF_OK;
}
