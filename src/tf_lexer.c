#include "tf_lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tf_alloc.h"
#include "tf_console.h"

static void skip_spaces(tf_lexer *lexer) {
    while (isspace(lexer->pos[0])) lexer->pos++;
}

int tf_lexer_is_symbol_char(int c) {
    // NOTE: the lexer is intentionally mostly whitespace-delimited. We only
    // split punctuation into standalone tokens when it has structural meaning
    // in the grammar (currently ':' and ';'). Other punctuation can still be
    // part of ordinary word names such as '.s', '<=', or 'empty?'.
    if (c == '\0') return 0;
    unsigned char uc = (unsigned char)c;
    const char *sym_chars = "+-*/%<>=!.?";
    return isalpha(uc) || isdigit(uc) || c == '_' || strchr(sym_chars, uc) != NULL;
}

static tf_obj *lexer_tokenize_until(tf_lexer *lexer, int terminator);
static tf_obj *lexer_tokenize_number(tf_lexer *lexer);
static tf_obj *lexer_tokenize_symbol(tf_lexer *lexer);
static tf_obj *lexer_tokenize_string(tf_lexer *lexer);

/* Parse source text into a list of runtime objects. */
tf_obj *tf_lexer_parse(char *prg_text) {
    tf_lexer lexer_state = {.start = prg_text, .pos = prg_text};
    return lexer_tokenize_until(&lexer_state, 0);
}

static tf_obj *lexer_tokenize_until(tf_lexer *lexer, int terminator) {
    tf_obj *prg = tf_obj_new_list();

    while (lexer->pos && lexer->pos[0] != 0) {
        skip_spaces(lexer);
        if (*lexer->pos == 0) break;  // end of program

        if (lexer->pos[0] == '\\') {
            while (lexer->pos[0] != '\n' && lexer->pos[0] != 0) lexer->pos++;
            continue;
        }
        if (lexer->pos[0] == '(') {
            lexer->pos++;
            while (lexer->pos[0] != 0 && lexer->pos[0] != ')') { lexer->pos++; }
            if (lexer->pos[0] != 0) lexer->pos++;
            continue;
        }

        if (terminator && *lexer->pos == terminator) {
            lexer->pos++;
            return prg;
        }

        tf_obj *o = NULL;
        if (isdigit(lexer->pos[0])) {
            o = lexer_tokenize_number(lexer);
        } else if (lexer->pos[0] == '[') {
            lexer->pos++;
            o = lexer_tokenize_until(lexer, ']');
        } else if (lexer->pos[0] == '{') {
            lexer->pos++;
            o = lexer_tokenize_until(lexer, '}');
            if (o) o->type = TF_OBJ_TYPE_VARLIST;
        } else if (lexer->pos[0] == '$') {
            lexer->pos++;
            o = lexer_tokenize_symbol(lexer);
            if (o) {
                if (o->type == TF_OBJ_TYPE_SYMBOL) {
                    o->type = TF_OBJ_TYPE_VARFETCH;
                } else {
                    tf_obj_release(o);
                    o = NULL;
                }
            }
        } else if (lexer->pos[0] == '\'') {
            lexer->pos++;
            o = lexer_tokenize_symbol(lexer);
            if (o && o->type == TF_OBJ_TYPE_SYMBOL) { o->str.quoted = true; }
        } else if (lexer->pos[0] == ':') {
            o = tf_obj_new_symbol(lexer->pos, 1);
            lexer->pos++;
        } else if (lexer->pos[0] == ';') {
            o = tf_obj_new_symbol(lexer->pos, 1);
            lexer->pos++;
        } else if (tf_lexer_is_symbol_char(lexer->pos[0])) {
            o = lexer_tokenize_symbol(lexer);
        } else if (lexer->pos[0] == '"' && lexer->pos[1] != 0) {
            o = lexer_tokenize_string(lexer);
        }

        if (o == NULL) {
            tf_obj_release(prg);
            tf_console_lexer_errorf("syntax at character %zu: ... %.16s ...\n",
                                    lexer->pos - lexer->start, lexer->pos);
            return NULL;
        }
        tf_list_push(prg, o);
    }

    if (terminator != 0) {
        tf_obj_release(prg);
        tf_console_lexer_errorf("expected '%c' but reached end of program\n",
                                terminator);
        return NULL;
    }

    return prg;
}

#define MAX_NUM_LEN 128
static tf_obj *lexer_tokenize_number(tf_lexer *lexer) {
    char buf[MAX_NUM_LEN];
    char *start = lexer->pos;
    bool flt = false;

    while (isdigit(lexer->pos[0]) || lexer->pos[0] == '.') {
        if (lexer->pos[0] == '.') {
            if (flt) break;
            flt = true;
        }
        lexer->pos++;
    }
    int num_len = lexer->pos - start;
    if (num_len >= MAX_NUM_LEN) return NULL;
    memcpy(buf, start, num_len);
    buf[num_len] = 0;
    return flt ? tf_obj_new_float(atof(buf)) : tf_obj_new_int(atoi(buf));
}

static tf_obj *lexer_tokenize_symbol(tf_lexer *lexer) {
    char *start = lexer->pos;
    while (tf_lexer_is_symbol_char(lexer->pos[0])) lexer->pos++;
    int sym_len = lexer->pos - start;
    tf_obj *o = NULL;
    if (sym_len == 4 && !strncmp(start, "true", 4))
        o = tf_obj_new_bool(1);
    else if (sym_len == 5 && !strncmp(start, "false", 5))
        o = tf_obj_new_bool(0);
    else
        o = tf_obj_new_symbol(start, sym_len);
    return o;
}

static tf_obj *lexer_tokenize_string(tf_lexer *lexer) {
    lexer->pos++;  // skip opening "
    size_t cap = 64;
    size_t len = 0;
    char *buf = tf_xmalloc(cap);

    while (lexer->pos[0] != '"' && lexer->pos[0] != 0) {
        if (len + 1 >= cap) {
            cap *= 2;
            buf = tf_xrealloc(buf, cap);
        }

        if (lexer->pos[0] == '\\') {
            lexer->pos++;
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
            case '0': {
                if (lexer->pos[1] == '3' && lexer->pos[2] == '3') {
                    buf[len++] = '\033';
                    lexer->pos += 2;
                } else {
                    buf[len++] = '0';
                }
                break;
            }
            case '"':
                buf[len++] = '"';
                break;
            case '\\':
                buf[len++] = '\\';
                break;
            default:
                buf[len++] = lexer->pos[0];
                break;
            }
        } else {
            buf[len++] = lexer->pos[0];
        }
        lexer->pos++;
    }

    if (lexer->pos[0] != '"') {
        free(buf);
        return NULL;
    }
    lexer->pos++;  // skip closing "

    tf_obj *o = tf_obj_new_string(buf, len);
    free(buf);
    return o;
}
