#ifndef TF_LEXER_H
#define TF_LEXER_H

#include "tf_obj.h"

typedef struct {
    char *start;
    char *pos;
} tf_lexer;

/* Parse Toy source into a top-level program list. */
tf_obj *tf_lexer_parse(char *prg);

/* Shared by the lexer and REPL input scanner. */
int tf_lexer_is_symbol_char(int c);

#endif  // TF_LEXER_H
