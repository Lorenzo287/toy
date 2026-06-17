#include "tf_lib.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "tf_alloc.h"
#include "tf_exec.h"
#include "tf_obj.h"

static bool is_char_obj(tf_obj *o) {
    return o->type == TF_OBJ_TYPE_STR && o->str.len == 1;
}

static bool is_countable(tf_obj *o) {
    return o->type == TF_OBJ_TYPE_LIST || o->type == TF_OBJ_TYPE_STR ||
           o->type == TF_OBJ_TYPE_MAP || o->type == TF_OBJ_TYPE_SET ||
           o->type == TF_OBJ_TYPE_DEQUE || o->type == TF_OBJ_TYPE_PQUEUE;
}

static bool require_countable(tf_ctx *ctx, size_t depth) {
    if (!tf_ctx_require_stack(ctx, depth + 1)) return false;
    tf_obj *o = tf_stack_peek(ctx, depth);
    if (is_countable(o)) return true;

    tf_ctx_runtime_errorf(ctx,
                          "'%s' expected finite collection at stack depth %zu, found %s\n",
                          ctx->current_word, depth, tf_obj_type_name(o));
    return false;
}

static bool require_char(tf_ctx *ctx, size_t depth) {
    if (!tf_ctx_require_stack(ctx, depth + 1)) return false;
    tf_obj *o = tf_stack_peek(ctx, depth);
    if (is_char_obj(o)) return true;

    tf_ctx_runtime_errorf(ctx, "'%s' expected one-character string at stack depth %zu, found %s\n",
                          ctx->current_word, depth, tf_obj_type_name(o));
    return false;
}

static bool index_in_bounds(tf_ctx *ctx, int idx, int len) {
    if (idx >= 0 && idx < len) return true;
    tf_ctx_runtime_errorf(ctx, "'%s' index %d is out of range for length %d\n",
                          ctx->current_word, idx, len);
    return false;
}

typedef enum {
    TF_CHAR_PRED_LETTER,
    TF_CHAR_PRED_DIGIT,
    TF_CHAR_PRED_ALNUM,
    TF_CHAR_PRED_SPACE,
    TF_CHAR_PRED_UPPER,
    TF_CHAR_PRED_LOWER,
    TF_CHAR_PRED_PUNCT
} char_pred;

static bool match_char_pred(unsigned char c, char_pred pred) {
    switch (pred) {
    case TF_CHAR_PRED_LETTER:
        return isalpha(c) != 0;
    case TF_CHAR_PRED_DIGIT:
        return isdigit(c) != 0;
    case TF_CHAR_PRED_ALNUM:
        return isalnum(c) != 0;
    case TF_CHAR_PRED_SPACE:
        return isspace(c) != 0;
    case TF_CHAR_PRED_UPPER:
        return isupper(c) != 0;
    case TF_CHAR_PRED_LOWER:
        return islower(c) != 0;
    case TF_CHAR_PRED_PUNCT:
        return ispunct(c) != 0;
    }
    return false;
}

static tf_ret char_predicate(tf_ctx *ctx, char_pred pred) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    bool result = is_char_obj(o) &&
                  match_char_pred((unsigned char)o->str.ptr[0], pred);
    tf_stack_push(ctx, tf_obj_new_bool(result));
    tf_obj_release(o);
    return TF_OK;
}

static char *find_mem(char *haystack, size_t haystack_len,
                         const char *needle, size_t needle_len) {
    if (needle_len == 0) return haystack;
    if (needle_len > haystack_len) return NULL;
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0) {
            return haystack + i;
        }
    }
    return NULL;
}

static tf_obj *map_clone(tf_obj *src) {
    tf_obj *result = tf_obj_new_map();
    for (size_t i = 0; i < src->map.len; i++) {
        tf_map_set(result, src->map.entries[i].key, src->map.entries[i].value);
    }
    return result;
}

static tf_obj *map_clone_without(tf_obj *src, tf_obj *key) {
    tf_obj *result = tf_obj_new_map();
    for (size_t i = 0; i < src->map.len; i++) {
        if (tf_obj_equal(src->map.entries[i].key, key)) continue;
        tf_map_set(result, src->map.entries[i].key, src->map.entries[i].value);
    }
    return result;
}

static tf_obj *set_clone(tf_obj *src) {
    tf_obj *result = tf_obj_new_set();
    for (size_t i = 0; i < src->set.len; i++) {
        tf_set_add(result, src->set.entries[i].item);
    }
    return result;
}

static tf_obj *set_clone_without(tf_obj *src, tf_obj *item) {
    tf_obj *result = tf_obj_new_set();
    for (size_t i = 0; i < src->set.len; i++) {
        if (tf_obj_equal(src->set.entries[i].item, item)) continue;
        tf_set_add(result, src->set.entries[i].item);
    }
    return result;
}

static bool priority_from_obj(tf_ctx *ctx, tf_obj *priority, double *out) {
    if (priority->type == TF_OBJ_TYPE_INT) {
        *out = priority->i;
    } else if (priority->type == TF_OBJ_TYPE_FLOAT) {
        *out = priority->f;
    } else {
        tf_ctx_runtime_errorf(ctx, "'%s' expected numeric priority, found %s\n",
                              ctx->current_word, tf_obj_type_name(priority));
        return false;
    }

    if (!isfinite(*out)) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected finite priority\n",
                              ctx->current_word);
        return false;
    }
    return true;
}

