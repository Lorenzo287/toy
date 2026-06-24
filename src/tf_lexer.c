#include "tf_lexer.h"
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tf_alloc.h"
#include "tf_console.h"

#define TF_TOP_LEVEL_INITIAL_CAP 8

static void lexer_advance(tf_lexer *lexer);
static tf_source_span lexer_mark(tf_lexer *lexer);
static void lexer_finish_span(tf_lexer *lexer, tf_source_span *span);

static void skip_spaces(tf_lexer *lexer) {
    while (isspace((unsigned char)lexer->pos[0])) lexer_advance(lexer);
}

static size_t lexer_offset(tf_lexer *lexer) {
    return (size_t)(lexer->pos - lexer->start);
}

static void lexer_advance(tf_lexer *lexer) {
    if (lexer->pos[0] == '\0') return;
    if (lexer->pos[0] == '\n') {
        lexer->line++;
        lexer->col = 1;
    } else {
        lexer->col++;
    }
    lexer->pos++;
}

static tf_source_span lexer_mark(tf_lexer *lexer) {
    return (tf_source_span){.source = lexer->source,
                            .offset = (uint32_t)lexer_offset(lexer),
                            .line = lexer->line,
                            .col = lexer->col,
                            .len = 1};
}

static void lexer_finish_span(tf_lexer *lexer, tf_source_span *span) {
    span->len = (uint32_t)(lexer_offset(lexer) - span->offset);
}

static const char *source_basename(const char *path) {
    if (!path) return "<unknown>";
    const char *name = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') name = p + 1;
    }
    return name;
}

static void lexer_errorf(tf_lexer *lexer, const char *fmt, ...) {
    va_list ap;
    lexer->error = 1;
    tf_console_lexer_errorf("");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "  at %s:%zu:%zu\n",
            source_basename(tf_source_file_name(lexer->source)),
            (size_t)lexer->line, (size_t)lexer->col);
}

int tf_lexer_is_symbol_char(int c) {
    if (c == '\0') return 0;
    unsigned char uc = (unsigned char)c;
    const char *sym_chars = "+-*/%<>=!.?";
    return isalpha(uc) || isdigit(uc) || c == '_' || strchr(sym_chars, uc) != NULL;
}

static tf_obj *lexer_tokenize_until(tf_lexer *lexer, int terminator);
static tf_obj *lexer_tokenize_number(tf_lexer *lexer);
static tf_obj *lexer_tokenize_single_char_symbol(tf_lexer *lexer);
static tf_obj *lexer_tokenize_symbol(tf_lexer *lexer);
static tf_obj *lexer_tokenize_string(tf_lexer *lexer);
static tf_obj *lexer_vector_to_map(tf_lexer *lexer, tf_obj *items);
static tf_obj *lexer_vector_to_set(tf_lexer *lexer, tf_obj *items);
static int lexer_starts_signed_number(tf_lexer *lexer);
static int lexer_at_token_boundary(tf_lexer *lexer);
static int lexer_is_structural_char(int c);
static int lexer_skip_block_comment(tf_lexer *lexer);

