#include "tf_lib.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tf_alloc.h"
#include "tf_docs.h"
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
    tf_stack_push(ctx, tf_obj_new_bool(o->type == TF_OBJ_TYPE_VECTOR ||
                                       o->type == TF_OBJ_TYPE_LIST ||
                                       o->type == TF_OBJ_TYPE_STR));
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_callable_q(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    bool result =
        o->type == TF_OBJ_TYPE_VECTOR ||
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
tf_ret tf_string_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_STR);
}
tf_ret tf_symbol_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_SYMBOL);
}
tf_ret tf_vector_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_VECTOR);
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
tf_ret tf_deque_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_DEQUE);
}
tf_ret tf_pqueue_q(tf_ctx *ctx) {
    return type_check(ctx, TF_OBJ_TYPE_PQUEUE);
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
        type_str = "string";
        break;
    case TF_OBJ_TYPE_SYMBOL:
        type_str = "symbol";
        break;
    case TF_OBJ_TYPE_VECTOR:
        type_str = "vector";
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
    case TF_OBJ_TYPE_DEQUE:
        type_str = "deque";
        break;
    case TF_OBJ_TYPE_PQUEUE:
        type_str = "pqueue";
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
        unsigned char c = (unsigned char)s[i];
        switch (c) {
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
        default: {
            if (c < 0x20 || c >= 0x7f) {
                char escaped[5];
                snprintf(escaped, sizeof escaped, "\\x%02x", c);
                strbuf_append_cstr(buf, escaped);
            } else {
                strbuf_append_char(buf, (char)c);
            }
            break;
        }
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
        snprintf(num_buf, sizeof num_buf, "%.9g", o->f);
        strbuf_append_cstr(buf, num_buf);
        if (isfinite(o->f) && !strpbrk(num_buf, ".eE")) {
            strbuf_append_cstr(buf, ".0");
        }
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
        for (size_t i = 0; i < o->vector.len; i++) {
            if (i > 0) strbuf_append_char(buf, ' ');
            strbuf_append_source_obj(buf, o->vector.elem[i]);
        }
        strbuf_append_char(buf, '|');
        break;
    case TF_OBJ_TYPE_VECTOR:
        strbuf_append_char(buf, '[');
        for (size_t i = 0; i < o->vector.len; i++) {
            if (i > 0) strbuf_append_char(buf, ' ');
            strbuf_append_source_obj(buf, o->vector.elem[i]);
        }
        strbuf_append_char(buf, ']');
        break;
    case TF_OBJ_TYPE_LIST: {
        strbuf_append_char(buf, '(');
        tf_list_node *node = o->list.head;
        while (node) {
            strbuf_append_source_obj(buf, node->value);
            node = node->next;
            if (node) strbuf_append_char(buf, ' ');
        }
        strbuf_append_char(buf, ')');
        break;
    }
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
    case TF_OBJ_TYPE_DEQUE:
        strbuf_append_char(buf, '[');
        for (size_t i = 0; i < o->deque.len; i++) {
            if (i > 0) strbuf_append_char(buf, ' ');
            strbuf_append_source_obj(buf, tf_deque_get(o, i));
        }
        strbuf_append_cstr(buf, "] >deque");
        break;
    case TF_OBJ_TYPE_PQUEUE: {
        strbuf_append_char(buf, '[');
        tf_obj *tmp = tf_pqueue_clone(o);
        for (size_t i = 0; tmp->pqueue.len > 0; i++) {
            tf_obj *priority = NULL;
            tf_obj *value = NULL;
            tf_pqueue_pop(tmp, &priority, &value);
            if (i > 0) strbuf_append_char(buf, ' ');
            strbuf_append_cstr(buf, "[");
            strbuf_append_source_obj(buf, priority);
            strbuf_append_char(buf, ' ');
            strbuf_append_source_obj(buf, value);
            strbuf_append_char(buf, ']');
            tf_obj_release(priority);
            tf_obj_release(value);
        }
        tf_obj_release(tmp);
        strbuf_append_cstr(buf, "] >pqueue");
        break;
    }
    }
}

tf_ret tf_repr(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *value = tf_stack_pop(ctx);
    strbuf buf;
    strbuf_init(&buf);
    strbuf_append_source_obj(&buf, value);
    tf_stack_push(ctx, tf_obj_new_string_take(buf.ptr, buf.len));
    tf_obj_release(value);
    return TF_OK;
}

