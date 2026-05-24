#include "tf_lib.h"
#include <string.h>
#include <stdlib.h>
#include "tf_exec.h"
#include "tf_alloc.h"
#include "tf_console.h"
#include "tf_obj.h"

tf_ret tf_exec(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_peek(ctx, 0);
    if (o->type != TF_OBJ_TYPE_LIST && o->type != TF_OBJ_TYPE_SYMBOL) {
        return TF_ERR;
    }

    o = stack_pop(ctx);
    if (o->type == TF_OBJ_TYPE_LIST) {
        frame_push(ctx, o);
        release_obj(o);
        return TF_OK;
    } else if (o->type == TF_OBJ_TYPE_SYMBOL) {
        tf_ret res = call_symbol(ctx, o);
        release_obj(o);
        return res;
    } else {
        release_obj(o);
        return TF_ERR;
    }
}

tf_ret tf_app2(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *top = stack_peek(ctx, 0);
    if (top->type != TF_OBJ_TYPE_LIST && top->type != TF_OBJ_TYPE_SYMBOL) {
        return TF_ERR;
    }

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
// one-byte string so the same quotation protocol works for lists and strings.
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

// Predicate/control quotations are observers of the surrounding data stack.
// Temporary inputs are pushed before execution; the top result must be bool;
// all stack changes made by the predicate are discarded afterward.
static tf_ret tf_eval_predicate_sandbox_r(tf_ctx *ctx, tf_obj *pred,
                                          bool allow_bool, tf_obj **inputs,
                                          size_t input_len, bool *pred_val) {
    if (pred->type == TF_OBJ_TYPE_BOOL) {
        if (!allow_bool || input_len != 0) return TF_ERR;
        *pred_val = pred->b;
        return TF_OK;
    }

    if (pred->type != TF_OBJ_TYPE_LIST) return TF_ERR;

    tf_obj **saved_stack = NULL;
    size_t saved_len = 0;
    tf_save_stack_copy(ctx, &saved_stack, &saved_len);

    for (size_t i = 0; i < input_len; i++) {
        stack_push(ctx, inputs[i]);
        retain_obj(inputs[i]);
    }

    tf_ret exec_res = exec(ctx, pred);
    if (exec_res != TF_OK) {
        tf_restore_stack_owned(ctx, saved_stack, saved_len);
        free(saved_stack);
        return exec_res;
    }

    tf_obj *bool_res = stack_pop_type(ctx, TF_OBJ_TYPE_BOOL);
    if (!bool_res) {
        tf_restore_stack_owned(ctx, saved_stack, saved_len);
        free(saved_stack);
        return TF_ERR;
    }

    *pred_val = bool_res->b;
    release_obj(bool_res);

    tf_restore_stack_owned(ctx, saved_stack, saved_len);
    free(saved_stack);
    return TF_OK;
}

static tf_ret tf_eval_condition_r(tf_ctx *ctx, tf_obj *cond, bool allow_bool,
                                  bool *cond_val) {
    return tf_eval_predicate_sandbox_r(ctx, cond, allow_bool, NULL, 0,
                                       cond_val);
}

tf_ret tf_error(tf_ctx *ctx) {
    if (stack_len(ctx) > 0) {
        tf_obj *msg = stack_peek(ctx, 0);
        if (msg->type == TF_OBJ_TYPE_STR) {
            msg = stack_pop(ctx);
            if (ctx->error_suppression_depth == 0) {
                tf_console_runtime_errorf("%s\n", msg->str.ptr);
            }
            release_obj(msg);
            return TF_ERR;
        }
    }

    if (ctx->error_suppression_depth == 0) {
        tf_console_runtime_errorf("error\n");
    }
    return TF_ERR;
}

tf_ret tf_try_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *handler = stack_peek(ctx, 0);
    tf_obj *body = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST || handler->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    handler = stack_pop(ctx);
    body = stack_pop(ctx);

    tf_obj **saved_stack = NULL;
    size_t saved_len = 0;
    tf_save_stack_copy(ctx, &saved_stack, &saved_len);

    ctx->error_suppression_depth++;
    tf_ret res = exec(ctx, body);
    ctx->error_suppression_depth--;
    if (res == TF_ERR) {
        tf_restore_stack_copy(ctx, saved_stack, saved_len);
        res = exec(ctx, handler);
    } else if (res == TF_INTERRUPTED) {
        tf_restore_stack_copy(ctx, saved_stack, saved_len);
    }

    tf_release_stack_copy(saved_stack, saved_len);
    release_obj(body);
    release_obj(handler);
    return res;
}