static int hex_value(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parse source text into a vector of runtime objects. */
tf_obj *tf_lexer_parse(const char *filename, char *prg_text) {
    tf_source_file *source = tf_source_file_new(filename);
    tf_lexer lexer_state = {.source = source,
                            .start = prg_text,
                            .pos = prg_text,
                            .line = 1,
                            .col = 1,
                            .error = 0};
    tf_obj *result = lexer_tokenize_until(&lexer_state, 0);
    tf_source_file_release(source);
    return result;
}

static tf_obj *lexer_tokenize_until(tf_lexer *lexer, int terminator) {
    tf_source_span vector_span = lexer_mark(lexer);
    tf_obj *prg = terminator == 0
                                  ? tf_obj_new_vector_with_capacity(
                                        TF_TOP_LEVEL_INITIAL_CAP)
                                  : tf_obj_new_vector();

    while (lexer->pos && lexer->pos[0] != 0) {
        skip_spaces(lexer);
        if (*lexer->pos == 0) break;  // end of program

        if (lexer->pos[0] == '\\') {
            while (lexer->pos[0] != '\n' && lexer->pos[0] != 0) {
                lexer_advance(lexer);
            }
            continue;
        }
        if (lexer->pos[0] == '/' && lexer->pos[1] == '*') {
            if (!lexer_skip_block_comment(lexer)) {
                tf_obj_release(prg);
                return NULL;
            }
            continue;
        }
        if (terminator && *lexer->pos == terminator) {
            lexer_advance(lexer);
            lexer_finish_span(lexer, &vector_span);
            tf_obj_set_span(prg, vector_span);
            return prg;
        }

        tf_obj *o = NULL;
        if (!terminator && (lexer->pos[0] == ']' || lexer->pos[0] == '}' ||
                            lexer->pos[0] == ')')) {
            lexer_errorf(lexer, "unexpected '%c'\n", lexer->pos[0]);
        } else if (isdigit((unsigned char)lexer->pos[0]) ||
                   lexer_starts_signed_number(lexer)) {
            o = lexer_tokenize_number(lexer);
        } else if (lexer->pos[0] == '[') {
            tf_source_span span = lexer_mark(lexer);
            lexer_advance(lexer);
            o = lexer_tokenize_until(lexer, ']');
            if (o) {
                lexer_finish_span(lexer, &span);
                tf_obj_set_span(o, span);
            }
        } else if (lexer->pos[0] == '(') {
            tf_source_span span = lexer_mark(lexer);
            lexer_advance(lexer);
            tf_obj *items = lexer_tokenize_until(lexer, ')');
            if (items) {
                o = tf_list_from_vector(items);
                tf_obj_release(items);
                if (o) {
                    lexer_finish_span(lexer, &span);
                    tf_obj_set_span(o, span);
                }
            }
        } else if (lexer->pos[0] == '{') {
            tf_source_span span = lexer_mark(lexer);
            lexer_advance(lexer);
            tf_obj *items = lexer_tokenize_until(lexer, '}');
            if (items) {
                o = lexer_vector_to_map(lexer, items);
                tf_obj_release(items);
                if (o) {
                    lexer_finish_span(lexer, &span);
                    tf_obj_set_span(o, span);
                }
            }
        } else if (lexer->pos[0] == '#') {
            tf_source_span span = lexer_mark(lexer);
            lexer_advance(lexer);
            if (lexer->pos[0] != '{') {
                lexer_errorf(lexer, "expected '{' after '#'\n");
            } else {
                lexer_advance(lexer);
                tf_obj *items = lexer_tokenize_until(lexer, '}');
                if (items) {
                    o = lexer_vector_to_set(lexer, items);
                    tf_obj_release(items);
                    if (o) {
                        lexer_finish_span(lexer, &span);
                        tf_obj_set_span(o, span);
                    }
                }
            }
        } else if (lexer->pos[0] == '|') {
            tf_source_span span = lexer_mark(lexer);
            lexer_advance(lexer);
            o = lexer_tokenize_until(lexer, '|');
            if (o) {
                o->type = TF_OBJ_TYPE_VARLIST;
                lexer_finish_span(lexer, &span);
                tf_obj_set_span(o, span);
            }
        } else if (lexer->pos[0] == '$') {
            tf_source_span span = lexer_mark(lexer);
            lexer_advance(lexer);
            o = lexer_tokenize_symbol(lexer);
            if (o) {
                if (o->type == TF_OBJ_TYPE_SYMBOL) {
                    o->type = TF_OBJ_TYPE_VARFETCH;
                    lexer_finish_span(lexer, &span);
                    tf_obj_set_span(o, span);
                } else {
                    tf_obj_release(o);
                    o = NULL;
                }
            } else if (!lexer->error) {
                lexer_errorf(lexer, "expected variable name after '$'\n");
            }
        } else if (lexer->pos[0] == '\'') {
            tf_source_span span = lexer_mark(lexer);
            lexer_advance(lexer);
            o = lexer_tokenize_symbol(lexer);
            if (o && o->type == TF_OBJ_TYPE_SYMBOL) {
                o->str.quoted = true;
                lexer_finish_span(lexer, &span);
                tf_obj_set_span(o, span);
            }
            if (!o && !lexer->error) {
                lexer_errorf(lexer, "expected symbol name after '\''\n");
            }
        } else if ((lexer->pos[0] == '-' || lexer->pos[0] == '+') &&
                   (isdigit((unsigned char)lexer->pos[1]) ||
                    (lexer->pos[1] == '.' &&
                     isdigit((unsigned char)lexer->pos[2])))) {
            o = lexer_tokenize_single_char_symbol(lexer);
        } else if (tf_lexer_is_symbol_char(lexer->pos[0])) {
            o = lexer_tokenize_symbol(lexer);
        } else if (lexer->pos[0] == '"') {
            o = lexer_tokenize_string(lexer);
        }

        if (o == NULL) {
            tf_obj_release(prg);
            if (!lexer->error) {
                lexer_errorf(lexer, "unexpected input near '%.16s'\n", lexer->pos);
            }
            return NULL;
        }
        tf_vector_push(prg, o);
    }

    if (terminator != 0) {
        tf_obj_release(prg);
        lexer_errorf(lexer, "expected '%c' but reached end of program\n", terminator);
        return NULL;
    }

    lexer_finish_span(lexer, &vector_span);
    tf_obj_set_span(prg, vector_span);
    return prg;
}

#define MAX_NUM_LEN 128
static tf_obj *lexer_tokenize_number(tf_lexer *lexer) {
    char buf[MAX_NUM_LEN];
    char *start = lexer->pos;
    bool flt = false;
    bool digit = false;

    tf_source_span span = lexer_mark(lexer);
    if (lexer->pos[0] == '-' || lexer->pos[0] == '+') { lexer_advance(lexer); }

    while (isdigit((unsigned char)lexer->pos[0])) {
        digit = true;
        lexer_advance(lexer);
    }

    if (lexer->pos[0] == '.') {
        flt = true;
        lexer_advance(lexer);
        while (isdigit((unsigned char)lexer->pos[0])) {
            digit = true;
            lexer_advance(lexer);
        }
    }

    if (!digit) {
        lexer_errorf(lexer, "malformed number literal\n");
        return NULL;
    }

    if (lexer->pos[0] == 'e' || lexer->pos[0] == 'E') {
        flt = true;
        lexer_advance(lexer);
        if (lexer->pos[0] == '-' || lexer->pos[0] == '+') {
            lexer_advance(lexer);
        }
        if (!isdigit((unsigned char)lexer->pos[0])) {
            lexer_errorf(lexer, "malformed number literal\n");
            return NULL;
        }
        while (isdigit((unsigned char)lexer->pos[0])) lexer_advance(lexer);
    }
    if (isalpha((unsigned char)lexer->pos[0]) || lexer->pos[0] == '_' ||
        lexer->pos[0] == '.') {
        lexer_errorf(lexer, "malformed number literal\n");
        return NULL;
    }

    size_t num_len = (size_t)(lexer->pos - start);
    if (num_len >= MAX_NUM_LEN) {
        lexer_errorf(lexer, "number literal is too long\n");
        return NULL;
    }
    memcpy(buf, start, num_len);
    buf[num_len] = 0;
    errno = 0;
    tf_obj *o = NULL;
    if (flt) {
        char *end = NULL;
        double value = strtod(buf, &end);
        if (errno == ERANGE || end != buf + num_len) {
            lexer_errorf(lexer, "floating-point literal is out of range\n");
            return NULL;
        }
        o = tf_obj_new_float(value);
    } else {
        char *end = NULL;
        int64_t value = strtoll(buf, &end, 10);
        if (errno == ERANGE || end != buf + num_len) {
            lexer_errorf(lexer, "integer literal is out of range\n");
            return NULL;
        }
        o = tf_obj_new_int(value);
    }
    lexer_finish_span(lexer, &span);
    tf_obj_set_span(o, span);
    return o;
}

static tf_obj *lexer_tokenize_single_char_symbol(tf_lexer *lexer) {
    tf_source_span span = lexer_mark(lexer);
    tf_obj *o = tf_obj_new_symbol(lexer->pos, 1);
    lexer_advance(lexer);
    lexer_finish_span(lexer, &span);
    tf_obj_set_span(o, span);
    return o;
}

static tf_obj *lexer_tokenize_symbol(tf_lexer *lexer) {
    tf_source_span span = lexer_mark(lexer);
    char *start = lexer->pos;
    while (tf_lexer_is_symbol_char(lexer->pos[0])) {
        if (lexer->pos[0] == '/' && lexer->pos[1] == '*') break;
        lexer_advance(lexer);
    }
    size_t sym_len = (size_t)(lexer->pos - start);
    if (sym_len == 0) return NULL;
    tf_obj *o = NULL;
    if (sym_len == 4 && !strncmp(start, "true", 4))
        o = tf_obj_new_bool(1);
    else if (sym_len == 5 && !strncmp(start, "false", 5))
        o = tf_obj_new_bool(0);
    else
        o = tf_obj_new_symbol(start, sym_len);
    lexer_finish_span(lexer, &span);
    tf_obj_set_span(o, span);
    return o;
}

static tf_obj *lexer_vector_to_map(tf_lexer *lexer, tf_obj *items) {
    if (items->vector.len % 2 != 0) {
        lexer_errorf(lexer, "map literal expected key/value pairs\n");
        return NULL;
    }

    tf_obj *map = tf_obj_new_map();
    tf_map_reserve(map, items->vector.len / 2);
    for (size_t i = 0; i < items->vector.len; i += 2) {
        tf_obj *key = items->vector.elem[i];
        tf_obj *value = items->vector.elem[i + 1];
        if (!tf_obj_hashable(key)) {
            lexer_errorf(lexer, "map literal key is not hashable\n");
            tf_obj_release(map);
            return NULL;
        }
        if (tf_map_has(map, key)) {
            lexer_errorf(lexer, "map literal contains a duplicate key\n");
            tf_obj_release(map);
            return NULL;
        }
        tf_map_set(map, key, value);
    }
    return map;
}

static tf_obj *lexer_vector_to_set(tf_lexer *lexer, tf_obj *items) {
    tf_obj *set = tf_obj_new_set();
    tf_set_reserve(set, items->vector.len);
    for (size_t i = 0; i < items->vector.len; i++) {
        tf_obj *item = items->vector.elem[i];
        if (!tf_obj_hashable(item)) {
            lexer_errorf(lexer, "set literal item is not hashable\n");
            tf_obj_release(set);
            return NULL;
        }
        if (tf_set_has(set, item)) {
            lexer_errorf(lexer, "set literal contains a duplicate item\n");
            tf_obj_release(set);
            return NULL;
        }
        tf_set_add(set, item);
    }
    return set;
}

static tf_obj *lexer_tokenize_string(tf_lexer *lexer) {
    tf_source_span span = lexer_mark(lexer);
    char *string_start = lexer->pos;
    lexer_advance(lexer);  // skip opening "
    char inline_buf[TF_STRING_INLINE_CAP + 1];
    size_t cap = sizeof(inline_buf);
    size_t len = 0;
    char *buf = inline_buf;

    while (lexer->pos[0] != '"' && lexer->pos[0] != 0) {
        if (len + 1 >= cap) {
            size_t new_cap = cap * 2;
            if (buf == inline_buf) {
                buf = tf_xmalloc(new_cap);
                memcpy(buf, inline_buf, len);
            } else {
                buf = tf_xrealloc(buf, new_cap);
            }
            cap = new_cap;
        }

        if (lexer->pos[0] == '\\') {
            lexer_advance(lexer);
            if (lexer->pos[0] == 0) break;
            switch (lexer->pos[0]) {
            case 'n':
                buf[len++] = '\n';
                break;
            case 'r':
                buf[len++] = '\r';
                break;
            case 't':
                buf[len++] = '\t';
                break;
            case 'x': {
                int high = lexer->pos[1] == '\0'
                               ? -1
                               : hex_value((unsigned char)lexer->pos[1]);
                int low = lexer->pos[1] == '\0' || lexer->pos[2] == '\0'
                              ? -1
                              : hex_value((unsigned char)lexer->pos[2]);
                if (high < 0 || low < 0) {
                    lexer_errorf(
                        lexer,
                        "invalid hexadecimal escape; expected \\x followed by two hexadecimal digits\n");
                    if (buf != inline_buf) free(buf);
                    return NULL;
                }
                buf[len++] = (char)((high << 4) | low);
                lexer_advance(lexer);
                lexer_advance(lexer);
                break;
            }
            case '"':
                buf[len++] = '"';
                break;
            case '\\':
                buf[len++] = '\\';
                break;
            default:
                lexer_errorf(lexer, "unknown string escape '\\%c'\n",
                             lexer->pos[0]);
                if (buf != inline_buf) free(buf);
                return NULL;
            }
        } else {
            buf[len++] = lexer->pos[0];
        }
        lexer_advance(lexer);
    }

    if (lexer->pos[0] != '"') {
        lexer->pos = string_start;
        lexer->line = span.line;
        lexer->col = span.col;
        lexer_errorf(lexer, "unterminated string literal\n");
        if (buf != inline_buf) free(buf);
        return NULL;
    }
    lexer_advance(lexer);  // skip closing "

    tf_obj *o = buf == inline_buf ? tf_obj_new_string(buf, len)
                                  : tf_obj_new_string_take(buf, len);
    lexer_finish_span(lexer, &span);
    tf_obj_set_span(o, span);
    return o;
}

static int lexer_starts_signed_number(tf_lexer *lexer) {
    if ((lexer->pos[0] != '-' && lexer->pos[0] != '+') ||
        !lexer_at_token_boundary(lexer)) {
        return 0;
    }
    return isdigit((unsigned char)lexer->pos[1]) ||
           (lexer->pos[1] == '.' && isdigit((unsigned char)lexer->pos[2]));
}

static int lexer_at_token_boundary(tf_lexer *lexer) {
    if (lexer->pos == lexer->start) return 1;

    unsigned char prev = (unsigned char)lexer->pos[-1];
    if (prev == '/' && lexer->pos - lexer->start >= 2 &&
        lexer->pos[-2] == '*') {
        return 1;
    }
    return isspace(prev) || lexer_is_structural_char(prev) || prev == '(' ||
           prev == ')' || prev == '\\';
}

static int lexer_is_structural_char(int c) {
    return c == '[' || c == ']' || c == '{' || c == '}' || c == '|' ||
           c == '(' || c == ')';
}

static int lexer_skip_block_comment(tf_lexer *lexer) {
    tf_source_span span = lexer_mark(lexer);

    lexer_advance(lexer);
    lexer_advance(lexer);
    while (lexer->pos[0] != '\0') {
        if (lexer->pos[0] == '*' && lexer->pos[1] == '/') {
            lexer_advance(lexer);
            lexer_advance(lexer);
            return 1;
        }
        lexer_advance(lexer);
    }

    lexer->pos = lexer->start + span.offset;
    lexer->line = span.line;
    lexer->col = span.col;
    lexer_errorf(lexer, "unterminated block comment\n");
    return 0;
}
