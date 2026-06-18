#include "tf_repl.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "tf_alloc.h"
#include "tf_console.h"
#include "tf_docs.h"
#include "tf_lexer.h"
#include "tf_obj.h"
#include <linenoise.h>

typedef struct {
    int block_depth;
    int brace_depth;
    int var_depth;
    bool in_string;
    bool escape;
    bool line_comment;
    bool block_comment;
    int list_depth;
} repl_state;

static tf_ret run_source(tf_ctx *ctx, const char *filename, char *source,
                         bool debug);
static char *read_repl_line(bool complete);
static bool append_text(char **buf, size_t *len, size_t *cap, const char *text);
static void reset_state(repl_state *state);
static void feed_state(repl_state *state, const char *text);
static bool input_complete(const repl_state *state);
static void init_repl_ui(tf_ctx *ctx);
static void free_repl_ui(void);
static void save_history_atexit(void);

static tf_ctx *completion_ctx = NULL;
static char *history_path = NULL;
static bool hints_enabled = true;
static bool trace_enabled = true;
static int hints_color = 90;  // Bright black / Gray
static int history_atexit_registered = 0;

static void save_history_atexit(void) {
    if (history_path) { linenoiseHistorySave(history_path); }
}

static char *get_history_path(void);
static void repl_completion(const char *buf, linenoiseCompletions *lc);
static char *repl_hints(const char *buf, int *color, int *bold);
static void repl_free_hints(void *ptr);
static char *doc_stack_hint(const tf_doc_entry *doc);

tf_ret tf_run_file(tf_ctx *ctx, const char *filename, bool debug) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Failed to open program");
        return TF_ERR;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);

    char *prg_text = tf_xmalloc(size + 1);
    size_t n_read = fread(prg_text, 1, size, fp);
    prg_text[n_read] = '\0';
    fclose(fp);

    tf_ret result = run_source(ctx, filename, prg_text, debug);
    free(prg_text);
    return result;
}

static tf_ret hints_toggle(tf_ctx *ctx) {
    (void)ctx;
    hints_enabled = !hints_enabled;
    printf("%shints: %s%s\n", tf_console_clr(TF_CLR_INFO),
           tf_console_clr(TF_CLR_RESET), hints_enabled ? "on" : "off");
    return TF_OK;
}

static tf_ret trace_toggle(tf_ctx *ctx) {
    (void)ctx;
    trace_enabled = !trace_enabled;
    printf("%strace: %s%s\n", tf_console_clr(TF_CLR_INFO),
           tf_console_clr(TF_CLR_RESET), trace_enabled ? "on" : "off");
    return TF_OK;
}

tf_ret tf_run_repl(tf_ctx *ctx, bool debug) {
    char *source = NULL;
    size_t len = 0;
    size_t cap = 0;
    repl_state state;
    bool ctrl_c_exit_armed = false;

    reset_state(&state);
    init_repl_ui(ctx);
    printf("%s=== Toy REPL ===%s\n", tf_console_clr(TF_CLR_PROMPT),
           tf_console_clr(TF_CLR_RESET));
    printf("%sType 'hints' to toggle hints, 'trace' to toggle stack display.%s\n",
           tf_console_clr(TF_CLR_INFO),
           tf_console_clr(TF_CLR_RESET));
#ifdef _WIN32
    printf("%sPress Ctrl-Z to exit.%s\n", tf_console_clr(TF_CLR_INFO),
           tf_console_clr(TF_CLR_RESET));
#else
    printf("%sPress Ctrl-D to exit.%s\n", tf_console_clr(TF_CLR_INFO),
           tf_console_clr(TF_CLR_RESET));
#endif

    while (1) {
        char *line = read_repl_line(input_complete(&state));
        if (!line) {
            if (errno == EAGAIN) {
                printf("^C\n");
                if (ctrl_c_exit_armed) { break; }

                len = 0;
                if (source) source[0] = '\0';
                reset_state(&state);
                tf_console_interruptf("press Ctrl-C again to exit REPL\n");
                ctrl_c_exit_armed = true;
                continue;
            }
            if (len > 0) {
                fprintf(stderr, "\n%sIncomplete input discarded.%s\n",
                        tf_console_clr(TF_CLR_ERR), tf_console_clr(TF_CLR_RESET));
            } else {
                printf("\n");
            }
            break;
        }

        ctrl_c_exit_armed = false;

        if (len == 0 && line[0] == '\0') {
            linenoiseFree(line);
            continue;
        }

        if (line[0] != '\0') { linenoiseHistoryAdd(line); }

        if (!append_text(&source, &len, &cap, line) ||
            !append_text(&source, &len, &cap, "\n")) {
            linenoiseFree(line);
            free(source);
            tf_console_contextf("failed to grow REPL buffer\n");
            return TF_ERR;
        }

        feed_state(&state, line);
        feed_state(&state, "\n");
        linenoiseFree(line);

        if (!input_complete(&state)) { continue; }

        tf_ret result = run_source(ctx, "<repl>", source, debug);
        if (result == TF_ERR) {
            printf("%snot ok%s\n", tf_console_clr(TF_CLR_ERR),
                   tf_console_clr(TF_CLR_RESET));
        } else if (result == TF_INTERRUPTED) {
            fflush(stdout);
        } else {
            if (trace_enabled) {
                size_t stack_len = tf_stack_len(ctx);
                printf("%s<%zu>%s", tf_console_clr(TF_CLR_OK), stack_len,
                       tf_console_clr(TF_CLR_RESET));
                if (stack_len > 0) printf(" ");
                for (size_t i = 0; i < stack_len; i++) {
                    tf_obj_print_source(tf_stack_peek(ctx, stack_len - 1 - i));
                    if (i < stack_len - 1) printf(" ");
                }
                printf("\n");
            } else {
                printf("%sok%s\n", tf_console_clr(TF_CLR_OK),
                       tf_console_clr(TF_CLR_RESET));
            }
        }
        fflush(stdout);

        ctrl_c_exit_armed = false;
        len = 0;
        if (source) source[0] = '\0';
        reset_state(&state);
    }

    free_repl_ui();
    free(source);
    return TF_OK;
}

