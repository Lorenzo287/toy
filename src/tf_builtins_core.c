#include "tf_builtins.h"
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "tf_obj.h"

/* Helper for binary math operations */
static bool checked_int_add(int64_t a, int64_t b, int64_t *out) {
    if ((b > 0 && a > INT64_MAX - b) ||
        (b < 0 && a < INT64_MIN - b)) {
        return false;
    }
    *out = a + b;
    return true;
}

static bool checked_int_sub(int64_t a, int64_t b, int64_t *out) {
    if ((b < 0 && a > INT64_MAX + b) ||
        (b > 0 && a < INT64_MIN + b)) {
        return false;
    }
    *out = a - b;
    return true;
}

static bool checked_int_mul(int64_t a, int64_t b, int64_t *out) {
    if (a > 0) {
        if ((b > 0 && a > INT64_MAX / b) ||
            (b < 0 && b < INT64_MIN / a)) {
            return false;
        }
    } else if (a < 0) {
        if ((b > 0 && a < INT64_MIN / b) ||
            (b < 0 && a < INT64_MAX / b)) {
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
    tf_type a_type = tf_obj_typeof(a);
    tf_type b_type = tf_obj_typeof(b);

    if ((op == 'M' || op == 'm') && a_type != b_type &&
        !(a_type == TF_OBJ_TYPE_FLOAT && isnan(a->f)) &&
        !(b_type == TF_OBJ_TYPE_FLOAT && isnan(b->f))) {
        int order = tf_obj_compare_number(a, b);
        tf_obj *selected = op == 'M' ? (order >= 0 ? a : b)
                                     : (order <= 0 ? a : b);
        tf_obj *discarded = selected == a ? b : a;
        tf_stack_push(ctx, selected);
        tf_obj_release(discarded);
        return TF_OK;
    }

    bool is_float =
        a_type == TF_OBJ_TYPE_FLOAT || b_type == TF_OBJ_TYPE_FLOAT;

    if (is_float) {
        double fa = a_type == TF_OBJ_TYPE_FLOAT
                        ? a->f
                        : (double)tf_obj_int_value(a);
        double fb = b_type == TF_OBJ_TYPE_FLOAT
                        ? b->f
                        : (double)tf_obj_int_value(b);
        double fresult = 0;

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
            if (fb == 0.0) {
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
        int64_t ia = tf_obj_int_value(a);
        int64_t ib = tf_obj_int_value(b);
        int64_t iresult = 0;
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
            if (ib == 0) {
                tf_stack_push(ctx, a);
                tf_stack_push(ctx, b);
                tf_ctx_runtime_errorf(ctx, "'%s' cannot divide by zero\n",
                                      ctx->current_word);
                return TF_ERR;
            }
            if (ia == INT64_MIN && ib == -1) {
                tf_stack_push(ctx, a);
                tf_stack_push(ctx, b);
                tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                                      ctx->current_word);
                return TF_ERR;
            }
            iresult = ia / ib;
            break;
        case '%':
        case 'E':
            if (ib == 0) {
                tf_stack_push(ctx, a);
                tf_stack_push(ctx, b);
                tf_ctx_runtime_errorf(ctx, "'%s' cannot divide by zero\n",
                                      ctx->current_word);
                return TF_ERR;
            }
            iresult = ia == INT64_MIN && ib == -1 ? 0 : ia % ib;
            if (op == 'E' && iresult < 0) {
                uint64_t modulus = ib < 0 ? (uint64_t)(-(ib + 1)) + 1
                                          : (uint64_t)ib;
                iresult = (int64_t)((uint64_t)iresult + modulus);
            }
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
tf_ret tf_rem(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_INT) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) {
        return TF_ERR;
    }
    return binary_math(ctx, '%');
}
tf_ret tf_mod(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_INT) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) {
        return TF_ERR;
    }
    return binary_math(ctx, 'E');
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
    if (tf_obj_typeof(a) == TF_OBJ_TYPE_INT) {
        int64_t value = tf_obj_int_value(a);
        if (value == INT64_MIN) {
            tf_stack_push(ctx, a);
            tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                                  ctx->current_word);
            return TF_ERR;
        }
        tf_stack_push(ctx, tf_obj_new_int(-value));
    } else {
        tf_stack_push(ctx, tf_obj_new_float(-a->f));
    }
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_abs(tf_ctx *ctx) {
    if (!tf_ctx_require_number(ctx, 0)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    if (tf_obj_typeof(a) == TF_OBJ_TYPE_INT) {
        int64_t value = tf_obj_int_value(a);
        if (value == INT64_MIN) {
            tf_stack_push(ctx, a);
            tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                                  ctx->current_word);
            return TF_ERR;
        }
        tf_stack_push(ctx, tf_obj_new_int(value < 0 ? -value : value));
    } else {
        tf_stack_push(ctx, tf_obj_new_float(a->f < 0 ? -a->f : a->f));
    }
    tf_obj_release(a);
    return TF_OK;
}