tf_ret tf_geth(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 1) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) {
        return TF_ERR;
    }
    tf_obj *idx_obj = tf_stack_peek(ctx, 0);
    tf_obj *coll_obj = tf_stack_peek(ctx, 1);

    int idx = idx_obj->i;
    if (coll_obj->type == TF_OBJ_TYPE_LIST) {
        if (!index_in_bounds(ctx, idx, (int)coll_obj->list.len)) return TF_ERR;
        idx_obj = tf_stack_pop(ctx);
        coll_obj = tf_stack_pop(ctx);
        tf_obj *result = coll_obj->list.elem[idx];
        tf_obj_retain(result);
        tf_stack_push(ctx, result);
        tf_obj_release(idx_obj);
        tf_obj_release(coll_obj);
        return TF_OK;
    }

    if (!index_in_bounds(ctx, idx, (int)coll_obj->str.len)) return TF_ERR;
    idx_obj = tf_stack_pop(ctx);
    coll_obj = tf_stack_pop(ctx);
    char buf[2] = { coll_obj->str.ptr[idx], 0 };
    tf_stack_push(ctx, tf_obj_new_string(buf, 1));
    tf_obj_release(idx_obj);
    tf_obj_release(coll_obj);
    return TF_OK;
}

tf_ret tf_seth(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 2) ||
        !tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_INT)) {
        return TF_ERR;
    }
    tf_obj *val = tf_stack_peek(ctx, 0);
    tf_obj *idx_obj = tf_stack_peek(ctx, 1);
    tf_obj *coll_obj = tf_stack_peek(ctx, 2);

    int idx = idx_obj->i;

    if (coll_obj->type == TF_OBJ_TYPE_LIST) {
        if (!index_in_bounds(ctx, idx, (int)coll_obj->list.len)) return TF_ERR;
        val = tf_stack_pop(ctx);
        idx_obj = tf_stack_pop(ctx);
        coll_obj = tf_stack_pop(ctx);

        tf_obj *new_list = tf_obj_new_list();
        for (size_t i = 0; i < coll_obj->list.len; i++) {
            tf_obj *elem = (i == (size_t)idx) ? val : coll_obj->list.elem[i];
            tf_obj_retain(elem);
            tf_list_push(new_list, elem);
        }
        tf_stack_push(ctx, new_list);

        tf_obj_release(val);
        tf_obj_release(idx_obj);
        tf_obj_release(coll_obj);
        return TF_OK;
    }

    if (!index_in_bounds(ctx, idx, (int)coll_obj->str.len)) return TF_ERR;
    if (!require_char(ctx, 0)) return TF_ERR;
    val = tf_stack_pop(ctx);
    idx_obj = tf_stack_pop(ctx);
    coll_obj = tf_stack_pop(ctx);

    tf_obj *new_str = tf_obj_new_string(coll_obj->str.ptr, coll_obj->str.len);
    new_str->str.ptr[idx] = val->str.ptr[0];
    tf_stack_push(ctx, new_str);

    tf_obj_release(val);
    tf_obj_release(idx_obj);
    tf_obj_release(coll_obj);
    return TF_OK;
}

tf_ret tf_slice(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 2) ||
        !tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_INT) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) {
        return TF_ERR;
    }
    tf_obj *end_obj = tf_stack_peek(ctx, 0);
    tf_obj *start_obj = tf_stack_peek(ctx, 1);
    tf_obj *coll = tf_stack_peek(ctx, 2);

    end_obj = tf_stack_pop(ctx);
    start_obj = tf_stack_pop(ctx);
    coll = tf_stack_pop(ctx);

    int start = start_obj->i;
    int end = end_obj->i;
    int len = (coll->type == TF_OBJ_TYPE_LIST) ? (int)coll->list.len : (int)coll->str.len;

    if (start < 0) start = 0;
    if (end < 0) end = 0;
    if (end > len) end = len;
    if (start > end) start = end;

    if (coll->type == TF_OBJ_TYPE_LIST) {
        tf_obj *res = tf_obj_new_list();
        for (int i = start; i < end; i++) {
            tf_obj_retain(coll->list.elem[i]);
            tf_list_push(res, coll->list.elem[i]);
        }
        tf_stack_push(ctx, res);
    } else {
        tf_stack_push(ctx, tf_obj_new_string(coll->str.ptr + start, end - start));
    }

    tf_obj_release(start_obj);
    tf_obj_release(end_obj);
    tf_obj_release(coll);
    return TF_OK;
}

tf_ret tf_len(tf_ctx *ctx) {
    if (!require_countable(ctx, 0)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    int len = 0;
    if (o->type == TF_OBJ_TYPE_LIST) {
        len = (int)o->list.len;
    } else if (o->type == TF_OBJ_TYPE_STR) {
        len = (int)o->str.len;
    } else if (o->type == TF_OBJ_TYPE_MAP) {
        len = (int)o->map.len;
    } else if (o->type == TF_OBJ_TYPE_SET) {
        len = (int)o->set.len;
    } else if (o->type == TF_OBJ_TYPE_DEQUE) {
        len = (int)o->deque.len;
    } else {
        len = (int)o->pqueue.len;
    }
    tf_stack_push(ctx, tf_obj_new_int(len));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_first(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 0)) return TF_ERR;
    tf_obj *seq = tf_stack_peek(ctx, 0);
    if (seq->type == TF_OBJ_TYPE_LIST && seq->list.len > 0) {
        seq = tf_stack_pop(ctx);
        tf_obj *result = seq->list.elem[0];
        tf_obj_retain(result);
        tf_stack_push(ctx, result);
        tf_obj_release(seq);
        return TF_OK;
    } else if (seq->type == TF_OBJ_TYPE_STR && seq->str.len > 0) {
        seq = tf_stack_pop(ctx);
        tf_stack_push(ctx, tf_obj_new_string(seq->str.ptr, 1));
        tf_obj_release(seq);
        return TF_OK;
    } else {
        tf_ctx_runtime_errorf(ctx, "'%s' expected non-empty sequence\n",
                              ctx->current_word);
        return TF_ERR;
    }
}

tf_ret tf_rest(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 0)) return TF_ERR;
    tf_obj *seq = tf_stack_peek(ctx, 0);
    if (seq->type == TF_OBJ_TYPE_LIST) {
        seq = tf_stack_pop(ctx);
        tf_obj *rest = tf_obj_new_list();
        for (size_t i = 1; i < seq->list.len; i++) {
            tf_obj *elem = seq->list.elem[i];
            tf_obj_retain(elem);
            tf_list_push(rest, elem);
        }
        tf_stack_push(ctx, rest);
        tf_obj_release(seq);
        return TF_OK;
    }

    seq = tf_stack_pop(ctx);
    size_t start = seq->str.len > 0 ? 1 : 0;
    tf_stack_push(ctx, tf_obj_new_string(seq->str.ptr + start,
                                      seq->str.len - start));
    tf_obj_release(seq);
    return TF_OK;
}

