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
        saved_stack[i] = stack_peek(ctx, saved_len - 1 - i);
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
    if (list->list.len > 1) {
        total_len += sep->str.len * (list->list.len - 1);
    }

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

tf_ret tf_merge_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 3) return TF_ERR;
    tf_obj *pred = stack_peek(ctx, 0);
    tf_obj *l2 = stack_peek(ctx, 1);
    tf_obj *l1 = stack_peek(ctx, 2);
    if (pred->type != TF_OBJ_TYPE_LIST || l2->type != TF_OBJ_TYPE_LIST ||
        l1->type != TF_OBJ_TYPE_LIST) {
        return TF_ERR;
    }
    pred = stack_pop(ctx);
    l2 = stack_pop(ctx);
    l1 = stack_pop(ctx);

    tf_obj *res = init_list_obj();
    size_t i1 = 0, i2 = 0;

    while (i1 < l1->list.len && i2 < l2->list.len) {
        tf_obj *o1 = l1->list.elem[i1];
        tf_obj *o2 = l2->list.elem[i2];

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
