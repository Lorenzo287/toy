#include "tf_lib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tf_exec.h"
#include "tf_alloc.h"
#include "tf_console.h"
#include "tf_lexer.h"
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

    bool is_float = (a->type == TF_OBJ_TYPE_FLOAT || b->type == TF_OBJ_TYPE_FLOAT);

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

tf_ret tf_succ(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_INT) {
        stack_push(ctx, create_int_obj(a->i + 1));
    } else if (a->type == TF_OBJ_TYPE_FLOAT) {
        stack_push(ctx, create_float_obj(a->f + 1.0f));
    } else {
        stack_push(ctx, a);
        return TF_ERR;
    }
    release_obj(a);
    return TF_OK;
}

tf_ret tf_pred(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_INT) {
        stack_push(ctx, create_int_obj(a->i - 1));
    } else if (a->type == TF_OBJ_TYPE_FLOAT) {
        stack_push(ctx, create_float_obj(a->f - 1.0f));
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

/* Helper for logical and bitwise operations */
static tf_ret tf_logic(tf_ctx *ctx, char op) {
    if (stack_len(ctx) < 2) return TF_ERR;

    tf_obj *b = stack_pop(ctx);
    tf_obj *a = stack_pop(ctx);

    if (a->type == TF_OBJ_TYPE_BOOL && b->type == TF_OBJ_TYPE_BOOL) {
        bool res = false;
        if (op == '&') res = a->b && b->b;
        else if (op == '|') res = a->b || b->b;
        else if (op == '^') res = a->b ^ b->b;
        stack_push(ctx, create_bool_obj(res));
    } else if (a->type == TF_OBJ_TYPE_INT && b->type == TF_OBJ_TYPE_INT) {
        int res = 0;
        if (op == '&') res = a->i & b->i;
        else if (op == '|') res = a->i | b->i;
        else if (op == '^') res = a->i ^ b->i;
        stack_push(ctx, create_int_obj(res));
    } else {
        stack_push(ctx, a);
        stack_push(ctx, b);
        return TF_ERR;
    }

    release_obj(a);
    release_obj(b);
    return TF_OK;
}

static tf_ret tf_shift(tf_ctx *ctx, bool left) {
    if (stack_len(ctx) < 2) return TF_ERR;

    tf_obj *b = stack_pop(ctx);
    tf_obj *a = stack_pop(ctx);

    if (a->type == TF_OBJ_TYPE_INT && b->type == TF_OBJ_TYPE_INT) {
        int res = left ? (a->i << b->i) : (a->i >> b->i);
        stack_push(ctx, create_int_obj(res));
    } else {
        stack_push(ctx, a);
        stack_push(ctx, b);
        release_obj(a);
        release_obj(b);
        return TF_ERR;
    }

    release_obj(a);
    release_obj(b);
    return TF_OK;
}

tf_ret tf_shl(tf_ctx *ctx) { return tf_shift(ctx, true); }
tf_ret tf_shr(tf_ctx *ctx) { return tf_shift(ctx, false); }

tf_ret tf_and(tf_ctx *ctx) { return tf_logic(ctx, '&'); }
tf_ret tf_or(tf_ctx *ctx) { return tf_logic(ctx, '|'); }
tf_ret tf_xor(tf_ctx *ctx) { return tf_logic(ctx, '^'); }

tf_ret tf_not(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_BOOL) {
        stack_push(ctx, create_bool_obj(!a->b));
    } else if (a->type == TF_OBJ_TYPE_INT) {
        stack_push(ctx, create_int_obj(~a->i));
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
    size_t len = ctx->forth_stack->list.len;
    tf_obj *o = ctx->forth_stack->list.elem[len - 2];
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

tf_ret tf_swapd(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *c = stack_pop(ctx);
    tf_obj *b = stack_pop(ctx);
    tf_obj *a = stack_pop(ctx);
    stack_push(ctx, b);
    stack_push(ctx, a);
    stack_push(ctx, c);
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
            else if (a->type == TF_OBJ_TYPE_STR || a->type == TF_OBJ_TYPE_SYMBOL)
                result = (compare_string_obj(a, b) == 0);
            else
                result = (a == b);
        } else if (!strcmp(op, "!=")) {
            if (a->type == TF_OBJ_TYPE_BOOL)
                result = (a->b != b->b);
            else if (a->type == TF_OBJ_TYPE_STR || a->type == TF_OBJ_TYPE_SYMBOL)
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

    // Schedule with synthetic frame: [ a prg exec b prg exec ]
    // This allows the engine to run the quotation on each argument
    // sequentially without recursive C calls to exec().
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
        saved_stack[i] = ctx->forth_stack->list.elem[i];
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
        saved_stack[i] = ctx->forth_stack->list.elem[i];
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
        saved_stack[i] = ctx->forth_stack->list.elem[i];
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
        saved_stack[i] = ctx->forth_stack->list.elem[i];
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
        saved_stack[i] = ctx->forth_stack->list.elem[i];
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
        if (o->type == TF_OBJ_TYPE_SYMBOL && strcmp(o->str.ptr, ";") == 0) break;
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
    if (body->type != TF_OBJ_TYPE_LIST || func_name->type != TF_OBJ_TYPE_SYMBOL) {
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
    if (idx_obj->type != TF_OBJ_TYPE_INT || list_obj->type != TF_OBJ_TYPE_LIST) {
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
    if (idx_obj->type != TF_OBJ_TYPE_INT || list_obj->type != TF_OBJ_TYPE_LIST) {
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
    if (o->type == TF_OBJ_TYPE_LIST) {
        stack_push(ctx, create_int_obj((int)o->list.len));
    } else if (o->type == TF_OBJ_TYPE_STR) {
        stack_push(ctx, create_int_obj((int)o->str.len));
    } else {
        return TF_ERR;
    }
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

tf_ret tf_append(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *list_obj = stack_peek(ctx, 1);
    if (list_obj->type != TF_OBJ_TYPE_LIST) return TF_ERR;

    tf_obj *elem = stack_pop(ctx);
    list_obj = stack_pop(ctx);
    tf_obj *result = init_list_obj();

    for (size_t i = 0; i < list_obj->list.len; i++) {
        tf_obj *item = list_obj->list.elem[i];
        retain_obj(item);
        push_obj(result, item);
    }
    push_obj(result, elem);

    stack_push(ctx, result);
    release_obj(list_obj);
    return TF_OK;
}

tf_ret tf_concat(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *right = stack_peek(ctx, 0);
    tf_obj *left = stack_peek(ctx, 1);

    if (left->type == TF_OBJ_TYPE_STR && right->type == TF_OBJ_TYPE_STR) {
        right = stack_pop(ctx);
        left = stack_pop(ctx);
        size_t new_len = left->str.len + right->str.len;
        char *new_ptr = xmalloc(new_len + 1);
        memcpy(new_ptr, left->str.ptr, left->str.len);
        memcpy(new_ptr + left->str.len, right->str.ptr, right->str.len);
        new_ptr[new_len] = '\0';
        stack_push(ctx, create_string_obj(new_ptr, new_len));
        free(new_ptr);
        release_obj(left);
        release_obj(right);
        return TF_OK;
    }

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

tf_ret tf_splits(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *arg2 = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    tf_obj *arg1 = stack_pop_type(ctx, TF_OBJ_TYPE_STR);

    if (!arg1 || !arg2) {
        if (arg1) release_obj(arg1);
        if (arg2) release_obj(arg2);
        return TF_ERR;
    }

    tf_obj *result = init_list_obj();
    char *start = arg1->str.ptr;
    char *sep = arg2->str.ptr;
    size_t sep_len = arg2->str.len;

    if (sep_len == 0) {
        for (size_t i = 0; i < arg1->str.len; i++) {
            push_obj(result, create_string_obj(arg1->str.ptr + i, 1));
        }
    } else {
        char *p;
        while ((p = strstr(start, sep)) != NULL) {
            push_obj(result, create_string_obj(start, p - start));
            start = p + sep_len;
        }
        push_obj(result, create_string_obj(start, strlen(start)));
    }

    stack_push(ctx, result);
    release_obj(arg1);
    release_obj(arg2);
    return TF_OK;
}

tf_ret tf_split_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *list_obj = stack_peek(ctx, 1);

    if (pred->type != TF_OBJ_TYPE_LIST || list_obj->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    pred = stack_pop(ctx);
    list_obj = stack_pop(ctx);

    size_t saved_len = stack_len(ctx);
    tf_obj **saved_stack =
        saved_len > 0 ? xmalloc(sizeof(tf_obj *) * saved_len) : NULL;
    for (size_t i = 0; i < saved_len; i++) {
        saved_stack[i] = ctx->forth_stack->list.elem[i];
        retain_obj(saved_stack[i]);
    }

    tf_obj *true_list = init_list_obj();
    tf_obj *false_list = init_list_obj();
    tf_ret res = TF_OK;

    for (size_t i = 0; i < list_obj->list.len; i++) {
        tf_obj *elem = list_obj->list.elem[i];
        stack_push(ctx, elem);
        retain_obj(elem);

        res = exec(ctx, pred);
        if (res != TF_OK) break;

        tf_obj *bool_res = stack_pop_type(ctx, TF_OBJ_TYPE_BOOL);
        if (!bool_res) {
            res = TF_ERR;
            break;
        }

        tf_obj *target = bool_res->b ? true_list : false_list;
        retain_obj(elem);
        push_obj(target, elem);
        release_obj(bool_res);

        tf_restore_stack_copy(ctx, saved_stack, saved_len);
    }

    if (res != TF_OK) {
        tf_restore_stack_copy(ctx, saved_stack, saved_len);
        release_obj(true_list);
        release_obj(false_list);
    } else {
        stack_push(ctx, true_list);
        stack_push(ctx, false_list);
    }

    for (size_t i = 0; i < saved_len; i++) release_obj(saved_stack[i]);
    free(saved_stack);
    release_obj(pred);
    release_obj(list_obj);
    return res;
}

tf_ret tf_join(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *sep = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    tf_obj *list = stack_pop_type(ctx, TF_OBJ_TYPE_LIST);
    if (!sep || !list) {
        if (sep) release_obj(sep);
        if (list) release_obj(list);
        return TF_ERR;
    }

    size_t total_len = 0;
    for (size_t i = 0; i < list->list.len; i++) {
        tf_obj *elem = list->list.elem[i];
        if (elem->type == TF_OBJ_TYPE_STR) {
            total_len += elem->str.len;
        } else {
            /* Fallback or error? For now, we skip non-strings or could convert them.
               Let's require strings for now. */
        }
    }
    if (list->list.len > 1) {
        total_len += sep->str.len * (list->list.len - 1);
    }

    char *result = xmalloc(total_len + 1);
    char *p = result;
    for (size_t i = 0; i < list->list.len; i++) {
        tf_obj *elem = list->list.elem[i];
        if (elem->type == TF_OBJ_TYPE_STR) {
            memcpy(p, elem->str.ptr, elem->str.len);
            p += elem->str.len;
            if (i + 1 < list->list.len) {
                memcpy(p, sep->str.ptr, sep->str.len);
                p += sep->str.len;
            }
        }
    }
    *p = '\0';

    stack_push(ctx, create_string_obj(result, total_len));
    free(result);
    release_obj(list);
    release_obj(sep);
    return TF_OK;
}

tf_ret tf_trim(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *str = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!str) return TF_ERR;

    char *start = str->str.ptr;
    char *end = str->str.ptr + str->str.len - 1;

    while (start <= end && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*end)) end--;

    size_t new_len = (start <= end) ? (size_t)(end - start + 1) : 0;
    stack_push(ctx, create_string_obj(start, new_len));
    release_obj(str);
    return TF_OK;
}

tf_ret tf_upper(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *str = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!str) return TF_ERR;

    char *new_str = xmalloc(str->str.len + 1);
    for (size_t i = 0; i < str->str.len; i++) {
        new_str[i] = (char)toupper((unsigned char)str->str.ptr[i]);
    }
    new_str[str->str.len] = '\0';

    stack_push(ctx, create_string_obj(new_str, str->str.len));
    free(new_str);
    release_obj(str);
    return TF_OK;
}

tf_ret tf_lower(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *str = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!str) return TF_ERR;

    char *new_str = xmalloc(str->str.len + 1);
    for (size_t i = 0; i < str->str.len; i++) {
        new_str[i] = (char)tolower((unsigned char)str->str.ptr[i]);
    }
    new_str[str->str.len] = '\0';

    stack_push(ctx, create_string_obj(new_str, str->str.len));
    free(new_str);
    release_obj(str);
    return TF_OK;
}

tf_ret tf_splitmid(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;

    tf_obj *list_obj = stack_pop_type(ctx, TF_OBJ_TYPE_LIST);
    if (!list_obj) return TF_ERR;

    size_t len = list_obj->list.len;
    size_t mid = len / 2;

    tf_obj *left = init_list_obj();
    tf_obj *right = init_list_obj();

    for (size_t i = 0; i < mid; i++) {
        tf_obj *elem = list_obj->list.elem[i];
        retain_obj(elem);
        push_obj(left, elem);
    }
    for (size_t i = mid; i < len; i++) {
        tf_obj *elem = list_obj->list.elem[i];
        retain_obj(elem);
        push_obj(right, elem);
    }

    stack_push(ctx, left);
    stack_push(ctx, right);

    release_obj(list_obj);
    return TF_OK;
}

tf_ret tf_merge_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *pred = stack_pop_type(ctx, TF_OBJ_TYPE_LIST);
    tf_obj *l2 = stack_pop_type(ctx, TF_OBJ_TYPE_LIST);
    tf_obj *l1 = stack_pop_type(ctx, TF_OBJ_TYPE_LIST);

    if (!pred || !l1 || !l2) {
        if (pred) release_obj(pred);
        if (l1) release_obj(l1);
        if (l2) release_obj(l2);
        return TF_ERR;
    }

    tf_obj *res = init_list_obj();
    size_t i1 = 0, i2 = 0;

    while (i1 < l1->list.len && i2 < l2->list.len) {
        tf_obj *o1 = l1->list.elem[i1];
        tf_obj *o2 = l2->list.elem[i2];

        /* Setup stack for predicate: ( x y -- bool ) */
        stack_push(ctx, o1);
        retain_obj(o1);
        stack_push(ctx, o2);
        retain_obj(o2);

        if (exec(ctx, pred) != TF_OK) {
            release_obj(pred);
            release_obj(l1);
            release_obj(l2);
            release_obj(res);
            return TF_ERR;
        }

        tf_obj *bool_obj = stack_pop_type(ctx, TF_OBJ_TYPE_BOOL);
        if (!bool_obj) {
            release_obj(pred);
            release_obj(l1);
            release_obj(l2);
            release_obj(res);
            return TF_ERR;
        }

        bool take_left = bool_obj->b;
        release_obj(bool_obj);

        if (take_left) {
            push_obj(res, o1);
            retain_obj(o1);
            i1++;
        } else {
            push_obj(res, o2);
            retain_obj(o2);
            i2++;
        }
    }

    /* Append remaining */
    while (i1 < l1->list.len) {
        push_obj(res, l1->list.elem[i1]);
        retain_obj(l1->list.elem[i1]);
        i1++;
    }
    while (i2 < l2->list.len) {
        push_obj(res, l2->list.elem[i2]);
        retain_obj(l2->list.elem[i2]);
        i2++;
    }

    stack_push(ctx, res);
    release_obj(pred);
    release_obj(l1);
    release_obj(l2);
    return TF_OK;
}

tf_ret tf_range(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *end_obj = stack_peek(ctx, 0);
    tf_obj *start_obj = stack_peek(ctx, 1);
    if (start_obj->type != TF_OBJ_TYPE_INT || end_obj->type != TF_OBJ_TYPE_INT) {
        return TF_ERR;
    }

    int start = start_obj->i;
    int end = end_obj->i;
    if (end < start) return TF_ERR;

    end_obj = stack_pop(ctx);
    start_obj = stack_pop(ctx);

    tf_obj *result = init_list_obj();
    for (int i = start; i < end; i++) {
        push_obj(result, create_int_obj(i));
    }

    stack_push(ctx, result);
    release_obj(end_obj);
    release_obj(start_obj);
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
    if (ms_obj->i < 0) return TF_ERR;
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

tf_ret tf_load_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = stack_peek(ctx, 0);
    if (path->type != TF_OBJ_TYPE_STR) return TF_ERR;

    path = stack_pop(ctx);
    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        tf_console_runtime_errorf("failed to load '%s'\n", path->str.ptr);
        release_obj(path);
        return TF_ERR;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        release_obj(path);
        return TF_ERR;
    }

    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        release_obj(path);
        return TF_ERR;
    }
    rewind(fp);

    char *source = xmalloc((size_t)size + 1);
    size_t n_read = fread(source, 1, (size_t)size, fp);
    source[n_read] = '\0';
    fclose(fp);

    tf_obj *prg = lexer(source);
    free(source);
    if (!prg) {
        release_obj(path);
        return TF_ERR;
    }

    tf_ret result = exec(ctx, prg);
    release_obj(prg);
    release_obj(path);
    return result;
}