static void strbuf_append_word_source(strbuf *buf, tf_word *word) {
    if (word->type == TF_WORD_NATIVE) {
        strbuf_append_mem(buf, word->name->str.ptr, word->name->str.len);
        strbuf_append_cstr(buf, " is a native word");
        return;
    }

    strbuf_append_char(buf, '\'');
    strbuf_append_mem(buf, word->name->str.ptr, word->name->str.len);
    strbuf_append_cstr(buf, " [");
    for (size_t i = 0; i < word->user_impl->vector.len; i++) {
        strbuf_append_char(buf, ' ');
        strbuf_append_source_obj(buf, word->user_impl->vector.elem[i]);
    }
    strbuf_append_cstr(buf, " ] def");
}

static int ascii_lower(int c) {
    return tolower((unsigned char)c);
}

static bool contains_ascii_casefold(const char *haystack, size_t haystack_len,
                                    const char *needle, size_t needle_len) {
    if (needle_len == 0) return true;
    if (haystack_len < needle_len) return false;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++) {
            if (ascii_lower(haystack[i + j]) != ascii_lower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static bool word_matches_query(tf_word *word, const char *query,
                               size_t query_len) {
    if (contains_ascii_casefold(word->name->str.ptr, word->name->str.len,
                                query, query_len)) {
        return true;
    }

    if (word->type != TF_WORD_NATIVE) return false;

    const tf_doc_entry *doc = tf_doc_lookup(word->name->str.ptr);
    return doc &&
           (contains_ascii_casefold(doc->stack_effect, strlen(doc->stack_effect),
                                    query, query_len) ||
            contains_ascii_casefold(doc->syntax, strlen(doc->syntax),
                                    query, query_len) ||
            contains_ascii_casefold(doc->description, strlen(doc->description),
                                    query, query_len));
}

tf_ret tf_words(tf_ctx *ctx) {
    size_t count = ctx->words.count;
    tf_obj *result = tf_obj_new_vector_with_capacity(count);
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
        tf_vector_push(result, tf_obj_new_quoted_symbol(words[i]->name->str.ptr,
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
    strbuf_append_word_source(&buf, word);

    tf_stack_push(ctx, tf_obj_new_string_take(buf.ptr, buf.len));
    tf_obj_release(name);
    return TF_OK;
}

tf_ret tf_doc(tf_ctx *ctx) {
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
    const tf_doc_entry *entry =
        word->type == TF_WORD_NATIVE ? tf_doc_lookup(word->name->str.ptr) : NULL;
    if (entry) {
        strbuf_append_cstr(&buf, entry->name);
        if (entry->stack_effect[0] != '\0') {
            strbuf_append_cstr(&buf, "\nstack: ");
            strbuf_append_cstr(&buf, entry->stack_effect);
        }
        if (entry->syntax[0] != '\0') {
            strbuf_append_cstr(&buf, "\nsyntax: ");
            strbuf_append_cstr(&buf, entry->syntax);
        }
        if (entry->description[0] != '\0') strbuf_append_char(&buf, '\n');
        strbuf_append_cstr(&buf, entry->description);
    } else {
        strbuf_append_word_source(&buf, word);
    }

    tf_stack_push(ctx, tf_obj_new_string_take(buf.ptr, buf.len));
    tf_obj_release(name);
    return TF_OK;
}

tf_ret tf_apropos(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *query = tf_stack_peek(ctx, 0);
    if (query->type != TF_OBJ_TYPE_STR && query->type != TF_OBJ_TYPE_SYMBOL) {
        tf_ctx_runtime_errorf(
            ctx, "'%s' expected string or symbol at stack depth 0, found %s\n",
            ctx->current_word, tf_obj_type_name(query));
        return TF_ERR;
    }
    query = tf_stack_pop(ctx);

    tf_word **matches = NULL;
    size_t match_count = 0;
    if (ctx->words.count > 0) {
        matches = tf_xmalloc(sizeof(tf_word *) * ctx->words.count);
        for (size_t i = 0; i < ctx->words.capacity; i++) {
            tf_word *word = ctx->words.buckets[i];
            if (!word) continue;
            if (word_matches_query(word, query->str.ptr, query->str.len)) {
                matches[match_count++] = word;
            }
        }
        qsort(matches, match_count, sizeof(tf_word *), word_name_cmp);
    }

    tf_obj *result = tf_obj_new_vector_with_capacity(match_count);
    for (size_t i = 0; i < match_count; i++) {
        tf_vector_push(
            result, tf_obj_new_quoted_symbol(matches[i]->name->str.ptr,
                                             matches[i]->name->str.len));
    }

    free(matches);
    tf_obj_release(query);
    tf_stack_push(ctx, result);
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
    if (body->type != TF_OBJ_TYPE_VECTOR) {
        tf_ctx_runtime_errorf(ctx, "'def' expected vector at stack depth 0, found %s\n",
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
