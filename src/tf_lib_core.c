#include "tf_lib.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include "tf_alloc.h"
#include "tf_obj.h"

/* Helper for binary math operations */
static bool checked_int_add(int a, int b, int *out) {
    if ((b > 0 && a > INT_MAX - b) ||
        (b < 0 && a < INT_MIN - b)) {
        return false;
    }
    *out = a + b;
    return true;
}

static bool checked_int_sub(int a, int b, int *out) {
    if ((b < 0 && a > INT_MAX + b) ||
        (b > 0 && a < INT_MIN + b)) {
        return false;
    }
    *out = a - b;
    return true;
}

static bool checked_int_mul(int a, int b, int *out) {
    if (a > 0) {
        if ((b > 0 && a > INT_MAX / b) ||
            (b < 0 && b < INT_MIN / a)) {
            return false;
        }
    } else if (a < 0) {
        if ((b > 0 && a < INT_MIN / b) ||
            (b < 0 && a < INT_MAX / b)) {
            return false;
        }
    }
    *out = a * b;
    return true;
}

static tf_ret binary_math(tf_ctx *ctx, char op) {
    if (!tf_ctx_require_number(ctx, 1) || !tf_ctx_require_number(ctx, 0)) {
        return TF_ERR;
    }

    tf_obj *b = tf_stack_pop(ctx);
    tf_obj *a = tf_stack_pop(ctx);

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
                tf_stack_push(ctx, a);
                tf_stack_push(ctx, b);
                tf_ctx_runtime_errorf(ctx, "'%s' cannot divide by zero\n",
                                      ctx->current_word);
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
            tf_stack_push(ctx, a);
            tf_stack_push(ctx, b);
            return TF_ERR;
        }
        tf_stack_push(ctx, tf_obj_new_float(fresult));
    } else {
        int ia = a->i;
        int ib = b->i;
        int iresult = 0;
        bool ok = true;

        switch (op) {
        case '+':
            ok = checked_int_add(ia, ib, &iresult);
            break;
        case '-':
            ok = checked_int_sub(ia, ib, &iresult);
            break;
        case '*':
            ok = checked_int_mul(ia, ib, &iresult);
            break;
        case '/':
        case '%':
            if (ib == 0) {
                tf_stack_push(ctx, a);
                tf_stack_push(ctx, b);
                tf_ctx_runtime_errorf(ctx, "'%s' cannot divide by zero\n",
                                      ctx->current_word);
                return TF_ERR;
            }
            if (ia == INT_MIN && ib == -1) {
                tf_stack_push(ctx, a);
                tf_stack_push(ctx, b);
                tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                                      ctx->current_word);
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
            tf_stack_push(ctx, a);
            tf_stack_push(ctx, b);
            return TF_ERR;
        }
        if (!ok) {
            tf_stack_push(ctx, a);
            tf_stack_push(ctx, b);
            tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                                  ctx->current_word);
            return TF_ERR;
        }
        tf_stack_push(ctx, tf_obj_new_int(iresult));
    }

    tf_obj_release(a);
    tf_obj_release(b);
    return TF_OK;
}

tf_ret tf_add(tf_ctx *ctx) {
    return binary_math(ctx, '+');
}
tf_ret tf_sub(tf_ctx *ctx) {
    return binary_math(ctx, '-');
}
tf_ret tf_mul(tf_ctx *ctx) {
    return binary_math(ctx, '*');
}
tf_ret tf_div(tf_ctx *ctx) {
    return binary_math(ctx, '/');
}
tf_ret tf_mod(tf_ctx *ctx) {
    return binary_math(ctx, '%');
}
tf_ret tf_max(tf_ctx *ctx) {
    return binary_math(ctx, 'M');
}
tf_ret tf_min(tf_ctx *ctx) {
    return binary_math(ctx, 'm');
}

tf_ret tf_neg(tf_ctx *ctx) {
    if (!tf_ctx_require_number(ctx, 0)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_INT) {
        if (a->i == INT_MIN) {
            tf_stack_push(ctx, a);
            tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                                  ctx->current_word);
            return TF_ERR;
        }
        tf_stack_push(ctx, tf_obj_new_int(-a->i));
    } else if (a->type == TF_OBJ_TYPE_FLOAT) {
        tf_stack_push(ctx, tf_obj_new_float(-a->f));
    }
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_abs(tf_ctx *ctx) {
    if (!tf_ctx_require_number(ctx, 0)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_INT) {
        if (a->i == INT_MIN) {
            tf_stack_push(ctx, a);
            tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                                  ctx->current_word);
            return TF_ERR;
        }
        tf_stack_push(ctx, tf_obj_new_int(a->i < 0 ? -a->i : a->i));
    } else if (a->type == TF_OBJ_TYPE_FLOAT) {
        tf_stack_push(ctx, tf_obj_new_float(a->f < 0 ? -a->f : a->f));
    }
    tf_obj_release(a);
    return TF_OK;
}

