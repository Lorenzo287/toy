#include "tf_lib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tf_exec.h"
#include "tf_alloc.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <time.h>
#include "tf_obj.h"

/* Helper for binary math operations */
static tf_ret tf_binary_math(tf_ctx *ctx, char op) {
    if (stack_len(ctx) < 2) return TF_ERR;

    tf_obj *b = stack_pop(ctx);
    tf_obj *a = stack_pop(ctx);

    if ((a->type != TF_OBJ_TYPE_INT && a->type != TF_OBJ_TYPE_FLOAT) ||
        (b->type != TF_OBJ_TYPE_INT && b->type != TF_OBJ_TYPE_FLOAT)) {
        stack_push(ctx, a);
        stack_push(ctx, b);
        return TF_ERR;
    }

    bool is_float =
        (a->type == TF_OBJ_TYPE_FLOAT || b->type == TF_OBJ_TYPE_FLOAT);

    if (is_float) {
        float fa = (a->type == TF_OBJ_TYPE_FLOAT) ? a->f : (float)a->i;
        float fb = (b->type == TF_OBJ_TYPE_FLOAT) ? b->f : (float)b->i;
        float fresult = 0;

        switch (op) {
        case '+':
            fresult = fa + fb;
            break;
        case '-':
            fresult = fa - fb;
            break;
        case '*':
            fresult = fa * fb;
            break;
        case '/':
            if (fb == 0.0f) {
                stack_push(ctx, a);
                stack_push(ctx, b);
                return TF_ERR;
            }
            fresult = fa / fb;
            break;
        case 'M':
            fresult = (fa > fb) ? fa : fb;
            break;  // max
        case 'm':
            fresult = (fa < fb) ? fa : fb;
            break;  // min
        default:
            stack_push(ctx, a);
            stack_push(ctx, b);
            return TF_ERR;
        }
        stack_push(ctx, create_float_obj(fresult));
    } else {
        int ia = a->i;
        int ib = b->i;
        int iresult = 0;

        switch (op) {
        case '+':
            iresult = ia + ib;
            break;
        case '-':
            iresult = ia - ib;
            break;
        case '*':
            iresult = ia * ib;
            break;
        case '/':
        case '%':
            if (ib == 0) {
                stack_push(ctx, a);
                stack_push(ctx, b);
                return TF_ERR;
            }
            if (op == '/')
                iresult = ia / ib;
            else
                iresult = ia % ib;
            break;
        case 'M':
            iresult = (ia > ib) ? ia : ib;
            break;  // max
        case 'm':
            iresult = (ia < ib) ? ia : ib;
            break;  // min
        default:
            stack_push(ctx, a);
            stack_push(ctx, b);
            return TF_ERR;
        }
        stack_push(ctx, create_int_obj(iresult));
    }

    release_obj(a);
    release_obj(b);
    return TF_OK;
}

tf_ret tf_add(tf_ctx *ctx) {
    return tf_binary_math(ctx, '+');
}
tf_ret tf_sub(tf_ctx *ctx) {
    return tf_binary_math(ctx, '-');
}
tf_ret tf_mul(tf_ctx *ctx) {
    return tf_binary_math(ctx, '*');
}
tf_ret tf_div(tf_ctx *ctx) {
    return tf_binary_math(ctx, '/');
}
tf_ret tf_mod(tf_ctx *ctx) {
    return tf_binary_math(ctx, '%');
}
tf_ret tf_max(tf_ctx *ctx) {
    return tf_binary_math(ctx, 'M');
}
tf_ret tf_min(tf_ctx *ctx) {
    return tf_binary_math(ctx, 'm');
}

tf_ret tf_neg(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_INT) {
        stack_push(ctx, create_int_obj(-a->i));
    } else if (a->type == TF_OBJ_TYPE_FLOAT) {
        stack_push(ctx, create_float_obj(-a->f));
    } else {
        stack_push(ctx, a);
        return TF_ERR;
    }
    release_obj(a);
    return TF_OK;
}

tf_ret tf_abs(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_INT) {
        stack_push(ctx, create_int_obj(a->i < 0 ? -a->i : a->i));
    } else if (a->type == TF_OBJ_TYPE_FLOAT) {
        stack_push(ctx, create_float_obj(a->f < 0 ? -a->f : a->f));
    } else {
        stack_push(ctx, a);
        return TF_ERR;
    }
    release_obj(a);
    return TF_OK;
}

tf_ret tf_dup(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = ctx->forth_stack->list.elem[ctx->forth_stack->list.len - 1];
    stack_push(ctx, o);
    retain_obj(o);
    return TF_OK;
}

