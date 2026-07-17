#ifndef TF_TERMINAL_H
#define TF_TERMINAL_H

#include <stdbool.h>
#include <stddef.h>

#define TF_CLR_RESET "\x1b[0m"
#define TF_CLR_PROMPT "\x1b[96m"
#define TF_CLR_OK "\x1b[92m"
#define TF_CLR_ERR "\x1b[91m"
#define TF_CLR_PROGRAM_ERR "\x1b[38;5;208m"
#define TF_CLR_WARN "\x1b[93m"
#define TF_CLR_INFO "\x1b[90m"

/* Process-terminal capability and ANSI color handling. */
void tf_terminal_init(void);
bool tf_terminal_use_color(void);
const char *tf_terminal_color(const char *code);
size_t tf_terminal_width(void);

#endif  // TF_TERMINAL_H