tf_ret tf_if_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *cond = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST) return TF_ERR;
    if (cond->type != TF_OBJ_TYPE_BOOL && cond->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    cond = stack_pop(ctx);

    bool cond_val = false;
    tf_ret eval_res = tf_eval_condition_r(ctx, cond, true, &cond_val);
    if (eval_res != TF_OK) {
        release_obj(body);
        release_obj(cond);
        return eval_res;
    }

    tf_ret final_res = TF_OK;
    if (cond_val) { final_res = exec(ctx, body); }

    release_obj(body);
    release_obj(cond);
    return final_res;
}

tf_ret tf_infra_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *data = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST || data->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    data = stack_pop(ctx);

    tf_obj **saved_stack = NULL;
    size_t saved_len = 0;
    tf_save_stack_copy(ctx, &saved_stack, &saved_len);

    tf_clear_stack(ctx);
    for (size_t i = 0; i < data->list.len; i++) {
        stack_push(ctx, data->list.elem[i]);
        retain_obj(data->list.elem[i]);
    }

    tf_ret res = exec(ctx, body);
    tf_obj *result = NULL;
    if (res == TF_OK) {
        result = init_list_obj();
        res = tf_collect_outputs(ctx, 0, result);
    }

    tf_restore_stack_copy(ctx, saved_stack, saved_len);
    if (res == TF_OK) {
        stack_push(ctx, result);
    } else if (result) {
        release_obj(result);
    }

    tf_release_stack_copy(saved_stack, saved_len);
    release_obj(body);
    release_obj(data);
    return res;
}

tf_ret tf_cond_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *clauses = stack_pop_type(ctx, TF_OBJ_TYPE_LIST);
    if (!clauses) return TF_ERR;

    tf_ret res = TF_OK;
    for (size_t i = 0; i < clauses->list.len; i++) {
        tf_obj *clause = clauses->list.elem[i];
        if (clause->type != TF_OBJ_TYPE_LIST || clause->list.len != 2) {
            res = TF_ERR;
            break;
        }

        tf_obj *pred = clause->list.elem[0];
        tf_obj *body = clause->list.elem[1];
        if (body->type != TF_OBJ_TYPE_LIST) {
            res = TF_ERR;
            break;
        }

        bool cond_val = false;
        res = tf_eval_condition_r(ctx, pred, true, &cond_val);
        if (res != TF_OK) break;
        if (cond_val) {
            res = exec(ctx, body);
            break;
        }
    }

    release_obj(clauses);
    return res;
}

static tf_ret tf_cleave_or_construct_r(tf_ctx *ctx, bool construct_result) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *branches = stack_peek(ctx, 0);
    if (branches->type != TF_OBJ_TYPE_LIST) return TF_ERR;

    branches = stack_pop(ctx);
    tf_obj *value = stack_pop(ctx);

    tf_obj **saved_stack = NULL;
    size_t saved_len = 0;
    tf_save_stack_copy(ctx, &saved_stack, &saved_len);

    tf_obj *outputs = init_list_obj();
    tf_ret res = TF_OK;
    for (size_t i = 0; i < branches->list.len; i++) {
        tf_obj *branch = branches->list.elem[i];
        if (branch->type != TF_OBJ_TYPE_LIST) {
            res = TF_ERR;
            break;
        }

        tf_restore_stack_copy(ctx, saved_stack, saved_len);
        stack_push(ctx, value);
        retain_obj(value);

        res = exec(ctx, branch);
        if (res != TF_OK) break;

        res = tf_collect_outputs(ctx, saved_len, outputs);
        if (res != TF_OK) break;
    }

    tf_restore_stack_copy(ctx, saved_stack, saved_len);
    if (res == TF_OK) {
        if (construct_result) {
            stack_push(ctx, outputs);
        } else {
            for (size_t i = 0; i < outputs->list.len; i++) {
                stack_push(ctx, outputs->list.elem[i]);
                retain_obj(outputs->list.elem[i]);
            }
            release_obj(outputs);
        }
    } else {
        release_obj(outputs);
    }

    tf_release_stack_copy(saved_stack, saved_len);
    release_obj(value);
    release_obj(branches);
    return res;
}

tf_ret tf_cleave_r(tf_ctx *ctx) {
    return tf_cleave_or_construct_r(ctx, false);
}

tf_ret tf_construct_r(tf_ctx *ctx) {
    return tf_cleave_or_construct_r(ctx, true);
}