tf_ret tf_drop(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    release_obj(o);
    return TF_OK;
}

tf_ret tf_swap(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    tf_obj *b = stack_pop(ctx);
    stack_push(ctx, a);
    stack_push(ctx, b);
    return TF_OK;
}

tf_ret tf_over(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *o = ctx->forth_stack->list.elem[ctx->forth_stack->list.len - 2];
    stack_push(ctx, o);
    retain_obj(o);
    return TF_OK;
}

tf_ret tf_rot(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *c = stack_pop(ctx);
    tf_obj *b = stack_pop(ctx);
    tf_obj *a = stack_pop(ctx);
    stack_push(ctx, b);
    stack_push(ctx, c);
    stack_push(ctx, a);
    return TF_OK;
}

tf_ret tf_nip(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    tf_obj *b = stack_pop(ctx);
    release_obj(b);
    stack_push(ctx, a);
    return TF_OK;
}

tf_ret tf_tuck(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    tf_obj *b = stack_pop(ctx);
    stack_push(ctx, a);
    stack_push(ctx, b);
    stack_push(ctx, a);
    retain_obj(a);
    return TF_OK;
}

tf_ret tf_pick(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *o = stack_peek(ctx, 0);
    if (o->type != TF_OBJ_TYPE_INT) return TF_ERR;
    int pos = o->i;
    size_t len = stack_len(ctx);
    if (pos < 0 || len < (size_t)pos + 2) return TF_ERR;

    o = stack_pop(ctx);
    release_obj(o);

    len = stack_len(ctx);
    tf_obj *a = ctx->forth_stack->list.elem[len - 1 - (size_t)pos];
    stack_push(ctx, a);
    retain_obj(a);
    return TF_OK;
}

tf_ret tf_roll(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *o = stack_peek(ctx, 0);
    if (o->type != TF_OBJ_TYPE_INT) return TF_ERR;
    int pos = o->i;
    if (pos < 0 || stack_len(ctx) < (size_t)pos + 2) return TF_ERR;

    o = stack_pop(ctx);
    release_obj(o);
    if (pos == 0) { return TF_OK; }

    tf_obj **temp_stack = xmalloc(sizeof(tf_obj *) * (size_t)pos);
    for (int i = 0; i < pos; i++) {
        tf_obj *a = stack_pop(ctx);
        temp_stack[i] = a;
    }
    tf_obj *b = stack_pop(ctx);
    for (int i = pos - 1; i >= 0; i--) stack_push(ctx, temp_stack[i]);
    stack_push(ctx, b);
    free(temp_stack);
    return TF_OK;
}

tf_ret tf_empty(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_OK;
    while (stack_len(ctx) > 0) {
        tf_obj *o = stack_pop(ctx);
        release_obj(o);
    }
    return TF_OK;
}

tf_ret tf_printf(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    print_value(o);
    release_obj(o);
    return TF_OK;
}

tf_ret tf_print(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    print_value(o);
    printf("\n");
    release_obj(o);
    return TF_OK;
}

tf_ret tf_dot(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_peek(ctx, 0);
    print_value(o);
    printf("\n");
    return TF_OK;
}

tf_ret tf_cr(tf_ctx *ctx) {
    printf("\n");
    (void)ctx;
    return TF_OK;
}

tf_ret tf_stack(tf_ctx *ctx) {
    size_t len = stack_len(ctx);
    printf("<%zu> ", len);
    for (size_t i = 0; i < len; i++) {
        print_value(ctx->forth_stack->list.elem[i]);
        printf(" ");
    }
    printf("\n");
    return TF_OK;
}