/* Transcendental Math */

static tf_ret unary_float_math(tf_ctx *ctx, double (*func)(double)) {
    if (!tf_ctx_require_number(ctx, 0)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    double val = tf_obj_typeof(a) == TF_OBJ_TYPE_FLOAT
                     ? a->f
                     : (double)tf_obj_int_value(a);
    tf_stack_push(ctx, tf_obj_new_float(func(val)));
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
    double val_a = tf_obj_typeof(a) == TF_OBJ_TYPE_FLOAT
                       ? a->f
                       : (double)tf_obj_int_value(a);
    double val_b = tf_obj_typeof(b) == TF_OBJ_TYPE_FLOAT
                       ? b->f
                       : (double)tf_obj_int_value(b);
    tf_stack_push(ctx, tf_obj_new_float(pow(val_a, val_b)));
    tf_obj_release(a);
    tf_obj_release(b);
    return TF_OK;
}

static tf_ret unary_int_math(tf_ctx *ctx, double (*func)(double)) {
    if (!tf_ctx_require_number(ctx, 0)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    if (tf_obj_typeof(a) == TF_OBJ_TYPE_INT) {
        tf_stack_push(ctx, a);
        return TF_OK;
    }
    double result = func(a->f);
    if (!isfinite(result) || result < -9223372036854775808.0 ||
        result >= 9223372036854775808.0) {
        tf_stack_push(ctx, a);
        tf_ctx_runtime_errorf(ctx, "'%s' integer result would be out of range\n",
                              ctx->current_word);
        return TF_ERR;
    }
    tf_stack_push(ctx, tf_obj_new_int((int64_t)result));
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_floor(tf_ctx *ctx) { return unary_int_math(ctx, floor); }
tf_ret tf_ceil(tf_ctx *ctx) { return unary_int_math(ctx, ceil); }
tf_ret tf_round(tf_ctx *ctx) { return unary_int_math(ctx, round); }
tf_ret tf_pred(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) return TF_ERR;
    tf_obj *a = tf_stack_pop_type(ctx, TF_OBJ_TYPE_INT);
    int64_t value = tf_obj_int_value(a);
    if (value == INT64_MIN) {
        tf_stack_push(ctx, a);
        tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                              ctx->current_word);
        return TF_ERR;
    }
    tf_stack_push(ctx, tf_obj_new_int(value - 1));
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_succ(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) return TF_ERR;
    tf_obj *a = tf_stack_pop_type(ctx, TF_OBJ_TYPE_INT);
    int64_t value = tf_obj_int_value(a);
    if (value == INT64_MAX) {
        tf_stack_push(ctx, a);
        tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                              ctx->current_word);
        return TF_ERR;
    }
    tf_stack_push(ctx, tf_obj_new_int(value + 1));
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_square(tf_ctx *ctx) {
    if (!tf_ctx_require_number(ctx, 0)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    if (tf_obj_typeof(a) == TF_OBJ_TYPE_INT) {
        int64_t value = tf_obj_int_value(a);
        int64_t result = 0;
        if (!checked_int_mul(value, value, &result)) {
            tf_stack_push(ctx, a);
            tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                                  ctx->current_word);
            return TF_ERR;
        }
        tf_stack_push(ctx, tf_obj_new_int(result));
    } else {
        tf_stack_push(ctx, tf_obj_new_float(a->f * a->f));
    }
    tf_obj_release(a);
    return TF_OK;
}

tf_ret tf_cube(tf_ctx *ctx) {
    if (!tf_ctx_require_number(ctx, 0)) return TF_ERR;
    tf_obj *a = tf_stack_pop(ctx);
    if (tf_obj_typeof(a) == TF_OBJ_TYPE_INT) {
        int64_t value = tf_obj_int_value(a);
        int64_t squared = 0;
        int64_t result = 0;
        if (!checked_int_mul(value, value, &squared) ||
            !checked_int_mul(squared, value, &result)) {
            tf_stack_push(ctx, a);
            tf_ctx_runtime_errorf(ctx, "'%s' integer result would overflow\n",
                                  ctx->current_word);
            return TF_ERR;
        }
        tf_stack_push(ctx, tf_obj_new_int(result));
    } else {
        tf_stack_push(ctx, tf_obj_new_float(a->f * a->f * a->f));
    }
    tf_obj_release(a);
    return TF_OK;
}

#define TF_PI_CONST 3.14159265358979323846
#define TF_E_CONST 2.71828182845904523536

tf_ret tf_pi(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_float(TF_PI_CONST));
    return TF_OK;
}