tf_ret tf_ifelse_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *else_b = stack_peek(ctx, 0);
    tf_obj *then_b = stack_peek(ctx, 1);
    tf_obj *cond = stack_peek(ctx, 2);
    if (else_b->type != TF_OBJ_TYPE_LIST || then_b->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }
    if (cond->type != TF_OBJ_TYPE_BOOL && cond->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    else_b = stack_pop(ctx);
    then_b = stack_pop(ctx);
    cond = stack_pop(ctx);

    bool cond_val = false;
    tf_ret eval_res = tf_eval_condition_r(ctx, cond, true, &cond_val);
    if (eval_res != TF_OK) {
        release_obj(else_b);
        release_obj(then_b);
        release_obj(cond);
        return eval_res;
    }

    tf_ret final_res = TF_OK;
    if (cond_val) {
        final_res = exec(ctx, then_b);
    } else {
        final_res = exec(ctx, else_b);
    }

    release_obj(else_b);
    release_obj(then_b);
    release_obj(cond);
    return final_res;
}

tf_ret tf_while_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *cond = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST || cond->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    cond = stack_pop(ctx);

    tf_ret final_res = TF_OK;
    while (1) {
        bool continue_loop = false;
        tf_ret eval_res = tf_eval_condition_r(ctx, cond, false, &continue_loop);
        if (eval_res != TF_OK) {
            final_res = eval_res;
            break;
        }

        if (!continue_loop) break;

        tf_ret exec_res = exec(ctx, body);
        if (exec_res != TF_OK) {
            final_res = exec_res;
            break;
        }
    }

    release_obj(body);
    release_obj(cond);
    return final_res;
}

tf_ret tf_dip_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    if (body->type != TF_OBJ_TYPE_LIST) return TF_ERR;

    body = stack_pop(ctx);
    tf_obj *saved = stack_pop(ctx);

    tf_obj **saved_stack = NULL;
    size_t saved_len = 0;
    tf_save_stack_copy(ctx, &saved_stack, &saved_len);

    tf_ret res = exec(ctx, body);
    if (res != TF_OK) { tf_restore_stack_copy(ctx, saved_stack, saved_len); }
    stack_push(ctx, saved);

    tf_release_stack_copy(saved_stack, saved_len);
    release_obj(body);
    return res;
}

tf_ret tf_keep_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    if (body->type != TF_OBJ_TYPE_LIST) return TF_ERR;

    body = stack_pop(ctx);
    tf_obj *saved = stack_pop(ctx);
    tf_obj **saved_stack = NULL;
    size_t base_len = 0;
    tf_save_stack_copy(ctx, &saved_stack, &base_len);

    retain_obj(saved);
    stack_push(ctx, saved);

    tf_ret res = exec(ctx, body);
    if (res != TF_OK) {
        tf_restore_stack_copy(ctx, saved_stack, base_len);
        stack_push(ctx, saved);
        tf_release_stack_copy(saved_stack, base_len);
        release_obj(body);
        return res;
    }

    if (stack_len(ctx) < base_len) {
        tf_restore_stack_copy(ctx, saved_stack, base_len);
        stack_push(ctx, saved);
        tf_release_stack_copy(saved_stack, base_len);
        release_obj(body);
        return TF_ERR;
    }

    size_t out_len = stack_len(ctx) - base_len;
    tf_obj **outputs = out_len > 0 ? xmalloc(sizeof(tf_obj *) * out_len) : NULL;
    for (size_t i = out_len; i > 0; i--) outputs[i - 1] = stack_pop(ctx);

    stack_push(ctx, saved);
    for (size_t i = 0; i < out_len; i++) stack_push(ctx, outputs[i]);

    free(outputs);
    tf_release_stack_copy(saved_stack, base_len);
    release_obj(body);
    return res;
}

