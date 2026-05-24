#include "tf_lib.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
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
            break; /* max */
        case 'm':
            fresult = (fa < fb) ? fa : fb;
            break; /* min */
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
            if (ia == INT_MIN && ib == -1) {
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
            break; /* max */
        case 'm':
            iresult = (ia < ib) ? ia : ib;
            break; /* min */
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
        if (a->i == INT_MIN) {
            stack_push(ctx, a);
            return TF_ERR;
        }
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
        if (a->i == INT_MIN) {
            stack_push(ctx, a);
            return TF_ERR;
        }
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

/* Transcendental Math */

static tf_ret tf_unary_float_math(tf_ctx *ctx, double (*func)(double)) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    if (a->type != TF_OBJ_TYPE_INT && a->type != TF_OBJ_TYPE_FLOAT) {
        stack_push(ctx, a);
        return TF_ERR;
    }
    double val = (a->type == TF_OBJ_TYPE_FLOAT) ? (double)a->f : (double)a->i;
    stack_push(ctx, create_float_obj((float)func(val)));
    release_obj(a);
    return TF_OK;
}

tf_ret tf_sqrt(tf_ctx *ctx) { return tf_unary_float_math(ctx, sqrt); }
tf_ret tf_exp(tf_ctx *ctx) { return tf_unary_float_math(ctx, exp); }
tf_ret tf_log(tf_ctx *ctx) { return tf_unary_float_math(ctx, log); }
tf_ret tf_log10(tf_ctx *ctx) { return tf_unary_float_math(ctx, log10); }
tf_ret tf_sin(tf_ctx *ctx) { return tf_unary_float_math(ctx, sin); }
tf_ret tf_cos(tf_ctx *ctx) { return tf_unary_float_math(ctx, cos); }
tf_ret tf_tan(tf_ctx *ctx) { return tf_unary_float_math(ctx, tan); }

tf_ret tf_pow(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *b = stack_pop(ctx);
    tf_obj *a = stack_pop(ctx);
    if ((a->type != TF_OBJ_TYPE_INT && a->type != TF_OBJ_TYPE_FLOAT) ||
        (b->type != TF_OBJ_TYPE_INT && b->type != TF_OBJ_TYPE_FLOAT)) {
        stack_push(ctx, a);
        stack_push(ctx, b);
        return TF_ERR;
    }
    double val_a = (a->type == TF_OBJ_TYPE_FLOAT) ? (double)a->f : (double)a->i;
    double val_b = (b->type == TF_OBJ_TYPE_FLOAT) ? (double)b->f : (double)b->i;
    stack_push(ctx, create_float_obj((float)pow(val_a, val_b)));
    release_obj(a);
    release_obj(b);
    return TF_OK;
}

static tf_ret tf_unary_int_math(tf_ctx *ctx, double (*func)(double)) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    if (a->type != TF_OBJ_TYPE_INT && a->type != TF_OBJ_TYPE_FLOAT) {
        stack_push(ctx, a);
        return TF_ERR;
    }
    double val = (a->type == TF_OBJ_TYPE_FLOAT) ? (double)a->f : (double)a->i;
    stack_push(ctx, create_int_obj((int)func(val)));
    release_obj(a);
    return TF_OK;
}

tf_ret tf_floor(tf_ctx *ctx) { return tf_unary_int_math(ctx, floor); }
tf_ret tf_ceil(tf_ctx *ctx) { return tf_unary_int_math(ctx, ceil); }
tf_ret tf_round(tf_ctx *ctx) { return tf_unary_int_math(ctx, round); }
tf_ret tf_pred(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *a = stack_pop_type(ctx, TF_OBJ_TYPE_INT);
    if (!a) return TF_ERR;
    if (a->i == INT_MIN) {
        stack_push(ctx, a);
        return TF_ERR;
    }
    stack_push(ctx, create_int_obj(a->i - 1));
    release_obj(a);
    return TF_OK;
}

tf_ret tf_succ(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *a = stack_pop_type(ctx, TF_OBJ_TYPE_INT);
    if (!a) return TF_ERR;
    if (a->i == INT_MAX) {
        stack_push(ctx, a);
        return TF_ERR;
    }
    stack_push(ctx, create_int_obj(a->i + 1));
    release_obj(a);
    return TF_OK;
}

tf_ret tf_square(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_INT) {
        stack_push(ctx, create_int_obj(a->i * a->i));
    } else if (a->type == TF_OBJ_TYPE_FLOAT) {
        stack_push(ctx, create_float_obj(a->f * a->f));
    } else {
        stack_push(ctx, a);
        release_obj(a);
        return TF_ERR;
    }
    release_obj(a);
    return TF_OK;
}

