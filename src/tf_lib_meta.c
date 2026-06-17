#include "tf_lib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tf_alloc.h"
#include "tf_exec.h"
#include "tf_obj.h"

tf_ret tf_number_q(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    tf_stack_push(ctx, tf_obj_new_bool(o->type == TF_OBJ_TYPE_INT ||
                                    o->type == TF_OBJ_TYPE_FLOAT));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_sequence_q(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    tf_stack_push(ctx, tf_obj_new_bool(o->type == TF_OBJ_TYPE_LIST ||
                                       o->type == TF_OBJ_TYPE_STR));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_callable_q(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    bool result =
        o->type == TF_OBJ_TYPE_LIST ||
        (o->type == TF_OBJ_TYPE_SYMBOL && tf_dict_lookup(ctx, o) != NULL);
    tf_stack_push(ctx, tf_obj_new_bool(result));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_nan_q(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    bool result = o->type == TF_OBJ_TYPE_FLOAT && isnan(o->f);
    tf_stack_push(ctx, tf_obj_new_bool(result));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_inf_q(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    bool result = o->type == TF_OBJ_TYPE_FLOAT && isinf(o->f);
    tf_stack_push(ctx, tf_obj_new_bool(result));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_word_q(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    bool result = o->type == TF_OBJ_TYPE_SYMBOL && tf_dict_lookup(ctx, o) != NULL;
    tf_stack_push(ctx, tf_obj_new_bool(result));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_var_q(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    bool result =
        o->type == TF_OBJ_TYPE_SYMBOL && tf_scope_lookup_var(ctx, o) != NULL;
    tf_stack_push(ctx, tf_obj_new_bool(result));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_inf(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_float(INFINITY));
    return TF_OK;
}

tf_ret tf_nan(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_float(NAN));
    return TF_OK;
}

tf_ret tf_body(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_SYMBOL)) return TF_ERR;
    tf_obj *name = tf_stack_pop_type(ctx, TF_OBJ_TYPE_SYMBOL);

    tf_word *f = tf_dict_lookup(ctx, name);
    if (!f || f->type != TF_WORD_USER) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected a user-defined word, found '%s'\n",
                              ctx->current_word, name->str.ptr);
        tf_obj_release(name);
        return TF_ERR;
    }

    tf_stack_push(ctx, f->user_impl);
    tf_obj_retain(f->user_impl);
    tf_obj_release(name);
    return TF_OK;
}

tf_ret tf_intern(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *str = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

    tf_stack_push(ctx, tf_obj_new_quoted_symbol(str->str.ptr, str->str.len));
    tf_obj_release(str);
    return TF_OK;
}

tf_ret tf_name(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_SYMBOL)) return TF_ERR;
    tf_obj *sym = tf_stack_pop_type(ctx, TF_OBJ_TYPE_SYMBOL);

    tf_stack_push(ctx, tf_obj_new_string(sym->str.ptr, sym->str.len));
    tf_obj_release(sym);
    return TF_OK;
}

static tf_ret type_check(tf_ctx *ctx, tf_type type) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    tf_stack_push(ctx, tf_obj_new_bool(o->type == type));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_bool_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_BOOL);
}
tf_ret tf_int_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_INT);
}
tf_ret tf_float_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_FLOAT);
}
tf_ret tf_str_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_STR);
}
tf_ret tf_symbol_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_SYMBOL);
}
tf_ret tf_list_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_LIST);
}
tf_ret tf_map_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_MAP);
}
tf_ret tf_set_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_SET);
}

tf_ret tf_typeof(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
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
    case TF_OBJ_TYPE_MAP:
        type_str = "map";
        break;
    case TF_OBJ_TYPE_SET:
        type_str = "set";
        break;
    case TF_OBJ_TYPE_VARLIST:
        type_str = "varlist";
        break;
    case TF_OBJ_TYPE_VARFETCH:
        type_str = "varfetch";
        break;
    }
    tf_stack_push(ctx, tf_obj_new_string((char *)type_str, strlen(type_str)));
    tf_obj_release(o);
    return TF_OK;
}

static int word_name_cmp(const void *a, const void *b) {
    tf_word *const *fa = a;
    tf_word *const *fb = b;
    return tf_obj_compare_string((*fa)->name, (*fb)->name);
}

typedef struct {
    char *ptr;
    size_t len;
    size_t cap;
} strbuf;

static void strbuf_init(strbuf *buf) {
    buf->len = 0;
    buf->cap = 64;
    buf->ptr = tf_xmalloc(buf->cap);
    buf->ptr[0] = '\0';
}