tf_ret tf_bi_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *right = stack_peek(ctx, 0);
    tf_obj *left = stack_peek(ctx, 1);
    if (left->type != TF_OBJ_TYPE_LIST || right->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    right = stack_pop(ctx);
    left = stack_pop(ctx);
    tf_obj *saved = stack_pop(ctx);
    tf_obj **saved_stack = NULL;
    size_t base_len = 0;
    tf_save_stack_copy(ctx, &saved_stack, &base_len);

    retain_obj(saved);
    stack_push(ctx, saved);
    tf_ret left_res = exec(ctx, left);
    if (stack_len(ctx) < base_len) {
        tf_restore_stack_copy(ctx, saved_stack, base_len);
        stack_push(ctx, saved);
        tf_release_stack_copy(saved_stack, base_len);
        release_obj(right);
        release_obj(left);
        return TF_ERR;
    }
    if (left_res != TF_OK) {
        tf_restore_stack_copy(ctx, saved_stack, base_len);
        stack_push(ctx, saved);
        tf_release_stack_copy(saved_stack, base_len);
        release_obj(right);
        release_obj(left);
        return left_res;
    }

    size_t left_out_len = stack_len(ctx) - base_len;
    tf_obj **left_outputs =
        left_out_len > 0 ? xmalloc(sizeof(tf_obj *) * left_out_len) : NULL;
    for (size_t i = left_out_len; i > 0; i--) {
        left_outputs[i - 1] = stack_pop(ctx);
    }

    retain_obj(saved);
    stack_push(ctx, saved);
    tf_ret right_res = exec(ctx, right);
    if (right_res != TF_OK) {
        tf_restore_stack_copy(ctx, saved_stack, base_len);
        stack_push(ctx, saved);
        for (size_t i = 0; i < left_out_len; i++) release_obj(left_outputs[i]);
        free(left_outputs);
        tf_release_stack_copy(saved_stack, base_len);
        release_obj(right);
        release_obj(left);
        return right_res;
    }
    if (stack_len(ctx) < base_len) {
        tf_restore_stack_copy(ctx, saved_stack, base_len);
        stack_push(ctx, saved);
        for (size_t i = 0; i < left_out_len; i++) release_obj(left_outputs[i]);
        free(left_outputs);
        tf_release_stack_copy(saved_stack, base_len);
        release_obj(right);
        release_obj(left);
        return TF_ERR;
    }

    size_t right_out_len = stack_len(ctx) - base_len;
    tf_obj **right_outputs =
        right_out_len > 0 ? xmalloc(sizeof(tf_obj *) * right_out_len) : NULL;
    for (size_t i = right_out_len; i > 0; i--) {
        right_outputs[i - 1] = stack_pop(ctx);
    }

    for (size_t i = 0; i < left_out_len; i++) stack_push(ctx, left_outputs[i]);
    for (size_t i = 0; i < right_out_len; i++) stack_push(ctx, right_outputs[i]);

    free(right_outputs);
    free(left_outputs);
    tf_release_stack_copy(saved_stack, base_len);
    release_obj(right);
    release_obj(left);
    release_obj(saved);

    return right_res;
}

tf_ret tf_linrec_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 4) return TF_ERR;
    tf_obj *rec2 = stack_peek(ctx, 0);
    tf_obj *rec1 = stack_peek(ctx, 1);
    tf_obj *then_b = stack_peek(ctx, 2);
    tf_obj *pred = stack_peek(ctx, 3);
    if (pred->type != TF_OBJ_TYPE_LIST || then_b->type != TF_OBJ_TYPE_LIST ||
        rec1->type != TF_OBJ_TYPE_LIST || rec2->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    rec2 = stack_pop(ctx);
    rec1 = stack_pop(ctx);
    then_b = stack_pop(ctx);
    pred = stack_pop(ctx);

    bool done = false;
    tf_ret res = tf_eval_condition_r(ctx, pred, false, &done);
    if (res == TF_OK) {
        if (done) {
            res = exec(ctx, then_b);
        } else {
            res = exec(ctx, rec1);
            if (res == TF_OK) {
                tf_obj *cont = init_list_obj();
                tf_obj *linrec_sym = create_symbol_obj("linrec", 6);
                tf_obj *exec_sym = create_symbol_obj("exec", 4);

                push_retained(cont, pred);
                push_retained(cont, then_b);
                push_retained(cont, rec1);
                push_retained(cont, rec2);
                push_obj(cont, linrec_sym);
                push_retained(cont, rec2);
                push_obj(cont, exec_sym);

                frame_push(ctx, cont);
                release_obj(cont);
            }
        }
    }

    release_obj(rec2);
    release_obj(rec1);
    release_obj(then_b);
    release_obj(pred);
    return res;
}

