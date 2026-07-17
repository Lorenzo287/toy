#ifndef TF_PARSER_H
#define TF_PARSER_H

#include "tf_obj.h"

typedef struct tf_ctx tf_ctx;

/* Parse Toy source into an owned top-level program vector. The context may be
   NULL when parse errors should be written directly to stderr. */
tf_obj *tf_parse_source(tf_ctx *ctx, const char *filename,
                        const char *source);

/* Shared by the parser and REPL input scanner. */
int tf_parser_is_symbol_char(int c);

#endif  // TF_PARSER_H