tf_ret tf_e(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_float(TF_E_CONST));
    return TF_OK;
}

tf_ret tf_tau(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_float(2.0 * TF_PI_CONST));
    return TF_OK;
}

/* Helper for logical and bitwise operations */
static tf_ret logic_op(tf_ctx *ctx, char op) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;

    tf_obj *b = tf_stack_pop(ctx);
    tf_obj *a = tf_stack_pop(ctx);
    tf_type a_type = tf_obj_typeof(a);
    tf_type b_type = tf_obj_typeof(b);

    if (a_type == TF_OBJ_TYPE_BOOL && b_type == TF_OBJ_TYPE_BOOL) {
        bool res = false;
        if (op == '&')
            res = a->b && b->b;
        else if (op == '|')
            res = a->b || b->b;
        else if (op == '^')
            res = a->b ^ b->b;
        tf_stack_push(ctx, tf_obj_new_bool(res));
    } else if (a_type == TF_OBJ_TYPE_INT && b_type == TF_OBJ_TYPE_INT) {
        int64_t a_value = tf_obj_int_value(a);
        int64_t b_value = tf_obj_int_value(b);
        int64_t res = 0;
        if (op == '&')
            res = a_value & b_value;
        else if (op == '|')
            res = a_value | b_value;
        else if (op == '^')
            res = a_value ^ b_value;
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
    int64_t shift = tf_obj_int_value(b);

    if (shift < 0 || shift >= 64) {
        tf_stack_push(ctx, a);
        tf_stack_push(ctx, b);
        tf_ctx_runtime_errorf(ctx, "'%s' shift count must be between 0 and %zu\n",
                              ctx->current_word,
                              (size_t)63);
        return TF_ERR;
    }
    uint64_t value = (uint64_t)tf_obj_int_value(a);
    uint64_t bits = left ? (value << (unsigned)shift)
                         : (value >> (unsigned)shift);
    int64_t res = 0;
    memcpy(&res, &bits, sizeof res);
    tf_stack_push(ctx, tf_obj_new_int(res));

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
    tf_type type = tf_obj_typeof(a);
    if (type == TF_OBJ_TYPE_BOOL) {
        tf_stack_push(ctx, tf_obj_new_bool(!a->b));
    } else if (type == TF_OBJ_TYPE_INT) {
        tf_stack_push(ctx, tf_obj_new_int(~tf_obj_int_value(a)));
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
    int64_t pos = tf_obj_int_value(o);
    size_t len = tf_stack_len(ctx);
    if (pos < 0 || (uint64_t)pos > SIZE_MAX ||
        len < (size_t)pos + 2) {
        tf_ctx_runtime_errorf(ctx, "'%s' stack position %" PRId64
                                   " is out of range\n",
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
    int64_t pos = tf_obj_int_value(o);
    if (pos < 0 || (uint64_t)pos > SIZE_MAX ||
        tf_stack_len(ctx) < (size_t)pos + 2) {
        tf_ctx_runtime_errorf(ctx, "'%s' stack position %" PRId64
                                   " is out of range\n",
                              ctx->current_word, pos);
        return TF_ERR;
    }

    o = tf_stack_pop(ctx);
    tf_obj_release(o);
    if (pos == 0) return TF_OK;

    size_t len = tf_stack_len(ctx);
    size_t target = len - 1 - (size_t)pos;
    tf_obj *selected = ctx->data_stack->vector.elem[target];
    memmove(&ctx->data_stack->vector.elem[target],
            &ctx->data_stack->vector.elem[target + 1],
            sizeof(tf_obj *) * (size_t)pos);
    ctx->data_stack->vector.elem[len - 1] = selected;
    return TF_OK;
}

tf_ret tf_empty(tf_ctx *ctx) {
    while (tf_stack_len(ctx) > 0) {
        tf_obj *o = tf_stack_pop(ctx);
        tf_obj_release(o);
    }
    return TF_OK;
}

static bool compare_from_order(int order, const char *op) {
    if (!strcmp(op, "==")) return order == 0;
    if (!strcmp(op, "!=")) return order != 0;
    if (!strcmp(op, "<")) return order < 0;
    if (!strcmp(op, ">")) return order > 0;
    if (!strcmp(op, "<=")) return order <= 0;
    return order >= 0;
}

static bool compare_mixed_number(tf_obj *a, tf_obj *b, const char *op) {
    tf_obj *float_obj = tf_obj_typeof(a) == TF_OBJ_TYPE_FLOAT ? a : b;
    if (isnan(float_obj->f)) return !strcmp(op, "!=");
    return compare_from_order(tf_obj_compare_number(a, b), op);
}

static tf_ret compare_op(tf_ctx *ctx, const char *op) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;

    tf_obj *b = tf_stack_pop(ctx);
    tf_obj *a = tf_stack_pop(ctx);

    bool result = false;
    tf_type a_type = tf_obj_typeof(a);
    tf_type b_type = tf_obj_typeof(b);

    if ((a_type == TF_OBJ_TYPE_INT || a_type == TF_OBJ_TYPE_FLOAT) &&
        (b_type == TF_OBJ_TYPE_INT || b_type == TF_OBJ_TYPE_FLOAT)) {
        if (a_type == TF_OBJ_TYPE_INT && b_type == TF_OBJ_TYPE_INT) {
            int64_t a_value = tf_obj_int_value(a);
            int64_t b_value = tf_obj_int_value(b);
            if (!strcmp(op, "=="))
                result = a_value == b_value;
            else if (!strcmp(op, "!="))
                result = a_value != b_value;
            else if (!strcmp(op, "<"))
                result = a_value < b_value;
            else if (!strcmp(op, ">"))
                result = a_value > b_value;
            else if (!strcmp(op, "<="))
                result = a_value <= b_value;
            else if (!strcmp(op, ">="))
                result = a_value >= b_value;
        } else if (a_type == TF_OBJ_TYPE_FLOAT &&
                   b_type == TF_OBJ_TYPE_FLOAT) {
            double da = a->f;
            double db = b->f;
            if (!strcmp(op, "=="))
                result = da == db;
            else if (!strcmp(op, "!="))
                result = da != db;
            else if (!strcmp(op, "<"))
                result = da < db;
            else if (!strcmp(op, ">"))
                result = da > db;
            else if (!strcmp(op, "<="))
                result = da <= db;
            else if (!strcmp(op, ">="))
                result = da >= db;
        } else {
            result = compare_mixed_number(a, b, op);
        }
    } else if (a_type == TF_OBJ_TYPE_STR && b_type == TF_OBJ_TYPE_STR) {
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
    } else if (a_type == b_type) {
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