tf_ret tf_uncons(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 0)) return TF_ERR;
    tf_obj *seq = tf_stack_peek(ctx, 0);
    if (seq->type == TF_OBJ_TYPE_LIST && seq->list.len > 0) {
        seq = tf_stack_pop(ctx);
        tf_obj *head = seq->list.elem[0];
        tf_obj_retain(head);

        tf_obj *rest = tf_obj_new_list();
        for (size_t i = 1; i < seq->list.len; i++) {
            tf_obj *elem = seq->list.elem[i];
            tf_obj_retain(elem);
            tf_list_push(rest, elem);
        }

        tf_stack_push(ctx, head);
        tf_stack_push(ctx, rest);
        tf_obj_release(seq);
        return TF_OK;
    } else if (seq->type == TF_OBJ_TYPE_STR && seq->str.len > 0) {
        seq = tf_stack_pop(ctx);
        tf_stack_push(ctx, tf_obj_new_string(seq->str.ptr, 1));
        tf_stack_push(ctx, tf_obj_new_string(seq->str.ptr + 1, seq->str.len - 1));
        tf_obj_release(seq);
        return TF_OK;
    } else {
        tf_ctx_runtime_errorf(ctx, "'%s' expected non-empty sequence\n",
                              ctx->current_word);
        return TF_ERR;
    }
}

tf_ret tf_take(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 1) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) {
        return TF_ERR;
    }
    tf_obj *n_obj = tf_stack_peek(ctx, 0);
    tf_obj *coll = tf_stack_peek(ctx, 1);

    n_obj = tf_stack_pop(ctx);
    coll = tf_stack_pop(ctx);
    int n = n_obj->i;
    tf_obj_release(n_obj);
    if (n < 0) n = 0;

    if (coll->type == TF_OBJ_TYPE_LIST) {
        if (n > (int)coll->list.len) n = (int)coll->list.len;
        tf_obj *res = tf_obj_new_list();
        for (int i = 0; i < n; i++) {
            tf_obj_retain(coll->list.elem[i]);
            tf_list_push(res, coll->list.elem[i]);
        }
        tf_stack_push(ctx, res);
    } else {
        if (n > (int)coll->str.len) n = (int)coll->str.len;
        tf_stack_push(ctx, tf_obj_new_string(coll->str.ptr, n));
    }
    tf_obj_release(coll);
    return TF_OK;
}

tf_ret tf_dropn(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 1) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) {
        return TF_ERR;
    }
    tf_obj *n_obj = tf_stack_peek(ctx, 0);
    tf_obj *coll = tf_stack_peek(ctx, 1);

    n_obj = tf_stack_pop(ctx);
    coll = tf_stack_pop(ctx);
    int n = n_obj->i;
    tf_obj_release(n_obj);
    if (n < 0) n = 0;

    if (coll->type == TF_OBJ_TYPE_LIST) {
        if (n > (int)coll->list.len) n = (int)coll->list.len;
        tf_obj *res = tf_obj_new_list();
        for (size_t i = n; i < coll->list.len; i++) {
            tf_obj_retain(coll->list.elem[i]);
            tf_list_push(res, coll->list.elem[i]);
        }
        tf_stack_push(ctx, res);
    } else {
        if (n > (int)coll->str.len) n = (int)coll->str.len;
        tf_stack_push(ctx, tf_obj_new_string(coll->str.ptr + n, coll->str.len - n));
    }
    tf_obj_release(coll);
    return TF_OK;
}

tf_ret tf_cons(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *seq = tf_stack_peek(ctx, 0);

    if (seq->type == TF_OBJ_TYPE_LIST) {
        seq = tf_stack_pop(ctx);
        tf_obj *head = tf_stack_pop(ctx);
        tf_obj *result = tf_obj_new_list();
        tf_list_push(result, head);

        for (size_t i = 0; i < seq->list.len; i++) {
            tf_obj *elem = seq->list.elem[i];
            tf_obj_retain(elem);
            tf_list_push(result, elem);
        }

        tf_stack_push(ctx, result);
        tf_obj_release(seq);
        return TF_OK;
    } else if (seq->type == TF_OBJ_TYPE_STR) {
        if (!require_char(ctx, 1)) return TF_ERR;

        seq = tf_stack_pop(ctx);
        tf_obj *head = tf_stack_pop(ctx);
        char *new_ptr = tf_xmalloc(seq->str.len + 2);
        new_ptr[0] = head->str.ptr[0];
        memcpy(new_ptr + 1, seq->str.ptr, seq->str.len);
        new_ptr[seq->str.len + 1] = '\0';
        tf_stack_push(ctx, tf_obj_new_string(new_ptr, seq->str.len + 1));
        free(new_ptr);
        tf_obj_release(head);
        tf_obj_release(seq);
        return TF_OK;
    }

    tf_ctx_runtime_errorf(ctx, "'%s' expected sequence at stack depth 0, found %s\n",
                          ctx->current_word, tf_obj_type_name(seq));
    return TF_ERR;
}