static tf_ret run_source(tf_ctx *ctx, const char *filename, char *source,
                         bool debug) {
    tf_obj *prg = tf_lexer_parse(filename, source);
    if (!prg) return TF_ERR;

    if (debug) {
        printf("=== Tokenized program ===\n");
        size_t count = 0;
        tf_obj_print(prg, &count);
        printf("\n\n");
    }

    tf_ret result = tf_vm_exec(ctx, prg);
    if (result == TF_INTERRUPTED) {
        tf_console_interruptf("execution interrupted\n");
        tf_obj_release(prg);
        return TF_INTERRUPTED;
    }

    if (debug) {
        printf("\n=== Stack content after execution ===\n");
        size_t count = 0;
        tf_obj_print(ctx->data_stack, &count);
        printf("\n");
    }

    tf_obj_release(prg);
    return result;
}

tf_ret tf_run_string(tf_ctx *ctx, const char *source, bool debug) {
    char *dup = tf_xstrdup(source);
    tf_ret res = run_source(ctx, "<eval>", dup, debug);
    free(dup);
    return res;
}

static void repl_completion(const char *buf, linenoiseCompletions *lc) {
    if (!completion_ctx) return;

    const char *token = buf;
    const char *prefix = "";
    size_t prefix_len = 0;

    for (const char *p = buf; *p != '\0'; p++) {
        if (isspace((unsigned char)*p) || *p == '[' || *p == ']' || *p == '|' ||
            *p == '{' || *p == '}' || *p == '(' || *p == ')') {
            token = p + 1;
        }
    }

    if (*token == '\'' || *token == '$') {
        prefix = token;
        token++;
        prefix_len = 1;
    }

    size_t stem_len = strlen(token);
    size_t head_len = (size_t)(token - buf);

    for (size_t i = 0; i < completion_ctx->words.capacity; i++) {
        tf_word *func = completion_ctx->words.buckets[i];
        if (!func) continue;

        if (strncmp(func->name->str.ptr, token, stem_len) != 0) continue;

        size_t word_len = func->name->str.len;
        char *completion = tf_xmalloc(head_len + prefix_len + word_len + 1);
        memcpy(completion, buf, head_len);
        if (prefix_len) memcpy(completion + head_len, prefix, prefix_len);
        memcpy(completion + head_len + prefix_len, func->name->str.ptr, word_len);
        completion[head_len + prefix_len + word_len] = '\0';
        linenoiseAddCompletion(lc, completion);
        free(completion);
    }
}

static char *repl_hints(const char *buf, int *color, int *bold) {
    if (!hints_enabled || !completion_ctx) return NULL;

    const char *token = buf;
    for (const char *p = buf; *p != '\0'; p++) {
        if (isspace((unsigned char)*p) || *p == '[' || *p == ']' || *p == '|' ||
            *p == '{' || *p == '}' || *p == '(' || *p == ')') {
            token = p + 1;
        }
    }

    if (*token == '\0') return NULL;

    size_t stem_len = strlen(token);
    tf_obj *sym = tf_obj_new_symbol((char *)token, stem_len);
    tf_word *f = tf_dict_lookup(completion_ctx, sym);
    tf_obj_release(sym);

    const tf_doc_entry *doc = tf_doc_lookup(token);
    if (doc && f) {
        *color = hints_color;
        *bold = 0;
        return doc_stack_hint(doc);
    }

    // Also look for user-defined words
    if (f) {
        *color = hints_color;
        *bold = 0;
        return tf_xstrdup("  user word");
    }

    return NULL;
}

