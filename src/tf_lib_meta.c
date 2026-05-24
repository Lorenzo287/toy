#include "tf_lib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tf_alloc.h"
#include "tf_exec.h"
#include "tf_obj.h"

tf_ret tf_number_q(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    stack_push(ctx, create_bool_obj(o->type == TF_OBJ_TYPE_INT ||
                                    o->type == TF_OBJ_TYPE_FLOAT));
    release_obj(o);
    return TF_OK;
}

tf_ret tf_nan_q(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    bool result = o->type == TF_OBJ_TYPE_FLOAT && isnan(o->f);
    stack_push(ctx, create_bool_obj(result));
    release_obj(o);
    return TF_OK;
}

tf_ret tf_inf_q(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    bool result = o->type == TF_OBJ_TYPE_FLOAT && isinf(o->f);
    stack_push(ctx, create_bool_obj(result));
    release_obj(o);
    return TF_OK;
}

tf_ret tf_word_q(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    bool result = (o->type == TF_OBJ_TYPE_SYMBOL || o->type == TF_OBJ_TYPE_STR) &&
                  get_func(ctx, o) != NULL;
    stack_push(ctx, create_bool_obj(result));
    release_obj(o);
    return TF_OK;
}

tf_ret tf_var_q(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    bool result = (o->type == TF_OBJ_TYPE_SYMBOL || o->type == TF_OBJ_TYPE_STR) &&
                  tf_var_fetch(ctx, o) != NULL;
    stack_push(ctx, create_bool_obj(result));
    release_obj(o);
    return TF_OK;
}

tf_ret tf_inf(tf_ctx *ctx) {
    stack_push(ctx, create_float_obj(INFINITY));
    return TF_OK;
}

tf_ret tf_nan(tf_ctx *ctx) {
    stack_push(ctx, create_float_obj(NAN));
    return TF_OK;
}

tf_ret tf_body(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *name = stack_pop_type(ctx, TF_OBJ_TYPE_SYMBOL);
    if (!name) return TF_ERR;

    tf_func *f = get_func(ctx, name);
    if (!f || f->type != TF_FUNC_TYPE_USER) {
        release_obj(name);
        return TF_ERR;
    }

    stack_push(ctx, f->user_impl);
    retain_obj(f->user_impl);
    release_obj(name);
    return TF_OK;
}

tf_ret tf_intern(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *str = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!str) return TF_ERR;

    stack_push(ctx, create_symbol_obj(str->str.ptr, str->str.len));
    release_obj(str);
    return TF_OK;
}

tf_ret tf_name(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *sym = stack_pop_type(ctx, TF_OBJ_TYPE_SYMBOL);
    if (!sym) return TF_ERR;

    stack_push(ctx, create_string_obj(sym->str.ptr, sym->str.len));
    release_obj(sym);
    return TF_OK;
}

static tf_ret tf_type_check(tf_ctx *ctx, tf_type type) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    stack_push(ctx, create_bool_obj(o->type == type));
    release_obj(o);
    return TF_OK;
}

tf_ret tf_bool_q(tf_ctx *ctx) {
    return tf_type_check(ctx, TF_OBJ_TYPE_BOOL);
}
tf_ret tf_int_q(tf_ctx *ctx) {
    return tf_type_check(ctx, TF_OBJ_TYPE_INT);
}
tf_ret tf_float_q(tf_ctx *ctx) {
    return tf_type_check(ctx, TF_OBJ_TYPE_FLOAT);
}
tf_ret tf_str_q(tf_ctx *ctx) {
    return tf_type_check(ctx, TF_OBJ_TYPE_STR);
}
tf_ret tf_symbol_q(tf_ctx *ctx) {
    return tf_type_check(ctx, TF_OBJ_TYPE_SYMBOL);
}
tf_ret tf_list_q(tf_ctx *ctx) {
    return tf_type_check(ctx, TF_OBJ_TYPE_LIST);
}

tf_ret tf_typeof(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    const char *type_str = "unknown";
    switch (o->type) {
    case TF_OBJ_TYPE_BOOL:
        type_str = "bool";
        break;
    case TF_OBJ_TYPE_INT:
        type_str = "int";
        break;
    case TF_OBJ_TYPE_FLOAT:
        type_str = "float";
        break;
    case TF_OBJ_TYPE_STR:
        type_str = "str";
        break;
    case TF_OBJ_TYPE_SYMBOL:
        type_str = "symbol";
        break;
    case TF_OBJ_TYPE_LIST:
        type_str = "list";
        break;
    case TF_OBJ_TYPE_VARLIST:
        type_str = "varlist";
        break;
    case TF_OBJ_TYPE_VARFETCH:
        type_str = "varfetch";
        break;
    }
    stack_push(ctx, create_string_obj((char *)type_str, strlen(type_str)));
    release_obj(o);
    return TF_OK;
}

static int tf_func_name_cmp(const void *a, const void *b) {
    tf_func *const *fa = a;
    tf_func *const *fb = b;
    return compare_string_obj((*fa)->name, (*fb)->name);
}

typedef struct {
    char *ptr;
    size_t len;
    size_t cap;
} tf_strbuf;

static void tf_strbuf_init(tf_strbuf *buf) {
    buf->len = 0;
    buf->cap = 64;
    buf->ptr = xmalloc(buf->cap);
    buf->ptr[0] = '\0';
}