/* Helper for comparison operations */
static tf_ret tf_compare(tf_ctx *ctx, char *op) {
    if (stack_len(ctx) < 2) return TF_ERR;

    tf_obj *b = stack_pop(ctx);
    tf_obj *a = stack_pop(ctx);

    bool result = false;

    if ((a->type == TF_OBJ_TYPE_INT || a->type == TF_OBJ_TYPE_FLOAT) &&
        (b->type == TF_OBJ_TYPE_INT || b->type == TF_OBJ_TYPE_FLOAT)) {
        float fa = (a->type == TF_OBJ_TYPE_FLOAT) ? a->f : (float)a->i;
        float fb = (b->type == TF_OBJ_TYPE_FLOAT) ? b->f : (float)b->i;

        if (!strcmp(op, "=="))
            result = (fa == fb);
        else if (!strcmp(op, "!="))
            result = (fa != fb);
        else if (!strcmp(op, "<"))
            result = (fa < fb);
        else if (!strcmp(op, ">"))
            result = (fa > fb);
        else if (!strcmp(op, "<="))
            result = (fa <= fb);
        else if (!strcmp(op, ">="))
            result = (fa >= fb);
    } else if (a->type == b->type) {
        if (!strcmp(op, "==")) {
            if (a->type == TF_OBJ_TYPE_BOOL)
                result = (a->b == b->b);
            else if (a->type == TF_OBJ_TYPE_STR ||
                     a->type == TF_OBJ_TYPE_SYMBOL)
                result = (compare_string_obj(a, b) == 0);
            else
                result = (a == b);
        } else if (!strcmp(op, "!=")) {
            if (a->type == TF_OBJ_TYPE_BOOL)
                result = (a->b != b->b);
            else if (a->type == TF_OBJ_TYPE_STR ||
                     a->type == TF_OBJ_TYPE_SYMBOL)
                result = (compare_string_obj(a, b) != 0);
            else
                result = (a != b);
        } else {
            stack_push(ctx, a);
            stack_push(ctx, b);
            return TF_ERR;
        }
    } else {
        if (!strcmp(op, "=="))
            result = false;
        else if (!strcmp(op, "!="))
            result = true;
        else {
            stack_push(ctx, a);
            stack_push(ctx, b);
            return TF_ERR;
        }
    }

    stack_push(ctx, create_bool_obj(result));
    release_obj(a);
    release_obj(b);
    return TF_OK;
}

tf_ret tf_eq(tf_ctx *ctx) {
    return tf_compare(ctx, "==");
}
tf_ret tf_ne(tf_ctx *ctx) {
    return tf_compare(ctx, "!=");
}
tf_ret tf_lt(tf_ctx *ctx) {
    return tf_compare(ctx, "<");
}
tf_ret tf_gt(tf_ctx *ctx) {
    return tf_compare(ctx, ">");
}
tf_ret tf_le(tf_ctx *ctx) {
    return tf_compare(ctx, "<=");
}
tf_ret tf_ge(tf_ctx *ctx) {
    return tf_compare(ctx, ">=");
}

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

static void tf_restore_stack(tf_ctx *ctx, tf_obj **saved_stack, size_t saved_len) {
    while (stack_len(ctx) > 0) {
        tf_obj *o = stack_pop(ctx);
        release_obj(o);
    }

    for (size_t i = 0; i < saved_len; i++) {
        stack_push(ctx, saved_stack[i]);
    }
}

