#include "tf_lib.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "tf_alloc.h"
#include "tf_exec.h"
#include "tf_obj.h"

static bool is_char_obj(tf_obj *o) {
    return o->type == TF_OBJ_TYPE_STR && o->str.len == 1;
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
    if (tf_stack_len(ctx) < 1) return TF_ERR;
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

tf_ret tf_geth(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 2) return TF_ERR;
    tf_obj *idx_obj = tf_stack_peek(ctx, 0);
    tf_obj *coll_obj = tf_stack_peek(ctx, 1);
    if (idx_obj->type != TF_OBJ_TYPE_INT) return TF_ERR;

    int idx = idx_obj->i;
    if (coll_obj->type == TF_OBJ_TYPE_LIST) {
        if (idx < 0 || idx >= (int)coll_obj->list.len) return TF_ERR;
        idx_obj = tf_stack_pop(ctx);
        coll_obj = tf_stack_pop(ctx);
        tf_obj *result = coll_obj->list.elem[idx];
        tf_obj_retain(result);
        tf_stack_push(ctx, result);
        tf_obj_release(idx_obj);
        tf_obj_release(coll_obj);
        return TF_OK;
    } else if (coll_obj->type == TF_OBJ_TYPE_STR) {
        if (idx < 0 || idx >= (int)coll_obj->str.len) return TF_ERR;
        idx_obj = tf_stack_pop(ctx);
        coll_obj = tf_stack_pop(ctx);
        char buf[2] = { coll_obj->str.ptr[idx], 0 };
        tf_stack_push(ctx, tf_obj_new_string(buf, 1));
        tf_obj_release(idx_obj);
        tf_obj_release(coll_obj);
        return TF_OK;
    }
    return TF_ERR;
}

tf_ret tf_seth(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 3) return TF_ERR;
    tf_obj *val = tf_stack_peek(ctx, 0);
    tf_obj *idx_obj = tf_stack_peek(ctx, 1);
    tf_obj *coll_obj = tf_stack_peek(ctx, 2);

    if (idx_obj->type != TF_OBJ_TYPE_INT) return TF_ERR;
    int idx = idx_obj->i;

    if (coll_obj->type == TF_OBJ_TYPE_LIST) {
        if (idx < 0 || idx >= (int)coll_obj->list.len) return TF_ERR;
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
    } else if (coll_obj->type == TF_OBJ_TYPE_STR) {
        if (idx < 0 || idx >= (int)coll_obj->str.len) return TF_ERR;
        if (!is_char_obj(val)) return TF_ERR;
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
    return TF_ERR;
}

tf_ret tf_slice(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 3) return TF_ERR;
    tf_obj *end_obj = tf_stack_peek(ctx, 0);
    tf_obj *start_obj = tf_stack_peek(ctx, 1);
    tf_obj *coll = tf_stack_peek(ctx, 2);
    if (end_obj->type != TF_OBJ_TYPE_INT ||
        start_obj->type != TF_OBJ_TYPE_INT ||
        (coll->type != TF_OBJ_TYPE_LIST && coll->type != TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }

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
    if (tf_stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    int len = 0;
    if (o->type == TF_OBJ_TYPE_LIST) {
        len = (int)o->list.len;
    } else if (o->type == TF_OBJ_TYPE_STR) {
        len = (int)o->str.len;
    } else {
        tf_stack_push(ctx, o);
        return TF_ERR;
    }
    tf_stack_push(ctx, tf_obj_new_int(len));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_first(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 1) return TF_ERR;
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
        return TF_ERR;
    }
}

tf_ret tf_rest(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 1) return TF_ERR;
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
    } else if (seq->type == TF_OBJ_TYPE_STR) {
        seq = tf_stack_pop(ctx);
        size_t start = seq->str.len > 0 ? 1 : 0;
        tf_stack_push(ctx, tf_obj_new_string(seq->str.ptr + start,
                                          seq->str.len - start));
        tf_obj_release(seq);
        return TF_OK;
    }
    return TF_ERR;
}

tf_ret tf_uncons(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 1) return TF_ERR;
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
        return TF_ERR;
    }
}

