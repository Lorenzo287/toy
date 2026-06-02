#include "tf_repl.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "tf_alloc.h"
#include "tf_console.h"
#include "tf_lexer.h"
#include "tf_obj.h"
#include <linenoise.h>

typedef struct {
    int block_depth;
    int var_depth;
    int colon_depth;
    bool in_string;
    bool escape;
    bool line_comment;
    int paren_comment_depth;
    bool token_active;
    char token_first;
    size_t token_len;
} repl_state;

static tf_ret run_source(tf_ctx *ctx, const char *filename, char *source,
                         bool debug);
static char *read_repl_line(bool complete);
static bool append_text(char **buf, size_t *len, size_t *cap, const char *text);
static void reset_state(repl_state *state);
static void feed_state(repl_state *state, const char *text);
static bool input_complete(const repl_state *state);
static void finish_token(repl_state *state);
static void init_repl_ui(tf_ctx *ctx);
static void free_repl_ui(void);
static void save_history_atexit(void);

static tf_ctx *completion_ctx = NULL;
static char *history_path = NULL;
static bool hints_enabled = true;
static int hints_color = 90;  // Bright black / Gray
static int history_atexit_registered = 0;

static void save_history_atexit(void) {
    if (history_path) { linenoiseHistorySave(history_path); }
}

static char *get_history_path(void);
static void repl_completion(const char *buf, linenoiseCompletions *lc);
static char *repl_hints(const char *buf, int *color, int *bold);
static void repl_free_hints(void *ptr);

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