/* Transcendental Math */

static tf_ret unary_float_math(tf_ctx *ctx, double (*func)(double)) {
    if (!tf_ctx_require_number(ctx, 0)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    double val = (a->type == TF_OBJ_TYPE_FLOAT) ? (double)a->f : (double)a->i;
    tf_stack_push(ctx, tf_obj_new_float((float)func(val)));
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_sqrt(tf_ctx *ctx) { return unary_float_math(ctx, sqrt); }
tf_ret tf_exp(tf_ctx *ctx) { return unary_float_math(ctx, exp); }
tf_ret tf_log(tf_ctx *ctx) { return unary_float_math(ctx, log); }
tf_ret tf_log10(tf_ctx *ctx) { return unary_float_math(ctx, log10); }
tf_ret tf_sin(tf_ctx *ctx) { return unary_float_math(ctx, sin); }
tf_ret tf_cos(tf_ctx *ctx) { return unary_float_math(ctx, cos); }
tf_ret tf_tan(tf_ctx *ctx) { return unary_float_math(ctx, tan); }

tf_ret tf_pow(tf_ctx *ctx) {
    if (!tf_ctx_require_number(ctx, 1) || !tf_ctx_require_number(ctx, 0)) {
        return TF_ERR;
    }
    tf_obj *b = tf_stack_pop(ctx);
    tf_obj *a = tf_stack_pop(ctx);
    double val_a = (a->type == TF_OBJ_TYPE_FLOAT) ? (double)a->f : (double)a->i;
    double val_b = (b->type == TF_OBJ_TYPE_FLOAT) ? (double)b->f : (double)b->i;
    tf_stack_push(ctx, tf_obj_new_float((float)pow(val_a, val_b)));
    tf_obj_release(a);
    tf_obj_release(b);
    return TF_OK;
}

static tf_ret unary_int_math(tf_ctx *ctx, double (*func)(double)) {
    if (!tf_ctx_require_number(ctx, 0)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    double val = (a->type == TF_OBJ_TYPE_FLOAT) ? (double)a->f : (double)a->i;
    tf_stack_push(ctx, tf_obj_new_int((int)func(val)));
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_floor(tf_ctx *ctx) { return unary_int_math(ctx, floor); }
tf_ret tf_ceil(tf_ctx *ctx) { return unary_int_math(ctx, ceil); }
tf_ret tf_round(tf_ctx *ctx) { return unary_int_math(ctx, round); }
tf_ret tf_pred(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) return TF_ERR;
    tf_obj *a = tf_stack_pop_type(ctx, TF_OBJ_TYPE_INT);
    if (a->i == INT_MIN) {
        tf_stack_push(ctx, a);
        tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                              ctx->current_word);
        return TF_ERR;
    }
    tf_stack_push(ctx, tf_obj_new_int(a->i - 1));
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_succ(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) return TF_ERR;
    tf_obj *a = tf_stack_pop_type(ctx, TF_OBJ_TYPE_INT);
    if (a->i == INT_MAX) {
        tf_stack_push(ctx, a);
        tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                              ctx->current_word);
        return TF_ERR;
    }
    tf_stack_push(ctx, tf_obj_new_int(a->i + 1));
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_square(tf_ctx *ctx) {
    if (!tf_ctx_require_number(ctx, 0)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_INT) {
        int result = 0;
        if (!checked_int_mul(a->i, a->i, &result)) {
            tf_stack_push(ctx, a);
            tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                                  ctx->current_word);
            return TF_ERR;
        }
        tf_stack_push(ctx, tf_obj_new_int(result));
    } else if (a->type == TF_OBJ_TYPE_FLOAT) {
        tf_stack_push(ctx, tf_obj_new_float(a->f * a->f));
    }
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_cube(tf_ctx *ctx) {
    if (!tf_ctx_require_number(ctx, 0)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_INT) {
        int squared = 0;
        int result = 0;
        if (!checked_int_mul(a->i, a->i, &squared) ||
            !checked_int_mul(squared, a->i, &result)) {
            tf_stack_push(ctx, a);
            tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                                  ctx->current_word);
            return TF_ERR;
        }
        tf_stack_push(ctx, tf_obj_new_int(result));
    } else if (a->type == TF_OBJ_TYPE_FLOAT) {
        tf_stack_push(ctx, tf_obj_new_float(a->f * a->f * a->f));
    }
    tf_obj_release(a);
    return TF_OK;
}

#define TF_PI_CONST 3.14159265358979323846f
#define TF_E_CONST 2.71828182845904523536f

tf_ret tf_pi(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_float(TF_PI_CONST));
    return TF_OK;
}

tf_ret tf_e(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_float(TF_E_CONST));
    return TF_OK;
}