tf_ret tf_append(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *seq = tf_stack_peek(ctx, 1);
    tf_obj *elem = tf_stack_peek(ctx, 0);

    if (seq->type == TF_OBJ_TYPE_LIST) {
        elem = tf_stack_pop(ctx);
        seq = tf_stack_pop(ctx);
        tf_obj *result = tf_obj_new_list();

        for (size_t i = 0; i < seq->list.len; i++) {
            tf_obj *item = seq->list.elem[i];
            tf_obj_retain(item);
            tf_list_push(result, item);
        }
        tf_list_push(result, elem);

        tf_stack_push(ctx, result);
        tf_obj_release(seq);
        return TF_OK;
    } else if (seq->type == TF_OBJ_TYPE_STR) {
        if (!require_char(ctx, 0)) return TF_ERR;

        elem = tf_stack_pop(ctx);
        seq = tf_stack_pop(ctx);
        char *new_ptr = tf_xmalloc(seq->str.len + 2);
        memcpy(new_ptr, seq->str.ptr, seq->str.len);
        new_ptr[seq->str.len] = elem->str.ptr[0];
        new_ptr[seq->str.len + 1] = '\0';
        tf_stack_push(ctx, tf_obj_new_string(new_ptr, seq->str.len + 1));
        free(new_ptr);
        tf_obj_release(elem);
        tf_obj_release(seq);
        return TF_OK;
    }

    tf_ctx_runtime_errorf(ctx, "'%s' expected sequence at stack depth 1, found %s\n",
                          ctx->current_word, tf_obj_type_name(seq));
    return TF_ERR;
}

tf_ret tf_concat(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *right = tf_stack_peek(ctx, 0);
    tf_obj *left = tf_stack_peek(ctx, 1);

    if (left->type == TF_OBJ_TYPE_STR && right->type == TF_OBJ_TYPE_STR) {
        right = tf_stack_pop(ctx);
        left = tf_stack_pop(ctx);
        size_t new_len = left->str.len + right->str.len;
        char *new_ptr = tf_xmalloc(new_len + 1);
        memcpy(new_ptr, left->str.ptr, left->str.len);
        memcpy(new_ptr + left->str.len, right->str.ptr, right->str.len);
        new_ptr[new_len] = '\0';
        tf_stack_push(ctx, tf_obj_new_string(new_ptr, new_len));
        free(new_ptr);
        tf_obj_release(left);
        tf_obj_release(right);
        return TF_OK;
    }

    if (left->type != TF_OBJ_TYPE_LIST || right->type != TF_OBJ_TYPE_LIST) {
        tf_ctx_runtime_errorf(
            ctx, "'%s' expected both values to be strings or both values to be lists\n",
            ctx->current_word);
        return TF_ERR;
    }

    right = tf_stack_pop(ctx);
    left = tf_stack_pop(ctx);
    tf_obj *result = tf_obj_new_list();
    for (size_t i = 0; i < left->list.len; i++) {
        tf_obj *elem = left->list.elem[i];
        tf_obj_retain(elem);
        tf_list_push(result, elem);
    }
    for (size_t i = 0; i < right->list.len; i++) {
        tf_obj *elem = right->list.elem[i];
        tf_obj_retain(elem);
        tf_list_push(result, elem);
    }

    tf_stack_push(ctx, result);
    tf_obj_release(left);
    tf_obj_release(right);
    return TF_OK;
}

tf_ret tf_reverse(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 0)) return TF_ERR;
    tf_obj *seq = tf_stack_pop(ctx);

    if (seq->type == TF_OBJ_TYPE_LIST) {
        tf_obj *result = tf_obj_new_list();
        for (size_t i = seq->list.len; i > 0; i--) {
            tf_obj *elem = seq->list.elem[i - 1];
            tf_obj_retain(elem);
            tf_list_push(result, elem);
        }
        tf_stack_push(ctx, result);
        tf_obj_release(seq);
        return TF_OK;
    }

    char *buf = tf_xmalloc(seq->str.len + 1);
    for (size_t i = 0; i < seq->str.len; i++) {
        buf[i] = seq->str.ptr[seq->str.len - 1 - i];
    }
    buf[seq->str.len] = '\0';
    tf_stack_push(ctx, tf_obj_new_string(buf, seq->str.len));
    free(buf);
    tf_obj_release(seq);
    return TF_OK;
}

tf_ret tf_split_string(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_STR) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }
    tf_obj *arg2 = tf_stack_pop(ctx);
    tf_obj *arg1 = tf_stack_pop(ctx);

    tf_obj *result = tf_obj_new_list();
    char *start = arg1->str.ptr;
    char *sep = arg2->str.ptr;
    size_t sep_len = arg2->str.len;
    size_t remaining = arg1->str.len;

    if (sep_len == 0) {
        for (size_t i = 0; i < arg1->str.len; i++) {
            tf_list_push(result, tf_obj_new_string(arg1->str.ptr + i, 1));
        }
    } else {
        char *p;
        while ((p = find_mem(start, remaining, sep, sep_len)) != NULL) {
            tf_list_push(result, tf_obj_new_string(start, p - start));
            size_t consumed = (size_t)(p - start) + sep_len;
            start = p + sep_len;
            remaining -= consumed;
        }
        tf_list_push(result, tf_obj_new_string(start, remaining));
    }

    tf_stack_push(ctx, result);
    tf_obj_release(arg1);
    tf_obj_release(arg2);
    return TF_OK;
}

tf_ret tf_to_map(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_LIST)) return TF_ERR;
    tf_obj *pairs = tf_stack_pop_type(ctx, TF_OBJ_TYPE_LIST);
    tf_obj *map = tf_obj_new_map();

    for (size_t i = 0; i < pairs->list.len; i++) {
        tf_obj *pair = pairs->list.elem[i];
        if (pair->type != TF_OBJ_TYPE_LIST || pair->list.len != 2) {
            tf_ctx_runtime_errorf(
                ctx, "'%s' expected item %zu to be a two-item list\n",
                ctx->current_word, i);
            tf_obj_release(map);
            tf_obj_release(pairs);
            return TF_ERR;
        }

        tf_obj *key = pair->list.elem[0];
        tf_obj *value = pair->list.elem[1];
        if (!tf_obj_hashable(key)) {
            tf_ctx_runtime_errorf(ctx, "'%s' expected hashable key at item %zu\n",
                                  ctx->current_word, i);
            tf_obj_release(map);
            tf_obj_release(pairs);
            return TF_ERR;
        }
        if (tf_map_has(map, key)) {
            tf_ctx_runtime_errorf(ctx, "'%s' duplicate map key at item %zu\n",
                                  ctx->current_word, i);
            tf_obj_release(map);
            tf_obj_release(pairs);
            return TF_ERR;
        }
        tf_map_set(map, key, value);
    }

    tf_stack_push(ctx, map);
    tf_obj_release(pairs);
    return TF_OK;
}