tf_ret tf_cube(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *a = stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_INT) {
        stack_push(ctx, create_int_obj(a->i * a->i * a->i));
    } else if (a->type == TF_OBJ_TYPE_FLOAT) {
        stack_push(ctx, create_float_obj(a->f * a->f * a->f));
    } else {
        stack_push(ctx, a);
        release_obj(a);
        return TF_ERR;
    }
    release_obj(a);
    return TF_OK;
}

#define TF_PI_CONST 3.14159265358979323846f
#define TF_E_CONST 2.71828182845904523536f

tf_ret tf_pi(tf_ctx *ctx) {
    stack_push(ctx, create_float_obj(TF_PI_CONST));
    return TF_OK;
}

tf_ret tf_e(tf_ctx *ctx) {
    stack_push(ctx, create_float_obj(TF_E_CONST));
    return TF_OK;
}

tf_ret tf_tau(tf_ctx *ctx) {
    stack_push(ctx, create_float_obj(2.0f * TF_PI_CONST));
    return TF_OK;
}

/* Helper for logical and bitwise operations */
static tf_ret tf_logic(tf_ctx *ctx, char op) {
    if (stack_len(ctx) < 2) return TF_ERR;

    tf_obj *b = stack_pop(ctx);
    tf_obj *a = stack_pop(ctx);

    if (a->type == TF_OBJ_TYPE_BOOL && b->type == TF_OBJ_TYPE_BOOL) {
        bool res = false;
        if (op == '&')
            res = a->b && b->b;
        else if (op == '|')
            res = a->b || b->b;
        else if (op == '^')
            res = a->b ^ b->b;
        stack_push(ctx, create_bool_obj(res));
    } else if (a->type == TF_OBJ_TYPE_INT && b->type == TF_OBJ_TYPE_INT) {
        int res = 0;
        if (op == '&')
            res = a->i & b->i;
        else if (op == '|')
            res = a->i | b->i;
        else if (op == '^')
            res = a->i ^ b->i;
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
        if (b->i < 0 || b->i >= (int)(sizeof(unsigned int) * CHAR_BIT)) {
            stack_push(ctx, a);
            stack_push(ctx, b);
            return TF_ERR;
        }
        unsigned int value = (unsigned int)a->i;
        unsigned int res = left ? (value << b->i) : (value >> b->i);
        stack_push(ctx, create_int_obj((int)res));
    } else {
        stack_push(ctx, a);
        stack_push(ctx, b);
        return TF_ERR;
    }

    release_obj(a);
    release_obj(b);
    return TF_OK;
}

tf_ret tf_shl(tf_ctx *ctx) {
    return tf_shift(ctx, true);
}
tf_ret tf_shr(tf_ctx *ctx) {
    return tf_shift(ctx, false);
}
tf_ret tf_and(tf_ctx *ctx) {
    return tf_logic(ctx, '&');
}
tf_ret tf_or(tf_ctx *ctx) {
    return tf_logic(ctx, '|');
}
tf_ret tf_xor(tf_ctx *ctx) {
    return tf_logic(ctx, '^');
}

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
    for (int i = 0; i < pos; i++) { temp_stack[i] = stack_pop(ctx); }
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
#define TF_EQUAL_MAX_DEPTH 1024

static bool tf_obj_equal_inner(tf_obj *a, tf_obj *b, size_t depth) {
    if (a == b) return true;
    if (depth > TF_EQUAL_MAX_DEPTH) return false;
    if (a->type != b->type) return false;

    switch (a->type) {
    case TF_OBJ_TYPE_BOOL:
        return a->b == b->b;
    case TF_OBJ_TYPE_INT:
        return a->i == b->i;
    case TF_OBJ_TYPE_FLOAT:
        return a->f == b->f;
    case TF_OBJ_TYPE_STR:
    case TF_OBJ_TYPE_SYMBOL:
    case TF_OBJ_TYPE_VARFETCH:
        return compare_string_obj(a, b) == 0;
    case TF_OBJ_TYPE_LIST:
    case TF_OBJ_TYPE_VARLIST:
        if (a->list.len != b->list.len) return false;
        for (size_t i = 0; i < a->list.len; i++) {
            if (!tf_obj_equal_inner(a->list.elem[i], b->list.elem[i], depth + 1)) {
                return false;
            }
        }
        return true;
    default:
        return a == b;
    }
}

static bool tf_obj_equal(tf_obj *a, tf_obj *b) {
    return tf_obj_equal_inner(a, b, 0);
}

static tf_ret tf_compare(tf_ctx *ctx, const char *op) {
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
            result = tf_obj_equal(a, b);
        } else if (!strcmp(op, "!=")) {
            result = !tf_obj_equal(a, b);
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
