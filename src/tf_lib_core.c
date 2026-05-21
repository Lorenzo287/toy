#include "tf_lib.h"
#include <string.h>
#include <stdlib.h>
#include "tf_alloc.h"
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
        case '+': fresult = fa + fb; break;
        case '-': fresult = fa - fb; break;
        case '*': fresult = fa * fb; break;
        case '/':
            if (fb == 0.0f) {
                stack_push(ctx, a);
                stack_push(ctx, b);
                return TF_ERR;
            }
            fresult = fa / fb;
            break;
        case 'M': fresult = (fa > fb) ? fa : fb; break;  /* max */
        case 'm': fresult = (fa < fb) ? fa : fb; break;  /* min */
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
        case '+': iresult = ia + ib; break;
        case '-': iresult = ia - ib; break;
        case '*': iresult = ia * ib; break;
        case '/':
        case '%':
            if (ib == 0) {
                stack_push(ctx, a);
                stack_push(ctx, b);
                return TF_ERR;
            }
            if (op == '/') iresult = ia / ib;
            else iresult = ia % ib;
            break;
        case 'M': iresult = (ia > ib) ? ia : ib; break;  /* max */
        case 'm': iresult = (ia < ib) ? ia : ib; break;  /* min */
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

tf_ret tf_add(tf_ctx *ctx) { return tf_binary_math(ctx, '+'); }
tf_ret tf_sub(tf_ctx *ctx) { return tf_binary_math(ctx, '-'); }
tf_ret tf_mul(tf_ctx *ctx) { return tf_binary_math(ctx, '*'); }
tf_ret tf_div(tf_ctx *ctx) { return tf_binary_math(ctx, '/'); }
tf_ret tf_mod(tf_ctx *ctx) { return tf_binary_math(ctx, '%'); }
tf_ret tf_max(tf_ctx *ctx) { return tf_binary_math(ctx, 'M'); }
tf_ret tf_min(tf_ctx *ctx) { return tf_binary_math(ctx, 'm'); }

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
    tf_obj *o = stack_peek(ctx, 0);
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
    tf_obj *o = stack_peek(ctx, 1);
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

    tf_obj *a = stack_peek(ctx, (size_t)pos);
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
    if (pos == 0) return TF_OK;

    tf_obj **temp_stack = xmalloc(sizeof(tf_obj *) * (size_t)pos);
    for (int i = 0; i < pos; i++) {
        temp_stack[i] = stack_pop(ctx);
    }
    tf_obj *b = stack_pop(ctx);
    for (int i = pos - 1; i >= 0; i--) stack_push(ctx, temp_stack[i]);
    stack_push(ctx, b);
    free(temp_stack);
    return TF_OK;
}

tf_ret tf_empty(tf_ctx *ctx) {
    while (stack_len(ctx) > 0) {
        tf_obj *o = stack_pop(ctx);
        release_obj(o);
    }
    return TF_OK;
}

/* Comparison operations */
static tf_ret tf_compare(tf_ctx *ctx, const char *op) {
    if (stack_len(ctx) < 2) return TF_ERR;

    tf_obj *b = stack_pop(ctx);
    tf_obj *a = stack_pop(ctx);

    bool result = false;

    if ((a->type == TF_OBJ_TYPE_INT || a->type == TF_OBJ_TYPE_FLOAT) &&
        (b->type == TF_OBJ_TYPE_INT || b->type == TF_OBJ_TYPE_FLOAT)) {
        float fa = (a->type == TF_OBJ_TYPE_FLOAT) ? a->f : (float)a->i;
        float fb = (b->type == TF_OBJ_TYPE_FLOAT) ? b->f : (float)b->i;

        if (!strcmp(op, "==")) result = (fa == fb);
        else if (!strcmp(op, "!=")) result = (fa != fb);
        else if (!strcmp(op, "<")) result = (fa < fb);
        else if (!strcmp(op, ">")) result = (fa > fb);
        else if (!strcmp(op, "<=")) result = (fa <= fb);
        else if (!strcmp(op, ">=")) result = (fa >= fb);
    } else if (a->type == b->type) {
        if (!strcmp(op, "==")) {
            if (a->type == TF_OBJ_TYPE_BOOL) result = (a->b == b->b);
            else if (a->type == TF_OBJ_TYPE_STR || a->type == TF_OBJ_TYPE_SYMBOL)
                result = (compare_string_obj(a, b) == 0);
            else result = (a == b);
        } else if (!strcmp(op, "!=")) {
            if (a->type == TF_OBJ_TYPE_BOOL) result = (a->b != b->b);
            else if (a->type == TF_OBJ_TYPE_STR || a->type == TF_OBJ_TYPE_SYMBOL)
                result = (compare_string_obj(a, b) != 0);
            else result = (a != b);
        } else {
            stack_push(ctx, a);
            stack_push(ctx, b);
            return TF_ERR;
        }
    } else {
        if (!strcmp(op, "==")) result = false;
        else if (!strcmp(op, "!=")) result = true;
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

tf_ret tf_eq(tf_ctx *ctx) { return tf_compare(ctx, "=="); }
tf_ret tf_ne(tf_ctx *ctx) { return tf_compare(ctx, "!="); }
tf_ret tf_lt(tf_ctx *ctx) { return tf_compare(ctx, "<"); }
tf_ret tf_gt(tf_ctx *ctx) { return tf_compare(ctx, ">"); }
tf_ret tf_le(tf_ctx *ctx) { return tf_compare(ctx, "<="); }
tf_ret tf_ge(tf_ctx *ctx) { return tf_compare(ctx, ">="); }