tf_ret tf_to_set(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 0)) return TF_ERR;
    tf_obj *seq = tf_stack_pop(ctx);
    tf_obj *set = tf_obj_new_set();

    if (seq->type == TF_OBJ_TYPE_LIST) {
        for (size_t i = 0; i < seq->list.len; i++) {
            tf_obj *item = seq->list.elem[i];
            if (!tf_obj_hashable(item)) {
                tf_ctx_runtime_errorf(
                    ctx, "'%s' expected hashable set item at index %zu\n",
                    ctx->current_word, i);
                tf_obj_release(set);
                tf_obj_release(seq);
                return TF_ERR;
            }
            tf_set_add(set, item);
        }
    } else {
        for (size_t i = 0; i < seq->str.len; i++) {
            tf_obj *item = tf_obj_new_string(seq->str.ptr + i, 1);
            tf_set_add(set, item);
            tf_obj_release(item);
        }
    }

    tf_stack_push(ctx, set);
    tf_obj_release(seq);
    return TF_OK;
}

tf_ret tf_to_deque(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 0)) return TF_ERR;
    tf_obj *seq = tf_stack_pop(ctx);
    tf_obj *deque = tf_obj_new_deque();

    if (seq->type == TF_OBJ_TYPE_LIST) {
        for (size_t i = 0; i < seq->list.len; i++) {
            tf_deque_push_back(deque, seq->list.elem[i]);
        }
    } else {
        for (size_t i = 0; i < seq->str.len; i++) {
            tf_obj *item = tf_obj_new_string(seq->str.ptr + i, 1);
            tf_deque_push_back(deque, item);
            tf_obj_release(item);
        }
    }

    tf_stack_push(ctx, deque);
    tf_obj_release(seq);
    return TF_OK;
}

tf_ret tf_to_pqueue(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_LIST)) return TF_ERR;
    tf_obj *pairs = tf_stack_pop_type(ctx, TF_OBJ_TYPE_LIST);
    tf_obj *pqueue = tf_obj_new_pqueue();

    for (size_t i = 0; i < pairs->list.len; i++) {
        tf_obj *pair = pairs->list.elem[i];
        if (pair->type != TF_OBJ_TYPE_LIST || pair->list.len != 2) {
            tf_ctx_runtime_errorf(
                ctx, "'%s' expected item %zu to be a two-item list\n",
                ctx->current_word, i);
            tf_obj_release(pqueue);
            tf_obj_release(pairs);
            return TF_ERR;
        }

        double priority = 0;
        if (!priority_from_obj(ctx, pair->list.elem[0], &priority)) {
            tf_obj_release(pqueue);
            tf_obj_release(pairs);
            return TF_ERR;
        }
        tf_pqueue_push(pqueue, priority, pair->list.elem[1]);
    }

    tf_stack_push(ctx, pqueue);
    tf_obj_release(pairs);
    return TF_OK;
}

tf_ret tf_has_q(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *coll = tf_stack_peek(ctx, 1);
    tf_obj *key = tf_stack_peek(ctx, 0);

    if (coll->type != TF_OBJ_TYPE_MAP && coll->type != TF_OBJ_TYPE_SET) {
        tf_ctx_runtime_errorf(ctx,
                              "'%s' expected map or set at stack depth 1, found %s\n",
                              ctx->current_word, tf_obj_type_name(coll));
        return TF_ERR;
    }
    if (!tf_obj_hashable(key)) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected hashable key/item, found %s\n",
                              ctx->current_word, tf_obj_type_name(key));
        return TF_ERR;
    }

    key = tf_stack_pop(ctx);
    coll = tf_stack_pop(ctx);
    bool result = coll->type == TF_OBJ_TYPE_MAP ? tf_map_has(coll, key)
                                                : tf_set_has(coll, key);
    tf_stack_push(ctx, tf_obj_new_bool(result));
    tf_obj_release(key);
    tf_obj_release(coll);
    return TF_OK;
}

tf_ret tf_get(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_MAP) ||
        !tf_ctx_require_stack(ctx, 2)) {
        return TF_ERR;
    }
    tf_obj *map = tf_stack_peek(ctx, 1);
    tf_obj *key = tf_stack_peek(ctx, 0);
    if (!tf_obj_hashable(key)) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected hashable key, found %s\n",
                              ctx->current_word, tf_obj_type_name(key));
        return TF_ERR;
    }

    tf_obj *value = NULL;
    if (!tf_map_get(map, key, &value)) {
        tf_ctx_runtime_errorf(ctx, "'%s' key not found\n", ctx->current_word);
        return TF_ERR;
    }

    key = tf_stack_pop(ctx);
    map = tf_stack_pop(ctx);
    tf_stack_push(ctx, value);
    tf_obj_retain(value);
    tf_obj_release(key);
    tf_obj_release(map);
    return TF_OK;
}

tf_ret tf_assoc(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 2, TF_OBJ_TYPE_MAP) ||
        !tf_ctx_require_stack(ctx, 3)) {
        return TF_ERR;
    }
    tf_obj *map = tf_stack_peek(ctx, 2);
    tf_obj *key = tf_stack_peek(ctx, 1);
    if (!tf_obj_hashable(key)) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected hashable key, found %s\n",
                              ctx->current_word, tf_obj_type_name(key));
        return TF_ERR;
    }

    tf_obj *value = tf_stack_pop(ctx);
    key = tf_stack_pop(ctx);
    map = tf_stack_pop(ctx);

    tf_obj *result = map_clone(map);
    tf_map_set(result, key, value);
    tf_stack_push(ctx, result);

    tf_obj_release(value);
    tf_obj_release(key);
    tf_obj_release(map);
    return TF_OK;
}

