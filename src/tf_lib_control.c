#include "tf_lib.h"
#include <string.h>
#include <stdlib.h>
#include "tf_exec.h"
#include "tf_alloc.h"
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

static void tf_restore_stack(tf_ctx *ctx, tf_obj **saved_stack, size_t saved_len) {
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

static tf_ret tf_eval_condition_r(tf_ctx *ctx, tf_obj *cond, bool allow_bool,
                                  bool *cond_val) {
    if (cond->type == TF_OBJ_TYPE_BOOL) {
        if (!allow_bool) return TF_ERR;
        *cond_val = cond->b;
        return TF_OK;
    }

    if (cond->type != TF_OBJ_TYPE_LIST) return TF_ERR;

    size_t saved_len = stack_len(ctx);
    tf_obj **saved_stack =
        saved_len > 0 ? xmalloc(sizeof(tf_obj *) * saved_len) : NULL;
    for (size_t i = 0; i < saved_len; i++) {
        saved_stack[i] = stack_peek(ctx, saved_len - 1 - i);
        retain_obj(saved_stack[i]);
    }

    tf_ret exec_res = exec(ctx, cond);
    if (exec_res != TF_OK) {
        tf_restore_stack(ctx, saved_stack, saved_len);
        free(saved_stack);
        return exec_res;
    }

    tf_obj *bool_res = stack_pop_type(ctx, TF_OBJ_TYPE_BOOL);
    if (!bool_res) {
        tf_restore_stack(ctx, saved_stack, saved_len);
        free(saved_stack);
        return TF_ERR;
    }

    *cond_val = bool_res->b;
    release_obj(bool_res);

    tf_restore_stack(ctx, saved_stack, saved_len);
    free(saved_stack);
    return TF_OK;
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

    size_t saved_len = stack_len(ctx);
    tf_obj **saved_stack =
        saved_len > 0 ? xmalloc(sizeof(tf_obj *) * saved_len) : NULL;
    for (size_t i = 0; i < saved_len; i++) {
        saved_stack[i] = stack_peek(ctx, saved_len - 1 - i);
        retain_obj(saved_stack[i]);
    }

    tf_ret res = exec(ctx, body);
    if (res != TF_OK) { tf_restore_stack_copy(ctx, saved_stack, saved_len); }
    stack_push(ctx, saved);

    for (size_t i = 0; i < saved_len; i++) release_obj(saved_stack[i]);
    free(saved_stack);
    release_obj(body);
    return res;
}

tf_ret tf_keep_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    if (body->type != TF_OBJ_TYPE_LIST) return TF_ERR;

    body = stack_pop(ctx);
    tf_obj *saved = stack_pop(ctx);
    size_t base_len = stack_len(ctx);
    tf_obj **saved_stack =
        base_len > 0 ? xmalloc(sizeof(tf_obj *) * base_len) : NULL;
    for (size_t i = 0; i < base_len; i++) {
        saved_stack[i] = stack_peek(ctx, base_len - 1 - i);
        retain_obj(saved_stack[i]);
    }

    retain_obj(saved);
    stack_push(ctx, saved);

    tf_ret res = exec(ctx, body);
    if (res != TF_OK) {
        tf_restore_stack_copy(ctx, saved_stack, base_len);
        stack_push(ctx, saved);
        for (size_t i = 0; i < base_len; i++) release_obj(saved_stack[i]);
        free(saved_stack);
        release_obj(body);
        return res;
    }

    if (stack_len(ctx) < base_len) {
        tf_restore_stack_copy(ctx, saved_stack, base_len);
        stack_push(ctx, saved);
        for (size_t i = 0; i < base_len; i++) release_obj(saved_stack[i]);
        free(saved_stack);
        release_obj(body);
        return TF_ERR;
    }

    size_t out_len = stack_len(ctx) - base_len;
    tf_obj **outputs = out_len > 0 ? xmalloc(sizeof(tf_obj *) * out_len) : NULL;
    for (size_t i = out_len; i > 0; i--) outputs[i - 1] = stack_pop(ctx);

    stack_push(ctx, saved);
    for (size_t i = 0; i < out_len; i++) stack_push(ctx, outputs[i]);

    free(outputs);
    for (size_t i = 0; i < base_len; i++) release_obj(saved_stack[i]);
    free(saved_stack);
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
    size_t base_len = stack_len(ctx);
    tf_obj **saved_stack =
        base_len > 0 ? xmalloc(sizeof(tf_obj *) * base_len) : NULL;
    for (size_t i = 0; i < base_len; i++) {
        saved_stack[i] = stack_peek(ctx, base_len - 1 - i);
        retain_obj(saved_stack[i]);
    }

    retain_obj(saved);
    stack_push(ctx, saved);
    tf_ret left_res = exec(ctx, left);
    if (stack_len(ctx) < base_len) {
        tf_restore_stack_copy(ctx, saved_stack, base_len);
        stack_push(ctx, saved);
        for (size_t i = 0; i < base_len; i++) release_obj(saved_stack[i]);
        free(saved_stack);
        release_obj(right);
        release_obj(left);
        return TF_ERR;
    }
    if (left_res != TF_OK) {
        tf_restore_stack_copy(ctx, saved_stack, base_len);
        stack_push(ctx, saved);
        for (size_t i = 0; i < base_len; i++) release_obj(saved_stack[i]);
        free(saved_stack);
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
        for (size_t i = 0; i < base_len; i++) release_obj(saved_stack[i]);
        free(saved_stack);
        release_obj(right);
        release_obj(left);
        return right_res;
    }
    if (stack_len(ctx) < base_len) {
        tf_restore_stack_copy(ctx, saved_stack, base_len);
        stack_push(ctx, saved);
        for (size_t i = 0; i < left_out_len; i++) release_obj(left_outputs[i]);
        free(left_outputs);
        for (size_t i = 0; i < base_len; i++) release_obj(saved_stack[i]);
        free(saved_stack);
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
    for (size_t i = 0; i < base_len; i++) release_obj(saved_stack[i]);
    free(saved_stack);
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

tf_ret tf_replicate_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_pop_type(ctx, TF_OBJ_TYPE_LIST);
    tf_obj *n_obj = stack_pop_type(ctx, TF_OBJ_TYPE_INT);

    if (!body || !n_obj) {
        if (body) release_obj(body);
        if (n_obj) release_obj(n_obj);
        return TF_ERR;
    }

    int n = n_obj->i;
    release_obj(n_obj);

    if (n < 0) {
        release_obj(body);
        return TF_ERR;
    }

    tf_obj *res = init_list_obj();
    tf_ret ret = TF_OK;

    for (int i = 0; i < n; i++) {
        ret = exec(ctx, body);
        if (ret != TF_OK) break;
        if (stack_len(ctx) < 1) {
            ret = TF_ERR;
            break;
        }
        tf_obj *item = stack_pop(ctx);
        push_obj(res, item);
    }

    if (ret == TF_OK) {
        stack_push(ctx, res);
    } else {
        release_obj(res);
    }

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
    if (body->type != TF_OBJ_TYPE_LIST || data->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    data = stack_pop(ctx);

    tf_ret res = TF_OK;
    for (size_t i = 0; i < data->list.len; i++) {
        stack_push(ctx, data->list.elem[i]);
        retain_obj(data->list.elem[i]);
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
    if (body->type != TF_OBJ_TYPE_LIST || data->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    data = stack_pop(ctx);

    size_t saved_len = stack_len(ctx);
    tf_obj **saved_stack =
        saved_len > 0 ? xmalloc(sizeof(tf_obj *) * saved_len) : NULL;
    for (size_t i = 0; i < saved_len; i++) {
        saved_stack[i] = stack_peek(ctx, saved_len - 1 - i);
        retain_obj(saved_stack[i]);
    }

    tf_obj *result = init_list_obj();
    tf_ret res = TF_OK;
    for (size_t i = 0; i < data->list.len; i++) {
        tf_obj *elem = data->list.elem[i];
        stack_push(ctx, elem);
        retain_obj(elem);

        res = exec(ctx, body);
        if (res != TF_OK) break;

        if (stack_len(ctx) != saved_len + 1) {
            res = TF_ERR;
            break;
        }

        tf_obj *mapped = stack_pop(ctx);
        push_obj(result, mapped);
        tf_restore_stack_copy(ctx, saved_stack, saved_len);
    }

    if (res != TF_OK) {
        tf_restore_stack_copy(ctx, saved_stack, saved_len);
        release_obj(result);
    } else {
        stack_push(ctx, result);
    }

    for (size_t i = 0; i < saved_len; i++) release_obj(saved_stack[i]);
    free(saved_stack);
    release_obj(body);
    release_obj(data);
    return res;
}

tf_ret tf_fold_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *data = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST || data->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    data = stack_pop(ctx);
    tf_obj *acc = stack_pop(ctx);

    size_t saved_len = stack_len(ctx);
    tf_obj **saved_stack =
        saved_len > 0 ? xmalloc(sizeof(tf_obj *) * saved_len) : NULL;
    for (size_t i = 0; i < saved_len; i++) {
        saved_stack[i] = stack_peek(ctx, saved_len - 1 - i);
        retain_obj(saved_stack[i]);
    }

    stack_push(ctx, acc);
    tf_ret res = TF_OK;
    for (size_t i = 0; i < data->list.len; i++) {
        tf_obj *elem = data->list.elem[i];
        stack_push(ctx, elem);
        retain_obj(elem);

        res = exec(ctx, body);
        if (res != TF_OK) break;

        if (stack_len(ctx) != saved_len + 1) {
            res = TF_ERR;
            break;
        }
    }

    if (res != TF_OK) {
        tf_restore_stack_copy(ctx, saved_stack, saved_len);
    }

    for (size_t i = 0; i < saved_len; i++) release_obj(saved_stack[i]);
    free(saved_stack);
    release_obj(body);
    release_obj(data);
    return res;
}