tf_ret tf_take(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 2) return TF_ERR;
    tf_obj *n_obj = tf_stack_peek(ctx, 0);
    tf_obj *coll = tf_stack_peek(ctx, 1);
    if (n_obj->type != TF_OBJ_TYPE_INT ||
        (coll->type != TF_OBJ_TYPE_LIST && coll->type != TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }

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
    if (tf_stack_len(ctx) < 2) return TF_ERR;
    tf_obj *n_obj = tf_stack_peek(ctx, 0);
    tf_obj *coll = tf_stack_peek(ctx, 1);
    if (n_obj->type != TF_OBJ_TYPE_INT ||
        (coll->type != TF_OBJ_TYPE_LIST && coll->type != TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }

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
    if (tf_stack_len(ctx) < 2) return TF_ERR;
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
        tf_obj *head = tf_stack_peek(ctx, 1);
        if (!is_char_obj(head)) return TF_ERR;

        seq = tf_stack_pop(ctx);
        head = tf_stack_pop(ctx);
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

    return TF_ERR;
}

tf_ret tf_append(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 2) return TF_ERR;
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
        if (!is_char_obj(elem)) return TF_ERR;

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

    return TF_ERR;
}

tf_ret tf_concat(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 2) return TF_ERR;
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
    if (tf_stack_len(ctx) < 1) return TF_ERR;
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

    if (seq->type == TF_OBJ_TYPE_STR) {
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

    tf_stack_push(ctx, seq);
    return TF_ERR;
}

tf_ret tf_split_string(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 2) return TF_ERR;
    tf_obj *arg2 = tf_stack_peek(ctx, 0);
    tf_obj *arg1 = tf_stack_peek(ctx, 1);
    if (arg1->type != TF_OBJ_TYPE_STR || arg2->type != TF_OBJ_TYPE_STR) {
        return TF_ERR;
    }
    arg2 = tf_stack_pop(ctx);
    arg1 = tf_stack_pop(ctx);

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

tf_ret tf_join(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 2) return TF_ERR;
    tf_obj *sep = tf_stack_peek(ctx, 0);
    tf_obj *list = tf_stack_peek(ctx, 1);
    if (sep->type != TF_OBJ_TYPE_STR || list->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }

    size_t total_len = 0;
    for (size_t i = 0; i < list->list.len; i++) {
        tf_obj *elem = list->list.elem[i];
        if (elem->type != TF_OBJ_TYPE_STR) return TF_ERR;
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
    if (tf_stack_len(ctx) < 1) return TF_ERR;
    tf_obj *str = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!str) return TF_ERR;

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
    if (tf_stack_len(ctx) < 1) return TF_ERR;
    tf_obj *str = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!str) return TF_ERR;

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
    if (tf_stack_len(ctx) < 1) return TF_ERR;
    tf_obj *str = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!str) return TF_ERR;

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
    if (tf_stack_len(ctx) < 1) return TF_ERR;

    tf_obj *seq = tf_stack_peek(ctx, 0);
    if (seq->type != TF_OBJ_TYPE_LIST && seq->type != TF_OBJ_TYPE_STR) {
        return TF_ERR;
    }
    seq = tf_stack_pop(ctx);

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
    if (tf_stack_len(ctx) < 2) return TF_ERR;
    tf_obj *end_obj = tf_stack_peek(ctx, 0);
    tf_obj *start_obj = tf_stack_peek(ctx, 1);
    if (start_obj->type != TF_OBJ_TYPE_INT || end_obj->type != TF_OBJ_TYPE_INT) {
        return TF_ERR;
    }

    int start = start_obj->i;
    int end = end_obj->i;
    if (end < start) return TF_ERR;

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
    if (tf_stack_len(ctx) < 1) return TF_ERR;
    tf_obj *coll = tf_stack_pop(ctx);
    bool is_empty = false;
    if (coll->type == TF_OBJ_TYPE_LIST) {
        is_empty = coll->list.len == 0;
    } else if (coll->type == TF_OBJ_TYPE_STR) {
        is_empty = coll->str.len == 0;
    } else {
        tf_stack_push(ctx, coll);
        return TF_ERR;
    }
    tf_stack_push(ctx, tf_obj_new_bool(is_empty));
    tf_obj_release(coll);
    return TF_OK;
}

tf_ret tf_char_q(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 1) return TF_ERR;
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