static tf_ret tf_eval_condition(tf_ctx *ctx, tf_obj *cond, bool allow_bool,
                                bool *cond_val) {
    if (cond->type == TF_OBJ_TYPE_BOOL) {
        if (!allow_bool) return TF_ERR;
        *cond_val = cond->b;
        return TF_OK;
    }

    if (cond->type != TF_OBJ_TYPE_LIST) return TF_ERR;

    size_t saved_len = stack_len(ctx);
    tf_obj **saved_stack = xmalloc(sizeof(tf_obj *) * saved_len);
    for (size_t i = 0; i < saved_len; i++) {
        saved_stack[i] = ctx->forth_stack->list.elem[i];
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
    tf_ret eval_res = tf_eval_condition(ctx, cond, true, &cond_val);
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
    tf_ret eval_res = tf_eval_condition(ctx, cond, true, &cond_val);
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
        tf_ret eval_res = tf_eval_condition(ctx, cond, false, &continue_loop);
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

tf_ret tf_colon(tf_ctx *ctx) {
    if (ctx->cstack_len == 0) return TF_ERR;
    tf_frame *f = &ctx->call_stack[ctx->cstack_len - 1];

    if (f->pc >= f->prg->list.len) return TF_ERR;
    tf_obj *func_name = f->prg->list.elem[f->pc];
    if (func_name->type != TF_OBJ_TYPE_SYMBOL) return TF_ERR;

    tf_obj *body = init_list_obj();
    f->pc++;
    while (f->pc < f->prg->list.len) {
        tf_obj *o = f->prg->list.elem[f->pc];
        if (o->type == TF_OBJ_TYPE_SYMBOL && strcmp(o->str.ptr, ";") == 0) {
            break;
        }
        push_obj(body, o);
        retain_obj(o);
        f->pc++;
    }

    if (f->pc >= f->prg->list.len) {
        release_obj(body);
        return TF_ERR;
    }

    set_user_func(ctx, func_name, body);
    release_obj(body);
    f->pc++;
    return TF_OK;
}

tf_ret tf_def(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *func_name = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST ||
        func_name->type != TF_OBJ_TYPE_SYMBOL) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    func_name = stack_pop(ctx);

    set_user_func(ctx, func_name, body);

    release_obj(body);
    release_obj(func_name);
    return TF_OK;
}

tf_ret tf_geth(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *idx_obj = stack_peek(ctx, 0);
    tf_obj *list_obj = stack_peek(ctx, 1);
    if (idx_obj->type != TF_OBJ_TYPE_INT ||
        list_obj->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    int idx = idx_obj->i;
    if (idx < 0 || idx >= (int)list_obj->list.len) return TF_ERR;

    idx_obj = stack_pop(ctx);
    tf_obj *result = list_obj->list.elem[idx];
    retain_obj(result);
    stack_push(ctx, result);

    release_obj(idx_obj);
    return TF_OK;
}

tf_ret tf_seth(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *idx_obj = stack_peek(ctx, 1);
    tf_obj *list_obj = stack_peek(ctx, 2);
    if (idx_obj->type != TF_OBJ_TYPE_INT ||
        list_obj->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    int idx = idx_obj->i;
    if (idx < 0 || idx >= (int)list_obj->list.len) return TF_ERR;

    tf_obj *val = stack_pop(ctx);
    idx_obj = stack_pop(ctx);
    list_obj = stack_pop(ctx);

    release_obj(list_obj->list.elem[idx]);
    list_obj->list.elem[idx] = val;
    release_obj(idx_obj);
    release_obj(list_obj);
    return TF_OK;
}

tf_ret tf_len(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_peek(ctx, 0);
    if (o->type != TF_OBJ_TYPE_LIST) return TF_ERR;
    stack_push(ctx, create_int_obj((int)o->list.len));
    return TF_OK;
}

tf_ret tf_first(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *list_obj = stack_peek(ctx, 0);
    if (list_obj->type != TF_OBJ_TYPE_LIST || list_obj->list.len == 0) {
        return TF_ERR;
    }

    tf_obj *result = list_obj->list.elem[0];
    retain_obj(result);
    stack_push(ctx, result);
    return TF_OK;
}

tf_ret tf_rest(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *list_obj = stack_peek(ctx, 0);
    if (list_obj->type != TF_OBJ_TYPE_LIST || list_obj->list.len == 0) {
        return TF_ERR;
    }

    tf_obj *rest = init_list_obj();
	// push to new list starting from second element
    for (size_t i = 1; i < list_obj->list.len; i++) {
        tf_obj *elem = list_obj->list.elem[i];
        retain_obj(elem);
        push_obj(rest, elem);
    }
    stack_push(ctx, rest);
    return TF_OK;
}

tf_ret tf_uncons(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *list_obj = stack_peek(ctx, 0);
    if (list_obj->type != TF_OBJ_TYPE_LIST || list_obj->list.len == 0) {
        return TF_ERR;
    }

    list_obj = stack_pop(ctx);
    tf_obj *head = list_obj->list.elem[0];
    retain_obj(head);

    tf_obj *tail = init_list_obj();
    for (size_t i = 1; i < list_obj->list.len; i++) {
        tf_obj *elem = list_obj->list.elem[i];
        retain_obj(elem);
        push_obj(tail, elem);
    }

    stack_push(ctx, head);
    stack_push(ctx, tail);
    release_obj(list_obj);
    return TF_OK;
}

tf_ret tf_cons(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *list_obj = stack_peek(ctx, 0);
    if (list_obj->type != TF_OBJ_TYPE_LIST) return TF_ERR;

    list_obj = stack_pop(ctx);
    tf_obj *head = stack_pop(ctx);
    tf_obj *result = init_list_obj();
    push_obj(result, head);

    for (size_t i = 0; i < list_obj->list.len; i++) {
        tf_obj *elem = list_obj->list.elem[i];
        retain_obj(elem);
        push_obj(result, elem);
    }

    stack_push(ctx, result);
    release_obj(list_obj);
    return TF_OK;
}

tf_ret tf_concat(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *right = stack_peek(ctx, 0);
    tf_obj *left = stack_peek(ctx, 1);
    if (left->type != TF_OBJ_TYPE_LIST || right->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    right = stack_pop(ctx);
    left = stack_pop(ctx);
    tf_obj *result = init_list_obj();
    for (size_t i = 0; i < left->list.len; i++) {
        tf_obj *elem = left->list.elem[i];
        retain_obj(elem);
        push_obj(result, elem);
    }
    for (size_t i = 0; i < right->list.len; i++) {
        tf_obj *elem = right->list.elem[i];
        retain_obj(elem);
        push_obj(result, elem);
    }

    stack_push(ctx, result);
    release_obj(left);
    release_obj(right);
    return TF_OK;
}

tf_ret tf_empty_q(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *list_obj = stack_peek(ctx, 0);
    if (list_obj->type != TF_OBJ_TYPE_LIST) return TF_ERR;
    stack_push(ctx, create_bool_obj(list_obj->list.len == 0));
    return TF_OK;
}

tf_ret tf_rand(tf_ctx *ctx) {
    stack_push(ctx, create_int_obj(rand()));
    return TF_OK;
}

tf_ret tf_sleep(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *ms_obj = stack_peek(ctx, 0);
    if (ms_obj->type != TF_OBJ_TYPE_INT) return TF_ERR;
    ms_obj = stack_pop(ctx);
#ifdef _WIN32
    Sleep(ms_obj->i);
#else
    struct timespec req = {.tv_sec = ms_obj->i / 1000,
                           .tv_nsec = (long)(ms_obj->i % 1000) * 1000000L};
    nanosleep(&req, NULL);
#endif
    release_obj(ms_obj);
    return TF_OK;
}

tf_ret tf_key(tf_ctx *ctx) {
    int c = getchar();
    if (c == EOF) return TF_ERR;
    stack_push(ctx, create_int_obj(c));
    return TF_OK;
}

#define MAX_BUF_LEN 1023
tf_ret tf_input(tf_ctx *ctx) {
    char buf[MAX_BUF_LEN + 1];
    if (!fgets(buf, sizeof buf, stdin)) return TF_ERR;
    buf[strcspn(buf, "\n")] = '\0';
    stack_push(ctx, create_string_obj(buf, strlen(buf)));
    return TF_OK;
}

tf_ret tf_time(tf_ctx *ctx) {
    stack_push(ctx, create_int_obj((int)clock()));
    return TF_OK;
}

tf_ret tf_clear(tf_ctx *ctx) {
    (void)ctx;
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(out, &info)) {
        DWORD cell_count = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
        COORD home = {0, 0};
        DWORD written = 0;

        if (FillConsoleOutputCharacterA(out, ' ', cell_count, home, &written) &&
            FillConsoleOutputAttribute(out, info.wAttributes, cell_count, home,
                                       &written) &&
            SetConsoleCursorPosition(out, home)) {
            return TF_OK;
        }
    }
#endif

    printf("\x1b[H\x1b[2J");
    fflush(stdout);
    return TF_OK;
}

static int tf_func_name_cmp(const void *a, const void *b) {
    tf_func *const *fa = a;
    tf_func *const *fb = b;
    return compare_string_obj((*fa)->name, (*fb)->name);
}

tf_ret tf_words(tf_ctx *ctx) {
    size_t count = ctx->functions.count;
    if (count == 0) {
        printf("\n");
        return TF_OK;
    }
    tf_func **funcs = xmalloc(sizeof(tf_func *) * count);
    size_t j = 0;
    for (size_t i = 0; i < ctx->functions.capacity; i++) {
        tf_func *f = ctx->functions.buckets[i];
        if (f != NULL) funcs[j++] = f;
    }
    qsort(funcs, j, sizeof(tf_func *), tf_func_name_cmp);
    for (size_t i = 0; i < j; i++) {
        printf("%s", funcs[i]->name->str.ptr);
        printf(i + 1 < j ? " " : "\n");
    }
    free(funcs);
    return TF_OK;
}

tf_ret tf_see(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *name = stack_peek(ctx, 0);
    if (name->type != TF_OBJ_TYPE_SYMBOL) return TF_ERR;

    name = stack_pop(ctx);

    tf_func *func = get_func(ctx, name);
    if (!func) {
        release_obj(name);
        return TF_ERR;
    }

    if (func->type == TF_FUNC_TYPE_NATIVE) {
        printf("%s is a native word\n", func->name->str.ptr);
        release_obj(name);
        return TF_OK;
    }

    printf(": %s ", func->name->str.ptr);
    for (size_t i = 0; i < func->user_impl->list.len; i++) {
        print_source_obj(func->user_impl->list.elem[i]);
        if (i + 1 < func->user_impl->list.len) printf(" ");
    }
    printf(" ;\n");
    release_obj(name);
    return TF_OK;
}

tf_ret tf_exit(tf_ctx *ctx) {
    (void)ctx;
    exit(0);
    return TF_OK;
}