tf_ret tf_dissoc(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_MAP) ||
        !tf_ctx_require_stack(ctx, 2)) {
        return TF_ERR;
    }
    tf_obj *map = tf_stack_peek(ctx, 1);
    tf_obj *key = tf_stack_peek(ctx, 0);
    if (!tf_obj_hashable(key)) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected hashable key, found %s\n",
                              ctx->current_word, tf_obj_type_name(key));
        return TF_ERR;
    }

    key = tf_stack_pop(ctx);
    map = tf_stack_pop(ctx);
    tf_stack_push(ctx, map_clone_without(map, key));
    tf_obj_release(key);
    tf_obj_release(map);
    return TF_OK;
}

tf_ret tf_keys(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_MAP)) return TF_ERR;
    tf_obj *map = tf_stack_pop_type(ctx, TF_OBJ_TYPE_MAP);
    tf_obj *keys = tf_obj_new_list();
    for (size_t i = 0; i < map->map.len; i++) {
        tf_obj *key = map->map.entries[i].key;
        tf_obj_retain(key);
        tf_list_push(keys, key);
    }
    tf_stack_push(ctx, keys);
    tf_obj_release(map);
    return TF_OK;
}

tf_ret tf_values(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_MAP)) return TF_ERR;
    tf_obj *map = tf_stack_pop_type(ctx, TF_OBJ_TYPE_MAP);
    tf_obj *values = tf_obj_new_list();
    for (size_t i = 0; i < map->map.len; i++) {
        tf_obj *value = map->map.entries[i].value;
        tf_obj_retain(value);
        tf_list_push(values, value);
    }
    tf_stack_push(ctx, values);
    tf_obj_release(map);
    return TF_OK;
}

tf_ret tf_pairs(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_MAP)) return TF_ERR;
    tf_obj *map = tf_stack_pop_type(ctx, TF_OBJ_TYPE_MAP);
    tf_obj *pairs = tf_obj_new_list();
    for (size_t i = 0; i < map->map.len; i++) {
        tf_obj *pair = tf_obj_new_list();
        tf_obj *key = map->map.entries[i].key;
        tf_obj *value = map->map.entries[i].value;
        tf_obj_retain(key);
        tf_obj_retain(value);
        tf_list_push(pair, key);
        tf_list_push(pair, value);
        tf_list_push(pairs, pair);
    }
    tf_stack_push(ctx, pairs);
    tf_obj_release(map);
    return TF_OK;
}

tf_ret tf_items(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *coll = tf_stack_peek(ctx, 0);
    if (coll->type != TF_OBJ_TYPE_SET && coll->type != TF_OBJ_TYPE_DEQUE) {
        tf_ctx_runtime_errorf(ctx,
                              "'%s' expected set or deque, found %s\n",
                              ctx->current_word, tf_obj_type_name(coll));
        return TF_ERR;
    }

    coll = tf_stack_pop(ctx);
    tf_obj *items = tf_obj_new_list();
    if (coll->type == TF_OBJ_TYPE_SET) {
        for (size_t i = 0; i < coll->set.len; i++) {
            tf_obj *item = coll->set.entries[i].item;
            tf_obj_retain(item);
            tf_list_push(items, item);
        }
    } else {
        for (size_t i = 0; i < coll->deque.len; i++) {
            tf_obj *item = tf_deque_get(coll, i);
            tf_obj_retain(item);
            tf_list_push(items, item);
        }
    }
    tf_stack_push(ctx, items);
    tf_obj_release(coll);
    return TF_OK;
}

tf_ret tf_adjoin(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_SET) ||
        !tf_ctx_require_stack(ctx, 2)) {
        return TF_ERR;
    }
    tf_obj *set = tf_stack_peek(ctx, 1);
    tf_obj *item = tf_stack_peek(ctx, 0);
    if (!tf_obj_hashable(item)) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected hashable set item, found %s\n",
                              ctx->current_word, tf_obj_type_name(item));
        return TF_ERR;
    }

    item = tf_stack_pop(ctx);
    set = tf_stack_pop(ctx);
    tf_obj *result = set_clone(set);
    tf_set_add(result, item);
    tf_stack_push(ctx, result);
    tf_obj_release(item);
    tf_obj_release(set);
    return TF_OK;
}

tf_ret tf_remove(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_SET) ||
        !tf_ctx_require_stack(ctx, 2)) {
        return TF_ERR;
    }
    tf_obj *set = tf_stack_peek(ctx, 1);
    tf_obj *item = tf_stack_peek(ctx, 0);
    if (!tf_obj_hashable(item)) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected hashable set item, found %s\n",
                              ctx->current_word, tf_obj_type_name(item));
        return TF_ERR;
    }

    item = tf_stack_pop(ctx);
    set = tf_stack_pop(ctx);
    tf_stack_push(ctx, set_clone_without(set, item));
    tf_obj_release(item);
    tf_obj_release(set);
    return TF_OK;
}

tf_ret tf_push_front(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_DEQUE) ||
        !tf_ctx_require_stack(ctx, 2)) {
        return TF_ERR;
    }

    tf_obj *item = tf_stack_pop(ctx);
    tf_obj *deque = tf_stack_pop_type(ctx, TF_OBJ_TYPE_DEQUE);
    tf_obj *result = tf_deque_clone(deque);
    tf_deque_push_front(result, item);
    tf_stack_push(ctx, result);
    tf_obj_release(item);
    tf_obj_release(deque);
    return TF_OK;
}

tf_ret tf_push_back(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_DEQUE) ||
        !tf_ctx_require_stack(ctx, 2)) {
        return TF_ERR;
    }

    tf_obj *item = tf_stack_pop(ctx);
    tf_obj *deque = tf_stack_pop_type(ctx, TF_OBJ_TYPE_DEQUE);
    tf_obj *result = tf_deque_clone(deque);
    tf_deque_push_back(result, item);
    tf_stack_push(ctx, result);
    tf_obj_release(item);
    tf_obj_release(deque);
    return TF_OK;
}