tf_ret tf_tau(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_float(2.0f * TF_PI_CONST));
    return TF_OK;
}

/* Helper for logical and bitwise operations */
static tf_ret logic_op(tf_ctx *ctx, char op) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;

    tf_obj *b = tf_stack_pop(ctx);
    tf_obj *a = tf_stack_pop(ctx);

    if (a->type == TF_OBJ_TYPE_BOOL && b->type == TF_OBJ_TYPE_BOOL) {
        bool res = false;
        if (op == '&')
            res = a->b && b->b;
        else if (op == '|')
            res = a->b || b->b;
        else if (op == '^')
            res = a->b ^ b->b;
        tf_stack_push(ctx, tf_obj_new_bool(res));
    } else if (a->type == TF_OBJ_TYPE_INT && b->type == TF_OBJ_TYPE_INT) {
        int res = 0;
        if (op == '&')
            res = a->i & b->i;
        else if (op == '|')
            res = a->i | b->i;
        else if (op == '^')
            res = a->i ^ b->i;
        tf_stack_push(ctx, tf_obj_new_int(res));
    } else {
        tf_stack_push(ctx, a);
        tf_stack_push(ctx, b);
        tf_ctx_runtime_errorf(
            ctx, "'%s' expected both values to be bools or both values to be ints\n",
            ctx->current_word);
        return TF_ERR;
    }

    tf_obj_release(a);
    tf_obj_release(b);
    return TF_OK;
}

static tf_ret shift_op(tf_ctx *ctx, bool left) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_INT) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) {
        return TF_ERR;
    }

    tf_obj *b = tf_stack_pop(ctx);
    tf_obj *a = tf_stack_pop(ctx);

    if (b->i < 0 || b->i >= (int)(sizeof(unsigned int) * CHAR_BIT)) {
        tf_stack_push(ctx, a);
        tf_stack_push(ctx, b);
        tf_ctx_runtime_errorf(ctx, "'%s' shift count must be between 0 and %zu\n",
                              ctx->current_word,
                              sizeof(unsigned int) * CHAR_BIT - 1);
        return TF_ERR;
    }
    unsigned int value = (unsigned int)a->i;
    unsigned int res = left ? (value << b->i) : (value >> b->i);
    tf_stack_push(ctx, tf_obj_new_int((int)res));

    tf_obj_release(a);
    tf_obj_release(b);
    return TF_OK;
}

tf_ret tf_shl(tf_ctx *ctx) {
    return shift_op(ctx, true);
}
tf_ret tf_shr(tf_ctx *ctx) {
    return shift_op(ctx, false);
}
tf_ret tf_and(tf_ctx *ctx) {
    return logic_op(ctx, '&');
}
tf_ret tf_or(tf_ctx *ctx) {
    return logic_op(ctx, '|');
}
tf_ret tf_xor(tf_ctx *ctx) {
    return logic_op(ctx, '^');
}

tf_ret tf_not(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    if (a->type == TF_OBJ_TYPE_BOOL) {
        tf_stack_push(ctx, tf_obj_new_bool(!a->b));
    } else if (a->type == TF_OBJ_TYPE_INT) {
        tf_stack_push(ctx, tf_obj_new_int(~a->i));
    } else {
        tf_stack_push(ctx, a);
        tf_ctx_runtime_errorf(ctx, "'%s' expected bool or int, found %s\n",
                              ctx->current_word, tf_obj_type_name(a));
        return TF_ERR;
    }
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_dup(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_peek(ctx, 0);
    tf_stack_push(ctx, o);
    tf_obj_retain(o);
    return TF_OK;
}

tf_ret tf_drop(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_swap(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    tf_obj *b = tf_stack_pop(ctx);
    tf_stack_push(ctx, a);
    tf_stack_push(ctx, b);
    return TF_OK;
}

tf_ret tf_over(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *o = tf_stack_peek(ctx, 1);
    tf_stack_push(ctx, o);
    tf_obj_retain(o);
    return TF_OK;
}

tf_ret tf_rot(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 3)) return TF_ERR;
    tf_obj *c = tf_stack_pop(ctx);
    tf_obj *b = tf_stack_pop(ctx);
    tf_obj *a = tf_stack_pop(ctx);
    tf_stack_push(ctx, b);
    tf_stack_push(ctx, c);
    tf_stack_push(ctx, a);
    return TF_OK;
}

tf_ret tf_swapd(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 3)) return TF_ERR;
    tf_obj *c = tf_stack_pop(ctx);
    tf_obj *b = tf_stack_pop(ctx);
    tf_obj *a = tf_stack_pop(ctx);
    tf_stack_push(ctx, b);
    tf_stack_push(ctx, a);
    tf_stack_push(ctx, c);
    return TF_OK;
}