tf_ret tf_run_repl(tf_ctx *ctx, bool debug) {
    char *source = NULL;
    size_t len = 0;
    size_t cap = 0;
    repl_state state;

    reset_state(&state);
    init_repl_ui(ctx);
    printf("%s=== Toy REPL ===%s\n", tf_console_clr(TF_CLR_PROMPT),
           tf_console_clr(TF_CLR_RESET));
    printf("%sType 'hints' to toggle hints.%s\n", tf_console_clr(TF_CLR_INFO),
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
                len = 0;
                if (source) source[0] = '\0';
                reset_state(&state);
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
            printf("%sok%s\n", tf_console_clr(TF_CLR_OK),
                   tf_console_clr(TF_CLR_RESET));
        }
        fflush(stdout);

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

typedef struct {
    const char *name;
    const char *hint;
} hint_entry;

static const hint_entry native_hints[] = {
    {"dup", " ( x -- x x )"},
    {"drop", " ( x -- )"},
    {"swap", " ( a b -- b a )"},
    {"over", " ( a b -- a b a )"},
    {"rot", " ( a b c -- b c a )"},
    {"swapd", " ( a b c -- b a c )"},
    {"nip", " ( a b -- b )"},
    {"tuck", " ( a b -- b a b )"},
    {"pick", " ( ... n -- ... x )"},
    {"roll", " ( ... n -- ... )"},
    {"empty", " ( -- )"},

    {"+", " ( a b -- a+b )"},
    {"-", " ( a b -- a-b )"},
    {"*", " ( a b -- a*b )"},
    {"/", " ( a b -- a/b )"},
    {"%", " ( a b -- a%b )"},
    {"mod", " ( a b -- a%b )"},
    {"abs", " ( x -- |x| )"},
    {"neg", " ( x -- -x )"},
    {"max", " ( a b -- max )"},
    {"min", " ( a b -- min )"},
    {"sqrt", " ( n -- f )"},
    {"pow", " ( base exp -- f )"},
    {"exp", " ( n -- f )"},
    {"log", " ( n -- f )"},
    {"log10", " ( n -- f )"},
    {"sin", " ( n -- f )"},
    {"cos", " ( n -- f )"},
    {"tan", " ( n -- f )"},
    {"floor", " ( n -- i )"},
    {"ceil", " ( n -- i )"},
    {"round", " ( n -- i )"},
    {"pred", " ( i -- i-1 )"},
    {"succ", " ( i -- i+1 )"},
    {"square", " ( n -- n*n )"},
    {"cube", " ( n -- n*n*n )"},
    {"pi", " ( -- f )"},
    {"e", " ( -- f )"},
    {"tau", " ( -- f )"},

    {"and", " ( a b -- res )"},
    {"or", " ( a b -- res )"},
    {"xor", " ( a b -- res )"},
    {"not", " ( a -- res )"},
    {"shl", " ( n bits -- res )"},
    {"shr", " ( n bits -- res )"},

    {"==", " ( a b -- bool )"},
    {"!=", " ( a b -- bool )"},
    {"<", " ( a b -- bool )"},
    {">", " ( a b -- bool )"},
    {"<=", " ( a b -- bool )"},
    {">=", " ( a b -- bool )"},

    {"if", " ( bool|callable callable -- )"},
    {"ifelse", " ( bool|callable callable callable -- )"},
    {"while", " ( callable callable -- )"},
    {"try", " ( callable callable -- )"},
    {"error", " ( msg -- )"},
    {"infra", " ( list callable -- list )"},
    {"cond", " ( clauses -- )"},
    {"cleave", " ( x callable-list -- ... )"},
    {"construct", " ( x callable-list -- list )"},
    {"replicate", " ( n callable -- list )"},
    {"times", " ( n callable -- )"},
    {"exec", " ( callable -- ... )"},
    {"i", " ( callable -- ... )"},
    {"app2", " ( x y callable -- x' y' )"},
    {"dip", " ( x callable -- x )"},
    {"keep", " ( x callable -- x ... )"},
    {"bi", " ( x callable callable -- ... ... )"},
    {"linrec", " ( callable callable callable callable -- ... )"},
    {"binrec", " ( callable callable callable callable -- ... )"},
    {"genrec", " ( callable callable callable callable -- ... )"},
    {"treerec", " ( tree callable callable -- result )"},

    {"each", " ( list|string callable -- )"},
    {"map", " ( list|string callable -- list|string )"},
    {"fold", " ( init list|string callable -- acc )"},
    {"filter", " ( list|string callable -- list|string )"},
    {"some", " ( list|string callable -- bool )"},
    {"all", " ( list|string callable -- bool )"},
    {"split", " ( list|string callable -- true false ) | ( str sep -- list )"},
    {"merge", " ( seq seq callable -- seq )"},

    {"print", " ( x -- )"},
    {"printf", " ( x -- )"},
    {".", " ( x -- x )"},
    {".s", " ( -- )"},
    {".S", " ( -- )"},
    {"cr", " ( -- )"},
    {"key", " ( -- x )"},
    {"input", " ( -- x )"},
    {"clear", " ( -- )"},
    {"page", " ( -- )"},
    {"load", " ( path -- )"},
    {"readf", " ( path -- str )"},
    {"writef", " ( path content -- )"},
    {"delf", " ( path -- )"},
    {"readl", " ( path -- list )"},
    {"exists?", " ( path -- bool )"},

    {"typeof", " ( x -- str )"},
    {"bool?", " ( x -- bool )"},
    {"int?", " ( x -- bool )"},
    {"float?", " ( x -- bool )"},
    {"str?", " ( x -- bool )"},
    {"symbol?", " ( x -- bool )"},
    {"list?", " ( x -- bool )"},
    {"number?", " ( x -- bool )"},
    {"sequence?", " ( x -- bool )"},
    {"callable?", " ( x -- bool )"},
    {"nan?", " ( x -- bool )"},
    {"inf?", " ( x -- bool )"},
    {"word?", " ( symbol -- bool )"},
    {"var?", " ( symbol -- bool )"},
    {"inf", " ( -- f )"},
    {"nan", " ( -- f )"},
    {"body", " ( 'name -- quot )"},
    {"intern", " ( str -- symbol )"},
    {"name", " ( symbol -- str )"},
    {"words", " ( -- symbols )"},
    {"see", " ( 'name -- str )"},

    {"geth", " ( list idx -- value )"},
    {"seth", " ( list idx value -- list ) | ( str idx char -- str )"},
    {"slice", " ( coll start end -- coll )"},
    {"take", " ( coll n -- coll )"},
    {"dropn", " ( coll n -- coll )"},
    {"len", " ( list|string -- n )"},
    {"first", " ( list|string -- x|char )"},
    {"rest", " ( list|string -- rest )"},
    {"uncons", " ( list|string -- head tail )"},
    {"cons", " ( x list -- list ) | ( char str -- str )"},
    {"append", " ( list x -- list ) | ( str char -- str )"},
    {"concat", " ( list list -- list ) | ( str str -- str )"},
    {"reverse", " ( list|string -- list|string )"},
    {"join", " ( list sep -- str )"},
    {"trim", " ( str -- str )"},
    {"upper", " ( str -- str )"},
    {"lower", " ( str -- str )"},
    {"splitmid", " ( list|string -- left right )"},
    {"range", " ( start end -- list )"},
    {"empty?", " ( list|string -- bool )"},
    {"char?", " ( x -- bool )"},
    {"letter?", " ( x -- bool )"},
    {"digit?", " ( x -- bool )"},
    {"alnum?", " ( x -- bool )"},
    {"space?", " ( x -- bool )"},
    {"upper?", " ( x -- bool )"},
    {"lower?", " ( x -- bool )"},
    {"punct?", " ( x -- bool )"},

    {"rand", " ( -- n )"},
    {"sleep", " ( ms -- )"},
    {"argc", " ( -- n )"},
    {"argv", " ( -- list )"},
    {"env?", " ( key -- bool )"},
    {"getenv", " ( key -- value )"},
    {"setenv", " ( key value -- bool )"},
    {"pwd", " ( -- str )"},
    {"shell", " ( cmd -- output )"},
    {"time", " ( -- epoch )"},
    {"clock", " ( -- ticks )"},
    {"exit", " ( -- )"},
    {"bye", " ( -- )"},

    {"def", " ( 'name block -- )"},
    {":", " ( : name ... ; -- )"},
    {NULL, NULL}};

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
    for (int i = 0; native_hints[i].name; i++) {
        if (strcmp(native_hints[i].name, token) == 0) {
            *color = hints_color;
            *bold = 0;
            return tf_xstrdup(native_hints[i].hint);
        }
    }

    // Also look for user-defined words
    tf_obj *sym = tf_obj_new_symbol((char *)token, stem_len);
    tf_word *f = tf_dict_lookup(completion_ctx, sym);
    tf_obj_release(sym);

    if (f) {
        *color = hints_color;
        *bold = 0;
        return tf_xstrdup(" ( user word )");
    }

    return NULL;
}