tf_ret tf_pop_front(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_DEQUE)) return TF_ERR;
    tf_obj *deque = tf_stack_peek(ctx, 0);
    if (deque->deque.len == 0) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected non-empty deque\n",
                              ctx->current_word);
        return TF_ERR;
    }

    deque = tf_stack_pop_type(ctx, TF_OBJ_TYPE_DEQUE);
    tf_obj *result = tf_deque_clone(deque);
    tf_obj *item = tf_deque_pop_front(result);
    tf_stack_push(ctx, item);
    tf_stack_push(ctx, result);
    tf_obj_release(deque);
    return TF_OK;
}

tf_ret tf_pop_back(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_DEQUE)) return TF_ERR;
    tf_obj *deque = tf_stack_peek(ctx, 0);
    if (deque->deque.len == 0) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected non-empty deque\n",
                              ctx->current_word);
        return TF_ERR;
    }

    deque = tf_stack_pop_type(ctx, TF_OBJ_TYPE_DEQUE);
    tf_obj *result = tf_deque_clone(deque);
    tf_obj *item = tf_deque_pop_back(result);
    tf_stack_push(ctx, item);
    tf_stack_push(ctx, result);
    tf_obj_release(deque);
    return TF_OK;
}

tf_ret tf_front(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_DEQUE)) return TF_ERR;
    tf_obj *deque = tf_stack_peek(ctx, 0);
    if (deque->deque.len == 0) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected non-empty deque\n",
                              ctx->current_word);
        return TF_ERR;
    }

    tf_obj *item = tf_deque_get(deque, 0);
    tf_obj_retain(item);
    deque = tf_stack_pop_type(ctx, TF_OBJ_TYPE_DEQUE);
    tf_stack_push(ctx, item);
    tf_obj_release(deque);
    return TF_OK;
}

tf_ret tf_back(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_DEQUE)) return TF_ERR;
    tf_obj *deque = tf_stack_peek(ctx, 0);
    if (deque->deque.len == 0) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected non-empty deque\n",
                              ctx->current_word);
        return TF_ERR;
    }

    tf_obj *item = tf_deque_get(deque, deque->deque.len - 1);
    tf_obj_retain(item);
    deque = tf_stack_pop_type(ctx, TF_OBJ_TYPE_DEQUE);
    tf_stack_push(ctx, item);
    tf_obj_release(deque);
    return TF_OK;
}

tf_ret tf_pqueue_push_word(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 2, TF_OBJ_TYPE_PQUEUE) ||
        !tf_ctx_require_number(ctx, 1) ||
        !tf_ctx_require_stack(ctx, 3)) {
        return TF_ERR;
    }

    tf_obj *priority_obj = tf_stack_peek(ctx, 1);
    double priority = 0;
    if (!priority_from_obj(ctx, priority_obj, &priority)) return TF_ERR;

    tf_obj *value = tf_stack_pop(ctx);
    priority_obj = tf_stack_pop(ctx);
    tf_obj *pqueue = tf_stack_pop_type(ctx, TF_OBJ_TYPE_PQUEUE);
    tf_obj *result = tf_pqueue_clone(pqueue);
    tf_pqueue_push(result, priority, value);
    tf_stack_push(ctx, result);
    tf_obj_release(value);
    tf_obj_release(priority_obj);
    tf_obj_release(pqueue);
    return TF_OK;
}

tf_ret tf_pqueue_peek_word(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_PQUEUE)) return TF_ERR;
    tf_obj *pqueue = tf_stack_peek(ctx, 0);
    tf_obj *value = NULL;
    if (!tf_pqueue_peek(pqueue, NULL, &value)) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected non-empty pqueue\n",
                              ctx->current_word);
        return TF_ERR;
    }

    tf_obj_retain(value);
    pqueue = tf_stack_pop_type(ctx, TF_OBJ_TYPE_PQUEUE);
    tf_stack_push(ctx, value);
    tf_obj_release(pqueue);
    return TF_OK;
}

tf_ret tf_pqueue_pop_word(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_PQUEUE)) return TF_ERR;
    tf_obj *pqueue = tf_stack_peek(ctx, 0);
    if (pqueue->pqueue.len == 0) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected non-empty pqueue\n",
                              ctx->current_word);
        return TF_ERR;
    }

    pqueue = tf_stack_pop_type(ctx, TF_OBJ_TYPE_PQUEUE);
    tf_obj *result = tf_pqueue_clone(pqueue);
    tf_obj *value = NULL;
    tf_pqueue_pop(result, NULL, &value);
    tf_stack_push(ctx, value);
    tf_stack_push(ctx, result);
    tf_obj_release(pqueue);
    return TF_OK;
}

tf_ret tf_pqueue_drain(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_PQUEUE)) return TF_ERR;
    tf_obj *pqueue = tf_stack_pop_type(ctx, TF_OBJ_TYPE_PQUEUE);
    tf_obj *tmp = tf_pqueue_clone(pqueue);
    tf_obj *items = tf_obj_new_list();

    while (tmp->pqueue.len > 0) {
        tf_obj *value = NULL;
        tf_pqueue_pop(tmp, NULL, &value);
        tf_list_push(items, value);
    }

    tf_stack_push(ctx, items);
    tf_obj_release(tmp);
    tf_obj_release(pqueue);
    return TF_OK;
}

tf_ret tf_join(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_LIST) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }
    tf_obj *sep = tf_stack_peek(ctx, 0);
    tf_obj *list = tf_stack_peek(ctx, 1);

    size_t total_len = 0;
    for (size_t i = 0; i < list->list.len; i++) {
        tf_obj *elem = list->list.elem[i];
        if (elem->type != TF_OBJ_TYPE_STR) {
            tf_ctx_runtime_errorf(
                ctx, "'%s' expected list item %zu to be string, found %s\n",
                ctx->current_word, i, tf_obj_type_name(elem));
            return TF_ERR;
        }
        total_len += elem->str.len;
    }
    if (list->list.len > 1) { total_len += sep->str.len * (list->list.len - 1); }

    sep = tf_stack_pop(ctx);
    list = tf_stack_pop(ctx);

    char *result = tf_xmalloc(total_len + 1);
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

    tf_stack_push(ctx, tf_obj_new_string(result, total_len));
    free(result);
    tf_obj_release(list);
    tf_obj_release(sep);
    return TF_OK;
}