tf_ret tf_exit(tf_ctx *ctx) {
    (void)ctx;
    exit(0);
    return TF_OK;
}

tf_ret tf_readf(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        release_obj(path);
        return TF_ERR;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buf = xmalloc((size_t)size + 1);
    fread(buf, 1, (size_t)size, fp);
    buf[size] = '\0';
    fclose(fp);

    stack_push(ctx, create_string_obj(buf, (size_t)size));
    free(buf);
    release_obj(path);
    return TF_OK;
}

tf_ret tf_writef(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *content = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    tf_obj *path = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!content || !path) {
        if (content) release_obj(content);
        if (path) release_obj(path);
        return TF_ERR;
    }

    FILE *fp = fopen(path->str.ptr, "wb");
    if (!fp) {
        release_obj(content);
        release_obj(path);
        return TF_ERR;
    }

    fwrite(content->str.ptr, 1, content->str.len, fp);
    fclose(fp);

    release_obj(content);
    release_obj(path);
    return TF_OK;
}

tf_ret tf_readl(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    FILE *fp = fopen(path->str.ptr, "r");
    if (!fp) {
        release_obj(path);
        return TF_ERR;
    }

    tf_obj *list = init_list_obj();
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        push_obj(list, create_string_obj(line, strlen(line)));
    }
    fclose(fp);

    stack_push(ctx, list);
    release_obj(path);
    return TF_OK;
}

tf_ret tf_exists(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    FILE *fp = fopen(path->str.ptr, "r");
    bool exists = (fp != NULL);
    if (fp) fclose(fp);

    stack_push(ctx, create_bool_obj(exists));
    release_obj(path);
    return TF_OK;
}

tf_ret tf_delf(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    remove(path->str.ptr);

    release_obj(path);
    return TF_OK;
}