static char *doc_stack_hint(const tf_doc_entry *doc) {
    const char *text = doc->stack_effect[0] ? doc->stack_effect : doc->syntax;
    size_t len = strlen(text);

    char *hint = tf_xmalloc(len + 3);
    hint[0] = ' ';
    hint[1] = ' ';
    memcpy(hint + 2, text, len);
    hint[len + 2] = '\0';

    return hint;
}

static void repl_free_hints(void *ptr) {
    free(ptr);
}

static char *read_repl_line(bool complete) {
    errno = 0;
    return linenoise(complete ? TF_CLR_PROMPT "tf:: " TF_CLR_RESET
                              : TF_CLR_PROMPT "..:: " TF_CLR_RESET);
}

static bool append_text(char **buf, size_t *len, size_t *cap, const char *text) {
    size_t text_len = strlen(text);
    size_t required = *len + text_len + 1;

    if (required > *cap) {
        size_t new_cap = *cap == 0 ? 128 : *cap;
        while (new_cap < required) { new_cap *= 2; }
        *buf = tf_xrealloc(*buf, new_cap);
        *cap = new_cap;
    }

    memcpy(*buf + *len, text, text_len + 1);
    *len += text_len;
    return true;
}

static void reset_state(repl_state *state) {
    memset(state, 0, sizeof(*state));
}

static void feed_state(repl_state *state, const char *text) {
    for (size_t i = 0; text[i] != '\0'; i++) {
        char c = text[i];

        if (state->line_comment) {
            if (c == '\n') state->line_comment = false;
            continue;
        }

        if (state->block_comment) {
            if (c == '*' && text[i + 1] == '/') {
                state->block_comment = false;
                i++;
            }
            continue;
        }

        if (state->in_string) {
            if (state->escape) {
                state->escape = false;
                continue;
            }
            if (c == '\\') {
                state->escape = true;
                continue;
            }
            if (c == '"') state->in_string = false;
            continue;
        }

        if (c == '\\') {
            state->line_comment = true;
            continue;
        }
        if (c == '/' && text[i + 1] == '*') {
            state->block_comment = true;
            i++;
            continue;
        }
        if (c == '(') {
            state->list_depth++;
            continue;
        }
        if (c == ')') {
            if (state->list_depth > 0) state->list_depth--;
            continue;
        }
        if (c == '"') {
            state->in_string = true;
            continue;
        }
        if (c == '[') {
            state->block_depth++;
            continue;
        }
        if (c == ']') {
            if (state->block_depth > 0) state->block_depth--;
            continue;
        }
        if (c == '{') {
            state->brace_depth++;
            continue;
        }
        if (c == '}') {
            if (state->brace_depth > 0) state->brace_depth--;
            continue;
        }
        if (c == '|') {
            if (state->var_depth > 0) {
                state->var_depth--;
            } else {
                state->var_depth++;
            }
            continue;
        }
        if (isspace((unsigned char)c)) {
            continue;
        }
        if (tf_lexer_is_symbol_char((unsigned char)c)) {
            continue;
        }
    }
}

static bool input_complete(const repl_state *state) {
    return !state->in_string && !state->escape && !state->line_comment &&
           !state->block_comment && state->list_depth == 0 &&
           state->block_depth == 0 && state->brace_depth == 0 &&
           state->var_depth == 0;
}

static void init_repl_ui(tf_ctx *ctx) {
    completion_ctx = ctx;
    history_path = get_history_path();

    if (!history_atexit_registered) {
        atexit(save_history_atexit);
        history_atexit_registered = 1;
    }

    tf_dict_set_native(ctx, "hints", hints_toggle);
    tf_dict_set_native(ctx, "trace", trace_toggle);

    linenoiseSetMultiLine(1);
    linenoiseHistorySetMaxLen(256);
    linenoiseSetCompletionCallback(repl_completion);
    linenoiseSetHintsCallback(repl_hints);
    linenoiseSetFreeHintsCallback(repl_free_hints);
    if (history_path) {
        int history_status = linenoiseHistoryLoad(history_path);
        if (history_status == -1 && errno != ENOENT) {
            tf_console_contextf("failed to load REPL history\n");
        }
    }
}

static void free_repl_ui(void) {
    if (history_path) {
        if (linenoiseHistorySave(history_path) == -1) {
            tf_console_contextf("failed to save REPL history\n");
        }
        free(history_path);
        history_path = NULL;
    }
    completion_ctx = NULL;
}

static char *get_history_path(void) {
    const char *home = getenv("HOME");
#ifdef _WIN32
    if (!home) home = getenv("USERPROFILE");
#endif
    if (!home || home[0] == '\0') return NULL;

    const char *name = "/.toy_history";
#ifdef _WIN32
    name = "\\.toy_history";
#endif
    size_t len = strlen(home) + strlen(name) + 1;
    char *path = tf_xmalloc(len);
    snprintf(path, len, "%s%s", home, name);
    return path;
}