tf_ret tf_binrec_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 4) return TF_ERR;
    tf_obj *rec2 = stack_peek(ctx, 0);
    tf_obj *rec1 = stack_peek(ctx, 1);
    tf_obj *then_b = stack_peek(ctx, 2);
    tf_obj *pred = stack_peek(ctx, 3);
    if (pred->type != TF_OBJ_TYPE_LIST || then_b->type != TF_OBJ_TYPE_LIST ||
        rec1->type != TF_OBJ_TYPE_LIST || rec2->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    rec2 = stack_pop(ctx);
    rec1 = stack_pop(ctx);
    then_b = stack_pop(ctx);
    pred = stack_pop(ctx);

    bool done = false;
    tf_ret res = tf_eval_condition_r(ctx, pred, false, &done);
    if (res == TF_OK) {
        if (done) {
            res = exec(ctx, then_b);
        } else {
            res = exec(ctx, rec1);
            if (res == TF_OK) {
                tf_obj *rec_call = init_list_obj();
                tf_obj *binrec_sym = create_symbol_obj("binrec", 6);
                push_retained(rec_call, pred);
                push_retained(rec_call, then_b);
                push_retained(rec_call, rec1);
                push_retained(rec_call, rec2);
                push_obj(rec_call, binrec_sym);

                tf_obj *cont = init_list_obj();
                tf_obj *dip_sym = create_symbol_obj("dip", 3);
                tf_obj *exec_rec_sym = create_symbol_obj("exec", 4);
                tf_obj *exec_combine_sym = create_symbol_obj("exec", 4);

                push_retained(cont, rec_call);
                push_obj(cont, dip_sym);
                push_retained(cont, rec_call);
                push_obj(cont, exec_rec_sym);
                push_retained(cont, rec2);
                push_obj(cont, exec_combine_sym);

                frame_push(ctx, cont);
                release_obj(cont);
                release_obj(rec_call);
            }
        }
    }

    release_obj(rec2);
    release_obj(rec1);
    release_obj(then_b);
    release_obj(pred);
    return res;
}

static bool tf_is_quotation(tf_obj *o) {
    return o->type == TF_OBJ_TYPE_LIST;
}

static tf_ret tf_pop_rec_parts(tf_ctx *ctx, tf_obj **pred, tf_obj **then_b,
                               tf_obj **before, tf_obj **after) {
    if (stack_len(ctx) >= 4 && tf_is_quotation(stack_peek(ctx, 0)) &&
        tf_is_quotation(stack_peek(ctx, 1)) &&
        tf_is_quotation(stack_peek(ctx, 2)) &&
        tf_is_quotation(stack_peek(ctx, 3))) {
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
        if (!tf_is_quotation(parts->list.elem[i])) return TF_ERR;
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

tf_ret tf_genrec_r(tf_ctx *ctx) {
    tf_obj *pred = NULL;
    tf_obj *then_b = NULL;
    tf_obj *before = NULL;
    tf_obj *after = NULL;
    tf_ret res = tf_pop_rec_parts(ctx, &pred, &then_b, &before, &after);
    if (res != TF_OK) return res;

    bool done = false;
    res = tf_eval_condition_r(ctx, pred, false, &done);
    if (res == TF_OK) {
        if (done) {
            res = exec(ctx, then_b);
        } else {
            res = exec(ctx, before);
            if (res == TF_OK) {
                tf_obj *cont = init_list_obj();
                tf_obj *genrec_sym = create_symbol_obj("genrec", 6);
                tf_obj *exec_sym = create_symbol_obj("exec", 4);

                push_retained(cont, pred);
                push_retained(cont, then_b);
                push_retained(cont, before);
                push_retained(cont, after);
                push_obj(cont, genrec_sym);
                push_retained(cont, after);
                push_obj(cont, exec_sym);

                frame_push(ctx, cont);
                release_obj(cont);
            }
        }
    }

    release_obj(after);
    release_obj(before);
    release_obj(then_b);
    release_obj(pred);
    return res;
}

static tf_ret tf_exec_single_output_r(tf_ctx *ctx, tf_obj *input, tf_obj *body,
                                      tf_obj **out) {
    tf_obj **saved_stack = NULL;
    size_t saved_len = 0;
    tf_save_stack_copy(ctx, &saved_stack, &saved_len);

    stack_push(ctx, input);
    retain_obj(input);

    tf_ret res = exec(ctx, body);
    if (res == TF_OK) {
        if (stack_len(ctx) != saved_len + 1) {
            res = TF_ERR;
        } else {
            *out = stack_pop(ctx);
        }
    }

    tf_restore_stack_copy(ctx, saved_stack, saved_len);
    tf_release_stack_copy(saved_stack, saved_len);
    return res;
}

static tf_ret tf_treerec_eval_r(tf_ctx *ctx, tf_obj *tree, tf_obj *leaf,
                                tf_obj *node, tf_obj **out) {
    if (tree->type != TF_OBJ_TYPE_LIST) {
        return tf_exec_single_output_r(ctx, tree, leaf, out);
    }

    tf_obj *mapped = init_list_obj();
    for (size_t i = 0; i < tree->list.len; i++) {
        tf_obj *child = NULL;
        tf_ret res = tf_treerec_eval_r(ctx, tree->list.elem[i], leaf, node, &child);
        if (res != TF_OK) {
            release_obj(mapped);
            return res;
        }
        push_obj(mapped, child);
    }

    tf_ret res = tf_exec_single_output_r(ctx, mapped, node, out);
    release_obj(mapped);
    return res;
}

tf_ret tf_treerec_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *node = stack_peek(ctx, 0);
    tf_obj *leaf = stack_peek(ctx, 1);
    if (node->type != TF_OBJ_TYPE_LIST || leaf->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    node = stack_pop(ctx);
    leaf = stack_pop(ctx);
    tf_obj *tree = stack_pop(ctx);

    tf_obj *result = NULL;
    tf_ret res = tf_treerec_eval_r(ctx, tree, leaf, node, &result);
    if (res == TF_OK) stack_push(ctx, result);

    release_obj(tree);
    release_obj(leaf);
    release_obj(node);
    return res;
}

tf_ret tf_replicate_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *n_obj = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST || n_obj->type != TF_OBJ_TYPE_INT) {
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

    tf_obj **saved_stack = NULL;
    size_t saved_len = 0;
    tf_save_stack_copy(ctx, &saved_stack, &saved_len);

    tf_obj *res = init_list_obj();
    tf_ret ret = TF_OK;

    for (int i = 0; i < n; i++) {
        ret = exec(ctx, body);
        if (ret != TF_OK) break;
        if (stack_len(ctx) != saved_len + 1) {
            ret = TF_ERR;
            break;
        }
        tf_obj *item = stack_pop(ctx);
        push_obj(res, item);
        tf_restore_stack_copy(ctx, saved_stack, saved_len);
    }

    if (ret == TF_OK) {
        stack_push(ctx, res);
    } else {
        tf_restore_stack_copy(ctx, saved_stack, saved_len);
        release_obj(res);
    }

    tf_release_stack_copy(saved_stack, saved_len);
    release_obj(body);
    return ret;
}

tf_ret tf_times_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *n_obj = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST || n_obj->type != TF_OBJ_TYPE_INT) {
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

    tf_ret res = TF_OK;
    for (int i = 0; i < n; i++) {
        res = exec(ctx, body);
        if (res != TF_OK) break;
    }

    release_obj(body);
    release_obj(n_obj);
    return res;
}