tf_ret tf_trim(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *str = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

    if (str->str.len == 0) {
        tf_stack_push(ctx, tf_obj_new_string("", 0));
        tf_obj_release(str);
        return TF_OK;
    }

    char *start = str->str.ptr;
    char *end = str->str.ptr + str->str.len - 1;

    while (start <= end && isspace((unsigned char)*start)) start++;
    while (end > start && isspace((unsigned char)*end)) end--;

    size_t new_len = (start <= end) ? (size_t)(end - start + 1) : 0;
    tf_stack_push(ctx, tf_obj_new_string(start, new_len));
    tf_obj_release(str);
    return TF_OK;
}

tf_ret tf_upper(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *str = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

    char *new_str = tf_xmalloc(str->str.len + 1);
    for (size_t i = 0; i < str->str.len; i++) {
        new_str[i] = (char)toupper((unsigned char)str->str.ptr[i]);
    }
    new_str[str->str.len] = '\0';

    tf_stack_push(ctx, tf_obj_new_string(new_str, str->str.len));
    free(new_str);
    tf_obj_release(str);
    return TF_OK;
}

tf_ret tf_lower(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *str = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

    char *new_str = tf_xmalloc(str->str.len + 1);
    for (size_t i = 0; i < str->str.len; i++) {
        new_str[i] = (char)tolower((unsigned char)str->str.ptr[i]);
    }
    new_str[str->str.len] = '\0';

    tf_stack_push(ctx, tf_obj_new_string(new_str, str->str.len));
    free(new_str);
    tf_obj_release(str);
    return TF_OK;
}

tf_ret tf_splitmid(tf_ctx *ctx) {
    if (!tf_ctx_require_sequence(ctx, 0)) return TF_ERR;

    tf_obj *seq = tf_stack_pop(ctx);

    size_t len = seq->type == TF_OBJ_TYPE_LIST ? seq->list.len : seq->str.len;
    size_t mid = len / 2;

    if (seq->type == TF_OBJ_TYPE_LIST) {
        tf_obj *left = tf_obj_new_list();
        tf_obj *right = tf_obj_new_list();

        for (size_t i = 0; i < mid; i++) {
            tf_obj *elem = seq->list.elem[i];
            tf_obj_retain(elem);
            tf_list_push(left, elem);
        }
        for (size_t i = mid; i < len; i++) {
            tf_obj *elem = seq->list.elem[i];
            tf_obj_retain(elem);
            tf_list_push(right, elem);
        }

        tf_stack_push(ctx, left);
        tf_stack_push(ctx, right);
    } else {
        tf_stack_push(ctx, tf_obj_new_string(seq->str.ptr, mid));
        tf_stack_push(ctx, tf_obj_new_string(seq->str.ptr + mid, len - mid));
    }

    tf_obj_release(seq);
    return TF_OK;
}

tf_ret tf_range(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_INT) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) {
        return TF_ERR;
    }
    tf_obj *end_obj = tf_stack_peek(ctx, 0);
    tf_obj *start_obj = tf_stack_peek(ctx, 1);

    int start = start_obj->i;
    int end = end_obj->i;
    if (end < start) {
        tf_ctx_runtime_errorf(ctx, "'%s' end must be greater than or equal to start\n",
                              ctx->current_word);
        return TF_ERR;
    }

    end_obj = tf_stack_pop(ctx);
    start_obj = tf_stack_pop(ctx);

    tf_obj *result = tf_obj_new_list();
    for (int i = start; i < end; i++) { tf_list_push(result, tf_obj_new_int(i)); }

    tf_stack_push(ctx, result);
    tf_obj_release(end_obj);
    tf_obj_release(start_obj);
    return TF_OK;
}

tf_ret tf_empty_q(tf_ctx *ctx) {
    if (!require_countable(ctx, 0)) return TF_ERR;
    tf_obj *coll = tf_stack_pop(ctx);
    bool is_empty = false;
    if (coll->type == TF_OBJ_TYPE_LIST) {
        is_empty = coll->list.len == 0;
    } else if (coll->type == TF_OBJ_TYPE_STR) {
        is_empty = coll->str.len == 0;
    } else if (coll->type == TF_OBJ_TYPE_MAP) {
        is_empty = coll->map.len == 0;
    } else if (coll->type == TF_OBJ_TYPE_SET) {
        is_empty = coll->set.len == 0;
    } else if (coll->type == TF_OBJ_TYPE_DEQUE) {
        is_empty = coll->deque.len == 0;
    } else if (coll->type == TF_OBJ_TYPE_PQUEUE) {
        is_empty = coll->pqueue.len == 0;
    }
    tf_stack_push(ctx, tf_obj_new_bool(is_empty));
    tf_obj_release(coll);
    return TF_OK;
}

tf_ret tf_char_q(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    tf_stack_push(ctx, tf_obj_new_bool(is_char_obj(o)));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_letter_q(tf_ctx *ctx) {
    return char_predicate(ctx, TF_CHAR_PRED_LETTER);
}

tf_ret tf_digit_q(tf_ctx *ctx) {
    return char_predicate(ctx, TF_CHAR_PRED_DIGIT);
}

tf_ret tf_alnum_q(tf_ctx *ctx) {
    return char_predicate(ctx, TF_CHAR_PRED_ALNUM);
}

tf_ret tf_space_q(tf_ctx *ctx) {
    return char_predicate(ctx, TF_CHAR_PRED_SPACE);
}

tf_ret tf_upper_q(tf_ctx *ctx) {
    return char_predicate(ctx, TF_CHAR_PRED_UPPER);
}

tf_ret tf_lower_q(tf_ctx *ctx) {
    return char_predicate(ctx, TF_CHAR_PRED_LOWER);
}

tf_ret tf_punct_q(tf_ctx *ctx) {
    return char_predicate(ctx, TF_CHAR_PRED_PUNCT);
}