static void strbuf_reserve(strbuf *buf, size_t extra) {
    size_t needed = buf->len + extra + 1;
    while (needed > buf->cap) buf->cap *= 2;
    buf->ptr = tf_xrealloc(buf->ptr, buf->cap);
}

static void strbuf_append_mem(strbuf *buf, const char *s, size_t len) {
    strbuf_reserve(buf, len);
    memcpy(buf->ptr + buf->len, s, len);
    buf->len += len;
    buf->ptr[buf->len] = '\0';
}

static void strbuf_append_cstr(strbuf *buf, const char *s) {
    strbuf_append_mem(buf, s, strlen(s));
}

static void strbuf_append_char(strbuf *buf, char c) {
    strbuf_reserve(buf, 1);
    buf->ptr[buf->len++] = c;
    buf->ptr[buf->len] = '\0';
}

static void strbuf_append_escaped(strbuf *buf, const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        switch (s[i]) {
        case '\\':
            strbuf_append_cstr(buf, "\\\\");
            break;
        case '"':
            strbuf_append_cstr(buf, "\\\"");
            break;
        case '\n':
            strbuf_append_cstr(buf, "\\n");
            break;
        case '\r':
            strbuf_append_cstr(buf, "\\r");
            break;
        case '\t':
            strbuf_append_cstr(buf, "\\t");
            break;
        default:
            strbuf_append_char(buf, s[i]);
            break;
        }
    }
}

static void strbuf_append_source_obj(strbuf *buf, tf_obj *o) {
    char num_buf[64];

    switch (o->type) {
    case TF_OBJ_TYPE_INT:
        snprintf(num_buf, sizeof num_buf, "%d", o->i);
        strbuf_append_cstr(buf, num_buf);
        break;
    case TF_OBJ_TYPE_FLOAT:
        snprintf(num_buf, sizeof num_buf, "%g", o->f);
        strbuf_append_cstr(buf, num_buf);
        break;
    case TF_OBJ_TYPE_BOOL:
        strbuf_append_cstr(buf, o->b ? "true" : "false");
        break;
    case TF_OBJ_TYPE_STR:
        strbuf_append_char(buf, '"');
        strbuf_append_escaped(buf, o->str.ptr, o->str.len);
        strbuf_append_char(buf, '"');
        break;
    case TF_OBJ_TYPE_SYMBOL:
        if (o->str.quoted) strbuf_append_char(buf, '\'');
        strbuf_append_mem(buf, o->str.ptr, o->str.len);
        break;
    case TF_OBJ_TYPE_VARFETCH:
        strbuf_append_char(buf, '$');
        strbuf_append_mem(buf, o->str.ptr, o->str.len);
        break;
    case TF_OBJ_TYPE_VARLIST:
        strbuf_append_char(buf, '|');
        for (size_t i = 0; i < o->list.len; i++) {
            if (i > 0) strbuf_append_char(buf, ' ');
            strbuf_append_source_obj(buf, o->list.elem[i]);
        }
        strbuf_append_char(buf, '|');
        break;
    case TF_OBJ_TYPE_LIST:
        strbuf_append_char(buf, '[');
        for (size_t i = 0; i < o->list.len; i++) {
            if (i > 0) strbuf_append_char(buf, ' ');
            strbuf_append_source_obj(buf, o->list.elem[i]);
        }
        strbuf_append_char(buf, ']');
        break;
    case TF_OBJ_TYPE_MAP:
        strbuf_append_char(buf, '{');
        for (size_t i = 0; i < o->map.len; i++) {
            if (i > 0) strbuf_append_char(buf, ' ');
            strbuf_append_source_obj(buf, o->map.entries[i].key);
            strbuf_append_char(buf, ' ');
            strbuf_append_source_obj(buf, o->map.entries[i].value);
        }
        strbuf_append_char(buf, '}');
        break;
    case TF_OBJ_TYPE_SET:
        strbuf_append_cstr(buf, "#{");
        for (size_t i = 0; i < o->set.len; i++) {
            if (i > 0) strbuf_append_char(buf, ' ');
            strbuf_append_source_obj(buf, o->set.entries[i].item);
        }
        strbuf_append_char(buf, '}');
        break;
    }
}