tf_ret tf_nip(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    tf_obj *b = tf_stack_pop(ctx);
    tf_obj_release(b);
    tf_stack_push(ctx, a);
    return TF_OK;
}

tf_ret tf_tuck(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    tf_obj *b = tf_stack_pop(ctx);
    tf_stack_push(ctx, a);
    tf_stack_push(ctx, b);
    tf_stack_push(ctx, a);
    tf_obj_retain(a);
    return TF_OK;
}

tf_ret tf_pick(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) return TF_ERR;
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *o = tf_stack_peek(ctx, 0);
    int pos = o->i;
    size_t len = tf_stack_len(ctx);
    if (pos < 0 || len < (size_t)pos + 2) {
        tf_ctx_runtime_errorf(ctx, "'%s' stack position %d is out of range\n",
                              ctx->current_word, pos);
        return TF_ERR;
    }

    o = tf_stack_pop(ctx);
    tf_obj_release(o);

    tf_obj *a = tf_stack_peek(ctx, (size_t)pos);
    tf_stack_push(ctx, a);
    tf_obj_retain(a);
    return TF_OK;
}

tf_ret tf_roll(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) return TF_ERR;
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *o = tf_stack_peek(ctx, 0);
    int pos = o->i;
    if (pos < 0 || tf_stack_len(ctx) < (size_t)pos + 2) {
        tf_ctx_runtime_errorf(ctx, "'%s' stack position %d is out of range\n",
                              ctx->current_word, pos);
        return TF_ERR;
    }

    o = tf_stack_pop(ctx);
    tf_obj_release(o);
    if (pos == 0) return TF_OK;

    tf_obj **temp_stack = tf_xmalloc(sizeof(tf_obj *) * (size_t)pos);
    for (int i = 0; i < pos; i++) { temp_stack[i] = tf_stack_pop(ctx); }
    tf_obj *b = tf_stack_pop(ctx);
    for (int i = pos - 1; i >= 0; i--) tf_stack_push(ctx, temp_stack[i]);
    tf_stack_push(ctx, b);
    free(temp_stack);
    return TF_OK;
}

tf_ret tf_empty(tf_ctx *ctx) {
    while (tf_stack_len(ctx) > 0) {
        tf_obj *o = tf_stack_pop(ctx);
        tf_obj_release(o);
    }
    return TF_OK;
}

static tf_ret compare_op(tf_ctx *ctx, const char *op) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;

    tf_obj *b = tf_stack_pop(ctx);
    tf_obj *a = tf_stack_pop(ctx);

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
    } else if (a->type == TF_OBJ_TYPE_STR && b->type == TF_OBJ_TYPE_STR) {
        int cmp = tf_obj_compare_string(a, b);
        if (!strcmp(op, "=="))
            result = cmp == 0;
        else if (!strcmp(op, "!="))
            result = cmp != 0;
        else if (!strcmp(op, "<"))
            result = cmp < 0;
        else if (!strcmp(op, ">"))
            result = cmp > 0;
        else if (!strcmp(op, "<="))
            result = cmp <= 0;
        else if (!strcmp(op, ">="))
            result = cmp >= 0;
    } else if (a->type == b->type) {
        if (!strcmp(op, "==")) {
            result = tf_obj_equal(a, b);
        } else if (!strcmp(op, "!=")) {
            result = !tf_obj_equal(a, b);
        } else {
            tf_stack_push(ctx, a);
            tf_stack_push(ctx, b);
            tf_ctx_runtime_errorf(ctx,
                                  "'%s' cannot order values of type %s\n",
                                  ctx->current_word, tf_obj_type_name(a));
            return TF_ERR;
        }
    } else {
        if (!strcmp(op, "=="))
            result = false;
        else if (!strcmp(op, "!="))
            result = true;
        else {
            tf_stack_push(ctx, a);
            tf_stack_push(ctx, b);
            tf_ctx_runtime_errorf(
                ctx, "'%s' cannot compare %s with %s\n", ctx->current_word,
                tf_obj_type_name(a), tf_obj_type_name(b));
            return TF_ERR;
        }
    }

    tf_stack_push(ctx, tf_obj_new_bool(result));
    tf_obj_release(a);
    tf_obj_release(b);
    return TF_OK;
}

tf_ret tf_eq(tf_ctx *ctx) {
    return compare_op(ctx, "==");
}
tf_ret tf_ne(tf_ctx *ctx) {
    return compare_op(ctx, "!=");
}
tf_ret tf_lt(tf_ctx *ctx) {
    return compare_op(ctx, "<");
}
tf_ret tf_gt(tf_ctx *ctx) {
    return compare_op(ctx, ">");
}
tf_ret tf_le(tf_ctx *ctx) {
    return compare_op(ctx, "<=");
}
tf_ret tf_ge(tf_ctx *ctx) {
    return compare_op(ctx, ">=");
}