tf_ret tf_each_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *data = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST || !tf_is_sequence(data)) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    data = stack_pop(ctx);

    tf_ret res = TF_OK;
    size_t len = tf_sequence_len(data);
    for (size_t i = 0; i < len; i++) {
        stack_push(ctx, tf_sequence_item_owned(data, i));
        res = exec(ctx, body);
        if (res != TF_OK) break;
    }

    release_obj(body);
    release_obj(data);
    return res;
}

tf_ret tf_map_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *data = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST || !tf_is_sequence(data)) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    data = stack_pop(ctx);

    tf_obj **saved_stack = NULL;
    size_t saved_len = 0;
    tf_save_stack_copy(ctx, &saved_stack, &saved_len);

    bool string_result = data->type == TF_OBJ_TYPE_STR;
    tf_obj *list_result = string_result ? NULL : init_list_obj();
    tf_bytebuf str_result;
    if (string_result) tf_bytebuf_init(&str_result, data->str.len);

    tf_ret res = TF_OK;
    size_t len = tf_sequence_len(data);
    for (size_t i = 0; i < len; i++) {
        stack_push(ctx, tf_sequence_item_owned(data, i));

        res = exec(ctx, body);
        if (res != TF_OK) break;

        if (stack_len(ctx) != saved_len + 1) {
            res = TF_ERR;
            break;
        }

        tf_obj *mapped = stack_pop(ctx);
        if (string_result) {
            if (!tf_is_char_string(mapped)) {
                release_obj(mapped);
                res = TF_ERR;
                break;
            }
            tf_bytebuf_append(&str_result, mapped->str.ptr[0]);
            release_obj(mapped);
        } else {
            push_obj(list_result, mapped);
        }
        tf_restore_stack_copy(ctx, saved_stack, saved_len);
    }

    if (res != TF_OK) {
        tf_restore_stack_copy(ctx, saved_stack, saved_len);
        if (string_result) {
            free(str_result.ptr);
        } else {
            release_obj(list_result);
        }
    } else {
        if (string_result) {
            stack_push(ctx, tf_bytebuf_to_string(&str_result));
            free(str_result.ptr);
        } else {
            stack_push(ctx, list_result);
        }
    }

    tf_release_stack_copy(saved_stack, saved_len);
    release_obj(body);
    release_obj(data);
    return res;
}