static void repl_free_hints(void *ptr) {
    free(ptr);
}

static char *read_repl_line(bool complete) {
    return linenoise(complete ? TF_CLR_PROMPT "tf> " TF_CLR_RESET
                              : TF_CLR_PROMPT "..> " TF_CLR_RESET);
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

        if (state->paren_comment_depth > 0) {
            if (c == '(') {
                state->paren_comment_depth++;
            } else if (c == ')') {
                state->paren_comment_depth--;
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
            finish_token(state);
            state->line_comment = true;
            continue;
        }
        if (c == '(') {
            finish_token(state);
            state->paren_comment_depth = 1;
            continue;
        }
        if (c == '"') {
            finish_token(state);
            state->in_string = true;
            continue;
        }
        if (c == '[') {
            finish_token(state);
            state->block_depth++;
            continue;
        }
        if (c == ']') {
            finish_token(state);
            if (state->block_depth > 0) state->block_depth--;
            continue;
        }
        if (c == '|') {
            finish_token(state);
            if (state->var_depth > 0) {
                state->var_depth--;
            } else {
                state->var_depth++;
            }
            continue;
        }
        if (c == ':' || c == ';') {
            finish_token(state);
            state->token_active = true;
            state->token_first = c;
            state->token_len = 1;
            finish_token(state);
            continue;
        }
        if (isspace((unsigned char)c)) {
            finish_token(state);
            continue;
        }
        if (tf_lexer_is_symbol_char((unsigned char)c)) {
            if (!state->token_active) {
                state->token_active = true;
                state->token_first = c;
                state->token_len = 1;
            } else {
                state->token_len++;
            }
            continue;
        }

        finish_token(state);
    }
}

static bool input_complete(const repl_state *state) {
    return !state->in_string && !state->escape && !state->line_comment &&
           state->paren_comment_depth == 0 && state->block_depth == 0 &&
           state->var_depth == 0 && state->colon_depth == 0;
}

static void init_repl_ui(tf_ctx *ctx) {
    completion_ctx = ctx;
    history_path = get_history_path();

    if (!history_atexit_registered) {
        atexit(save_history_atexit);
        history_atexit_registered = 1;
    }

    tf_dict_set_native(ctx, "hints", hints_toggle);

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

static void finish_token(repl_state *state) {
    if (!state->token_active) return;

    if (state->token_len == 1) {
        if (state->token_first == ':') {
            state->colon_depth++;
        } else if (state->token_first == ';' && state->colon_depth > 0) {
            state->colon_depth--;
        }
    }

    state->token_active = false;
    state->token_first = '\0';
    state->token_len = 0;
}
