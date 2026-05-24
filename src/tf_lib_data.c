#include "tf_lib.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "tf_alloc.h"
#include "tf_exec.h"
#include "tf_obj.h"

tf_ret tf_geth(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *idx_obj = stack_peek(ctx, 0);
    tf_obj *coll_obj = stack_peek(ctx, 1);
    if (idx_obj->type != TF_OBJ_TYPE_INT) return TF_ERR;

    int idx = idx_obj->i;
    if (coll_obj->type == TF_OBJ_TYPE_LIST) {
        if (idx < 0 || idx >= (int)coll_obj->list.len) return TF_ERR;
        idx_obj = stack_pop(ctx);
        tf_obj *result = coll_obj->list.elem[idx];
        retain_obj(result);
        stack_push(ctx, result);
        release_obj(idx_obj);
        return TF_OK;
    } else if (coll_obj->type == TF_OBJ_TYPE_STR) {
        if (idx < 0 || idx >= (int)coll_obj->str.len) return TF_ERR;
        idx_obj = stack_pop(ctx);
        char buf[2] = { coll_obj->str.ptr[idx], 0 };
        stack_push(ctx, create_string_obj(buf, 1));
        release_obj(idx_obj);
        return TF_OK;
    }
    return TF_ERR;
}

tf_ret tf_seth(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *val = stack_peek(ctx, 0);
    tf_obj *idx_obj = stack_peek(ctx, 1);
    tf_obj *coll_obj = stack_peek(ctx, 2);

    if (idx_obj->type != TF_OBJ_TYPE_INT) return TF_ERR;
    int idx = idx_obj->i;

    if (coll_obj->type == TF_OBJ_TYPE_LIST) {
        if (idx < 0 || idx >= (int)coll_obj->list.len) return TF_ERR;
        val = stack_pop(ctx);
        idx_obj = stack_pop(ctx);
        coll_obj = stack_pop(ctx);
        release_obj(coll_obj->list.elem[idx]);
        coll_obj->list.elem[idx] = val;
        release_obj(idx_obj);
        release_obj(coll_obj);
        return TF_OK;
    } else if (coll_obj->type == TF_OBJ_TYPE_STR) {
        if (idx < 0 || idx >= (int)coll_obj->str.len) return TF_ERR;
        if (val->type != TF_OBJ_TYPE_STR || val->str.len < 1) return TF_ERR;
        val = stack_pop(ctx);
        idx_obj = stack_pop(ctx);
        coll_obj = stack_pop(ctx);

        tf_obj *new_str = create_string_obj(coll_obj->str.ptr, coll_obj->str.len);
        new_str->str.ptr[idx] = val->str.ptr[0];
        stack_push(ctx, new_str);

        release_obj(val);
        release_obj(idx_obj);
        release_obj(coll_obj);
        return TF_OK;
    }
    return TF_ERR;
}

tf_ret tf_slice(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *end_obj = stack_peek(ctx, 0);
    tf_obj *start_obj = stack_peek(ctx, 1);
    tf_obj *coll = stack_peek(ctx, 2);
    if (end_obj->type != TF_OBJ_TYPE_INT ||
        start_obj->type != TF_OBJ_TYPE_INT ||
        (coll->type != TF_OBJ_TYPE_LIST && coll->type != TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }

    end_obj = stack_pop(ctx);
    start_obj = stack_pop(ctx);
    coll = stack_pop(ctx);

    int start = start_obj->i;
    int end = end_obj->i;
    int len = (coll->type == TF_OBJ_TYPE_LIST) ? (int)coll->list.len : (int)coll->str.len;

    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start > end) start = end;

    if (coll->type == TF_OBJ_TYPE_LIST) {
        tf_obj *res = init_list_obj();
        for (int i = start; i < end; i++) {
            retain_obj(coll->list.elem[i]);
            push_obj(res, coll->list.elem[i]);
        }
        stack_push(ctx, res);
    } else {
        stack_push(ctx, create_string_obj(coll->str.ptr + start, end - start));
    }

    release_obj(start_obj);
    release_obj(end_obj);
    release_obj(coll);
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
    if (list_obj->type != TF_OBJ_TYPE_LIST) { return TF_ERR; }

    tf_obj *rest = init_list_obj();
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

    tf_obj *rest = init_list_obj();
    for (size_t i = 1; i < list_obj->list.len; i++) {
        tf_obj *elem = list_obj->list.elem[i];
        retain_obj(elem);
        push_obj(rest, elem);
    }

    stack_push(ctx, head);
    stack_push(ctx, rest);
    release_obj(list_obj);
    return TF_OK;
}

tf_ret tf_take(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *n_obj = stack_peek(ctx, 0);
    tf_obj *coll = stack_peek(ctx, 1);
    if (n_obj->type != TF_OBJ_TYPE_INT ||
        (coll->type != TF_OBJ_TYPE_LIST && coll->type != TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }

    n_obj = stack_pop(ctx);
    coll = stack_pop(ctx);
    int n = n_obj->i;
    release_obj(n_obj);
    if (n < 0) n = 0;

    if (coll->type == TF_OBJ_TYPE_LIST) {
        if (n > (int)coll->list.len) n = (int)coll->list.len;
        tf_obj *res = init_list_obj();
        for (int i = 0; i < n; i++) {
            retain_obj(coll->list.elem[i]);
            push_obj(res, coll->list.elem[i]);
        }
        stack_push(ctx, res);
    } else {
        if (n > (int)coll->str.len) n = (int)coll->str.len;
        stack_push(ctx, create_string_obj(coll->str.ptr, n));
    }
    release_obj(coll);
    return TF_OK;
}

tf_ret tf_dropn(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *n_obj = stack_peek(ctx, 0);
    tf_obj *coll = stack_peek(ctx, 1);
    if (n_obj->type != TF_OBJ_TYPE_INT ||
        (coll->type != TF_OBJ_TYPE_LIST && coll->type != TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }

    n_obj = stack_pop(ctx);
    coll = stack_pop(ctx);
    int n = n_obj->i;
    release_obj(n_obj);
    if (n < 0) n = 0;

    if (coll->type == TF_OBJ_TYPE_LIST) {
        if (n > (int)coll->list.len) n = (int)coll->list.len;
        tf_obj *res = init_list_obj();
        for (size_t i = n; i < coll->list.len; i++) {
            retain_obj(coll->list.elem[i]);
            push_obj(res, coll->list.elem[i]);
        }
        stack_push(ctx, res);
    } else {
        if (n > (int)coll->str.len) n = (int)coll->str.len;
        stack_push(ctx, create_string_obj(coll->str.ptr + n, coll->str.len - n));
    }
    release_obj(coll);
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

tf_ret tf_split_string(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *arg2 = stack_peek(ctx, 0);
    tf_obj *arg1 = stack_peek(ctx, 1);
    if (arg1->type != TF_OBJ_TYPE_STR || arg2->type != TF_OBJ_TYPE_STR) {
        return TF_ERR;
    }
    arg2 = stack_pop(ctx);
    arg1 = stack_pop(ctx);

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

tf_ret tf_join(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *sep = stack_peek(ctx, 0);
    tf_obj *list = stack_peek(ctx, 1);
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

    sep = stack_pop(ctx);
    list = stack_pop(ctx);

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

    if (str->str.len == 0) {
        stack_push(ctx, create_string_obj("", 0));
        release_obj(str);
        return TF_OK;
    }

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
    for (int i = start; i < end; i++) { push_obj(result, create_int_obj(i)); }

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