tf_ret tf_fold_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *data = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST || !tf_is_sequence(data)) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    data = stack_pop(ctx);
    tf_obj *acc = stack_pop(ctx);

    tf_obj **saved_stack = NULL;
    size_t saved_len = 0;
    tf_save_stack_copy(ctx, &saved_stack, &saved_len);

    stack_push(ctx, acc);
    tf_ret res = TF_OK;
    size_t len = tf_sequence_len(data);
    for (size_t i = 0; i < len; i++) {
        stack_push(ctx, tf_sequence_item_owned(data, i));

        res = exec(ctx, body);
        if (res != TF_OK) break;

        if (stack_len(ctx) != saved_len + 1) {
            res = TF_ERR;
            break;
        }
    }

    if (res != TF_OK) { tf_restore_stack_copy(ctx, saved_stack, saved_len); }

    tf_release_stack_copy(saved_stack, saved_len);
    release_obj(body);
    release_obj(data);
    return res;
}

tf_ret tf_split_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *seq = stack_peek(ctx, 1);

    if (pred->type == TF_OBJ_TYPE_STR && seq->type == TF_OBJ_TYPE_STR) {
        return tf_split_string(ctx);
    }

    if (pred->type != TF_OBJ_TYPE_LIST || !tf_is_sequence(seq)) {
        return TF_ERR;
    }

    pred = stack_pop(ctx);
    seq = stack_pop(ctx);

    bool string_result = seq->type == TF_OBJ_TYPE_STR;
    tf_obj *true_list = string_result ? NULL : init_list_obj();
    tf_obj *false_list = string_result ? NULL : init_list_obj();
    tf_bytebuf true_str;
    tf_bytebuf false_str;
    if (string_result) {
        tf_bytebuf_init(&true_str, seq->str.len);
        tf_bytebuf_init(&false_str, seq->str.len);
    }
    tf_ret res = TF_OK;

    size_t len = tf_sequence_len(seq);
    for (size_t i = 0; i < len; i++) {
        tf_obj *elem = tf_sequence_item_owned(seq, i);
        tf_obj *inputs[] = { elem };
        bool pred_val = false;
        res = tf_eval_predicate_sandbox_r(ctx, pred, false, inputs, 1,
                                          &pred_val);
        if (res != TF_OK) {
            release_obj(elem);
            break;
        }

        if (string_result) {
            tf_bytebuf_append(pred_val ? &true_str : &false_str,
                              elem->str.ptr[0]);
            release_obj(elem);
        } else {
            tf_obj *target = pred_val ? true_list : false_list;
            push_obj(target, elem);
        }
    }

    if (res != TF_OK) {
        if (string_result) {
            free(true_str.ptr);
            free(false_str.ptr);
        } else {
            release_obj(true_list);
            release_obj(false_list);
        }
    } else {
        if (string_result) {
            stack_push(ctx, tf_bytebuf_to_string(&true_str));
            stack_push(ctx, tf_bytebuf_to_string(&false_str));
            free(true_str.ptr);
            free(false_str.ptr);
        } else {
            stack_push(ctx, true_list);
            stack_push(ctx, false_list);
        }
    }

    release_obj(pred);
    release_obj(seq);
    return res;
}

tf_ret tf_merge_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *l2 = stack_peek(ctx, 1);
    tf_obj *l1 = stack_peek(ctx, 2);
    if (pred->type != TF_OBJ_TYPE_LIST || !tf_is_sequence(l1) ||
        l1->type != l2->type) {
        return TF_ERR;
    }
    pred = stack_pop(ctx);
    l2 = stack_pop(ctx);
    l1 = stack_pop(ctx);

    bool string_result = l1->type == TF_OBJ_TYPE_STR;
    tf_obj *list_res = string_result ? NULL : init_list_obj();
    tf_bytebuf str_res;
    if (string_result) {
        tf_bytebuf_init(&str_res, l1->str.len + l2->str.len);
    }
    size_t i1 = 0, i2 = 0;
    tf_ret ret = TF_OK;
    size_t l1_len = tf_sequence_len(l1);
    size_t l2_len = tf_sequence_len(l2);

    while (i1 < l1_len && i2 < l2_len) {
        tf_obj *o1 = tf_sequence_item_owned(l1, i1);
        tf_obj *o2 = tf_sequence_item_owned(l2, i2);
        tf_obj *inputs[] = { o1, o2 };
        bool take_left = false;
        ret = tf_eval_predicate_sandbox_r(ctx, pred, false, inputs, 2,
                                          &take_left);
        release_obj(o1);
        release_obj(o2);
        if (ret != TF_OK) break;

        if (take_left) {
            if (string_result) {
                tf_bytebuf_append(&str_res, l1->str.ptr[i1]);
            } else {
                push_retained(list_res, l1->list.elem[i1]);
            }
            i1++;
        } else {
            if (string_result) {
                tf_bytebuf_append(&str_res, l2->str.ptr[i2]);
            } else {
                push_retained(list_res, l2->list.elem[i2]);
            }
            i2++;
        }
    }

    if (ret == TF_OK) {
        while (i1 < l1_len) {
            if (string_result) {
                tf_bytebuf_append(&str_res, l1->str.ptr[i1]);
            } else {
                push_retained(list_res, l1->list.elem[i1]);
            }
            i1++;
        }
        while (i2 < l2_len) {
            if (string_result) {
                tf_bytebuf_append(&str_res, l2->str.ptr[i2]);
            } else {
                push_retained(list_res, l2->list.elem[i2]);
            }
            i2++;
        }

        if (string_result) {
            stack_push(ctx, tf_bytebuf_to_string(&str_res));
            free(str_res.ptr);
        } else {
            stack_push(ctx, list_res);
        }
    } else {
        if (string_result) {
            free(str_res.ptr);
        } else {
            release_obj(list_res);
        }
    }

    release_obj(pred);
    release_obj(l1);
    release_obj(l2);
    return ret;
}