tf_ret tf_words(tf_ctx *ctx) {
    size_t count = ctx->words.count;
    tf_obj *result = tf_obj_new_list();
    if (count == 0) {
        tf_stack_push(ctx, result);
        return TF_OK;
    }

    tf_word **words = tf_xmalloc(sizeof(tf_word *) * count);
    size_t j = 0;
    for (size_t i = 0; i < ctx->words.capacity; i++) {
        tf_word *word = ctx->words.buckets[i];
        if (word != NULL) words[j++] = word;
    }

    qsort(words, j, sizeof(tf_word *), word_name_cmp);
    for (size_t i = 0; i < j; i++) {
        tf_list_push(result, tf_obj_new_quoted_symbol(words[i]->name->str.ptr,
                                                      words[i]->name->str.len));
    }
    free(words);
    tf_stack_push(ctx, result);
    return TF_OK;
}

tf_ret tf_see(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_SYMBOL)) return TF_ERR;
    tf_obj *name = tf_stack_pop_type(ctx, TF_OBJ_TYPE_SYMBOL);

    tf_word *word = tf_dict_lookup(ctx, name);
    if (!word) {
        tf_ctx_runtime_errorf(ctx, "'%s' expected a defined word, found '%s'\n",
                              ctx->current_word, name->str.ptr);
        tf_obj_release(name);
        return TF_ERR;
    }

    strbuf buf;
    strbuf_init(&buf);
    if (word->type == TF_WORD_NATIVE) {
        strbuf_append_mem(&buf, word->name->str.ptr, word->name->str.len);
        strbuf_append_cstr(&buf, " is a native word");
    } else {
        strbuf_append_char(&buf, '\'');
        strbuf_append_mem(&buf, word->name->str.ptr, word->name->str.len);
        strbuf_append_cstr(&buf, " [");
        for (size_t i = 0; i < word->user_impl->list.len; i++) {
            strbuf_append_char(&buf, ' ');
            strbuf_append_source_obj(&buf, word->user_impl->list.elem[i]);
        }
        strbuf_append_cstr(&buf, " ] def");
    }

    tf_stack_push(ctx, tf_obj_new_string(buf.ptr, buf.len));
    free(buf.ptr);
    tf_obj_release(name);
    return TF_OK;
}

tf_ret tf_colon(tf_ctx *ctx) {
    if (ctx->call_stack_len == 0) {
        tf_ctx_runtime_errorf(ctx, "':' requires an active program frame\n");
        return TF_ERR;
    }
    tf_frame *f = &ctx->call_stack[ctx->call_stack_len - 1];

    if (f->pc >= f->program->list.len) {
        tf_ctx_runtime_errorf(ctx, "':' expected a word name\n");
        return TF_ERR;
    }
    tf_obj *word_name = f->program->list.elem[f->pc];
    if (word_name->type != TF_OBJ_TYPE_SYMBOL) {
        tf_ctx_runtime_errorf(ctx, "':' expected symbol word name, found %s\n",
                              tf_obj_type_name(word_name));
        return TF_ERR;
    }

    tf_obj *body = tf_obj_new_list();
    f->pc++;
    while (f->pc < f->program->list.len) {
        tf_obj *o = f->program->list.elem[f->pc];
        if (o->type == TF_OBJ_TYPE_SYMBOL && strcmp(o->str.ptr, ";") == 0) break;
        tf_list_push(body, o);
        tf_obj_retain(o);
        f->pc++;
    }

    if (f->pc >= f->program->list.len) {
        tf_obj_release(body);
        tf_ctx_runtime_errorf(ctx, "':' expected ';' to close definition\n");
        return TF_ERR;
    }

    tf_dict_set_user(ctx, word_name, body);
    tf_obj_release(body);
    f->pc++;
    return TF_OK;
}

tf_ret tf_def(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 2)) return TF_ERR;
    tf_obj *body = tf_stack_peek(ctx, 0);
    tf_obj *word_name = tf_stack_peek(ctx, 1);
    if (word_name->type != TF_OBJ_TYPE_SYMBOL) {
        tf_ctx_runtime_errorf(ctx, "'def' expected symbol at stack depth 1, found %s\n",
                              tf_obj_type_name(word_name));
        return TF_ERR;
    }
    if (body->type != TF_OBJ_TYPE_LIST) {
        tf_ctx_runtime_errorf(ctx, "'def' expected list at stack depth 0, found %s\n",
                              tf_obj_type_name(body));
        return TF_ERR;
    }

    body = tf_stack_pop(ctx);
    word_name = tf_stack_pop(ctx);
    tf_dict_set_user(ctx, word_name, body);

    tf_obj_release(body);
    tf_obj_release(word_name);
    return TF_OK;
}