static void tf_strbuf_reserve(tf_strbuf *buf, size_t extra) {
    size_t needed = buf->len + extra + 1;
    while (needed > buf->cap) buf->cap *= 2;
    buf->ptr = xrealloc(buf->ptr, buf->cap);
}

static void tf_strbuf_append_mem(tf_strbuf *buf, const char *s, size_t len) {
    tf_strbuf_reserve(buf, len);
    memcpy(buf->ptr + buf->len, s, len);
    buf->len += len;
    buf->ptr[buf->len] = '\0';
}

static void tf_strbuf_append_cstr(tf_strbuf *buf, const char *s) {
    tf_strbuf_append_mem(buf, s, strlen(s));
}

static void tf_strbuf_append_char(tf_strbuf *buf, char c) {
    tf_strbuf_reserve(buf, 1);
    buf->ptr[buf->len++] = c;
    buf->ptr[buf->len] = '\0';
}

static void tf_strbuf_append_escaped(tf_strbuf *buf, const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        switch (s[i]) {
        case '\\':
            tf_strbuf_append_cstr(buf, "\\\\");
            break;
        case '"':
            tf_strbuf_append_cstr(buf, "\\\"");
            break;
        case '\n':
            tf_strbuf_append_cstr(buf, "\\n");
            break;
        case '\r':
            tf_strbuf_append_cstr(buf, "\\r");
            break;
        case '\t':
            tf_strbuf_append_cstr(buf, "\\t");
            break;
        default:
            tf_strbuf_append_char(buf, s[i]);
            break;
        }
    }
}

static void tf_strbuf_append_source_obj(tf_strbuf *buf, tf_obj *o) {
    char num_buf[64];

    switch (o->type) {
    case TF_OBJ_TYPE_INT:
        snprintf(num_buf, sizeof num_buf, "%d", o->i);
        tf_strbuf_append_cstr(buf, num_buf);
        break;
    case TF_OBJ_TYPE_FLOAT:
        snprintf(num_buf, sizeof num_buf, "%g", o->f);
        tf_strbuf_append_cstr(buf, num_buf);
        break;
    case TF_OBJ_TYPE_BOOL:
        tf_strbuf_append_cstr(buf, o->b ? "true" : "false");
        break;
    case TF_OBJ_TYPE_STR:
        tf_strbuf_append_char(buf, '"');
        tf_strbuf_append_escaped(buf, o->str.ptr, o->str.len);
        tf_strbuf_append_char(buf, '"');
        break;
    case TF_OBJ_TYPE_SYMBOL:
        if (o->str.quoted) tf_strbuf_append_char(buf, '\'');
        tf_strbuf_append_mem(buf, o->str.ptr, o->str.len);
        break;
    case TF_OBJ_TYPE_VARFETCH:
        tf_strbuf_append_char(buf, '$');
        tf_strbuf_append_mem(buf, o->str.ptr, o->str.len);
        break;
    case TF_OBJ_TYPE_VARLIST:
        tf_strbuf_append_char(buf, '{');
        for (size_t i = 0; i < o->list.len; i++) {
            if (i > 0) tf_strbuf_append_char(buf, ' ');
            tf_strbuf_append_source_obj(buf, o->list.elem[i]);
        }
        tf_strbuf_append_char(buf, '}');
        break;
    case TF_OBJ_TYPE_LIST:
        tf_strbuf_append_char(buf, '[');
        for (size_t i = 0; i < o->list.len; i++) {
            if (i > 0) tf_strbuf_append_char(buf, ' ');
            tf_strbuf_append_source_obj(buf, o->list.elem[i]);
        }
        tf_strbuf_append_char(buf, ']');
        break;
    }
}

tf_ret tf_words(tf_ctx *ctx) {
    size_t count = ctx->functions.count;
    tf_obj *result = init_list_obj();
    if (count == 0) {
        stack_push(ctx, result);
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
        push_obj(result, create_string_obj(funcs[i]->name->str.ptr,
                                           funcs[i]->name->str.len));
    }
    free(funcs);
    stack_push(ctx, result);
    return TF_OK;
}

tf_ret tf_see(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *name = stack_pop_type(ctx, TF_OBJ_TYPE_SYMBOL);
    if (!name) return TF_ERR;

    tf_func *func = get_func(ctx, name);
    if (!func) {
        release_obj(name);
        return TF_ERR;
    }

    tf_strbuf buf;
    tf_strbuf_init(&buf);
    if (func->type == TF_FUNC_TYPE_NATIVE) {
        tf_strbuf_append_mem(&buf, func->name->str.ptr, func->name->str.len);
        tf_strbuf_append_cstr(&buf, " is a native word");
    } else {
        tf_strbuf_append_char(&buf, '\'');
        tf_strbuf_append_mem(&buf, func->name->str.ptr, func->name->str.len);
        tf_strbuf_append_cstr(&buf, " [");
        for (size_t i = 0; i < func->user_impl->list.len; i++) {
            tf_strbuf_append_char(&buf, ' ');
            tf_strbuf_append_source_obj(&buf, func->user_impl->list.elem[i]);
        }
        tf_strbuf_append_cstr(&buf, " ] def");
    }

    stack_push(ctx, create_string_obj(buf.ptr, buf.len));
    free(buf.ptr);
    release_obj(name);
    return TF_OK;
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