static tf_ret tf_eval_item_predicate_r(tf_ctx *ctx, tf_obj *pred, tf_obj *item,
                                       bool *cond_val) {
    tf_obj *inputs[] = { item };
    return tf_eval_predicate_sandbox_r(ctx, pred, false, inputs, 1, cond_val);
}

tf_ret tf_filter_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *seq = stack_peek(ctx, 1);
    if (pred->type != TF_OBJ_TYPE_LIST || !tf_is_sequence(seq)) {
        return TF_ERR;
    }

    pred = stack_pop(ctx);
    seq = stack_pop(ctx);

    bool string_result = seq->type == TF_OBJ_TYPE_STR;
    tf_obj *list_result = string_result ? NULL : init_list_obj();
    tf_bytebuf str_result;
    if (string_result) tf_bytebuf_init(&str_result, seq->str.len);
    tf_ret res = TF_OK;
    size_t len = tf_sequence_len(seq);
    for (size_t i = 0; i < len; i++) {
        tf_obj *item = tf_sequence_item_owned(seq, i);
        bool cond_val = false;
        res = tf_eval_item_predicate_r(ctx, pred, item, &cond_val);
        if (res != TF_OK) {
            release_obj(item);
            break;
        }
        if (cond_val) {
            if (string_result) {
                tf_bytebuf_append(&str_result, item->str.ptr[0]);
                release_obj(item);
            } else {
                push_obj(list_result, item);
            }
        } else {
            release_obj(item);
        }
    }

    if (res == TF_OK) {
        if (string_result) {
            stack_push(ctx, tf_bytebuf_to_string(&str_result));
            free(str_result.ptr);
        } else {
            stack_push(ctx, list_result);
        }
    } else {
        if (string_result) {
            free(str_result.ptr);
        } else {
            release_obj(list_result);
        }
    }

    release_obj(pred);
    release_obj(seq);
    return res;
}

tf_ret tf_some_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *seq = stack_peek(ctx, 1);
    if (pred->type != TF_OBJ_TYPE_LIST || !tf_is_sequence(seq)) {
        return TF_ERR;
    }

    pred = stack_pop(ctx);
    seq = stack_pop(ctx);

    bool found = false;
    tf_ret res = TF_OK;
    size_t len = tf_sequence_len(seq);
    for (size_t i = 0; i < len; i++) {
        tf_obj *item = tf_sequence_item_owned(seq, i);
        bool cond_val = false;
        res = tf_eval_item_predicate_r(ctx, pred, item, &cond_val);
        release_obj(item);
        if (res != TF_OK) break;
        if (cond_val) {
            found = true;
            break;
        }
    }

    if (res == TF_OK) stack_push(ctx, create_bool_obj(found));

    release_obj(pred);
    release_obj(seq);
    return res;
}

tf_ret tf_all_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *seq = stack_peek(ctx, 1);
    if (pred->type != TF_OBJ_TYPE_LIST || !tf_is_sequence(seq)) {
        return TF_ERR;
    }

    pred = stack_pop(ctx);
    seq = stack_pop(ctx);

    bool all_match = true;
    tf_ret res = TF_OK;
    size_t len = tf_sequence_len(seq);
    for (size_t i = 0; i < len; i++) {
        tf_obj *item = tf_sequence_item_owned(seq, i);
        bool cond_val = false;
        res = tf_eval_item_predicate_r(ctx, pred, item, &cond_val);
        release_obj(item);
        if (res != TF_OK) break;
        if (!cond_val) {
            all_match = false;
            break;
        }
    }

    if (res == TF_OK) stack_push(ctx, create_bool_obj(all_match));

    release_obj(pred);
    release_obj(seq);
    return res;
}
