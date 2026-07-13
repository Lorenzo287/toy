#ifndef TF_LEXER_H
#define TF_LEXER_H

#include "tf_obj.h"

typedef struct {
    tf_source_file *source;
    const char *start;
    const char *pos;
    uint32_t line;
    uint32_t col;
    int error;
} tf_lexer;

/* Parse Toy source into a top-level program vector. */
tf_obj *tf_lexer_parse(const char *filename, const char *prg);

/* Shared by the lexer and REPL input scanner. */
int tf_lexer_is_symbol_char(int c);

#endif  // TF_LEXER_H
