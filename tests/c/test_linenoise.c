#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include "../../deps/linenoise/linenoise_win.c"
#else
#include "../../deps/linenoise/linenoise.c"
#endif

#define CHECK(condition, message)                                             \
    do {                                                                      \
        if (!(condition)) {                                                   \
            fprintf(stderr,"linenoise check failed: %s\n",message);         \
            return 1;                                                         \
        }                                                                     \
    } while (0)

static int set_columns_override(const char *value) {
#ifdef _WIN32
    return _putenv_s("LINENOISE_COLS",value) == 0;
#else
    return setenv("LINENOISE_COLS",value,1) == 0;
#endif
}

static int clear_columns_override(void) {
#ifdef _WIN32
    return _putenv_s("LINENOISE_COLS","") == 0;
#else
    return unsetenv("LINENOISE_COLS") == 0;
#endif
}

static void init_state(struct linenoiseState *state, char *buf, size_t buflen,
                       const char *prompt, size_t cols, int ofd) {
    memset(state,0,sizeof(*state));
    state->ifd = -1;
    state->ofd = ofd;
    state->buf = buf;
    state->buflen = buflen - 1;
    state->prompt = prompt;
    state->plen = strlen(prompt);
    state->cols = cols;
    state->oldrpos = 1;
    buf[0] = '\0';
}

static void long_completion(const char *buf, linenoiseCompletions *lc) {
    (void)buf;
    linenoiseAddCompletion(lc,"abcdef");
}

static void clear_history(void) {
    int i;
    for (i = 0; i < history_len; i++) free(history[i]);
    free(history);
    history = NULL;
    history_len = 0;
    history_max_len = LINENOISE_DEFAULT_HISTORY_MAX_LEN;
}

int main(void) {
    struct linenoiseState state;
    char buf[5];

    CHECK(set_columns_override("0"),"set zero column override");
    CHECK(getColumns(-1,-1) == LINENOISE_DEFAULT_COLUMNS,
          "zero columns fall back safely");
    CHECK(set_columns_override("not-a-number"),"set invalid column override");
    CHECK(getColumns(-1,-1) == LINENOISE_DEFAULT_COLUMNS,
          "invalid columns fall back safely");
    CHECK(set_columns_override("17"),"set valid column override");
    CHECK(getColumns(-1,-1) == 17,"valid columns are preserved");

    init_state(&state,buf,sizeof(buf),"prompt",1,-1);
    refreshSingleLine(&state,REFRESH_WRITE);

    CHECK(set_columns_override("80"),"set completion test columns");
    init_state(&state,buf,sizeof(buf),"",80,-1);
    state.in_completion = 1;
    state.completion_idx = 0;
    completionCallback = long_completion;
    CHECK(completeLine(&state,'!') == '!',"completion returns typed byte");
    CHECK(state.len == 4 && state.pos == 4,"completion length is clamped");
    CHECK(strcmp(buf,"abcd") == 0,"completion stores a terminated prefix");

    state.in_completion = 1;
    state.completion_idx = 0;
    refreshLineWithCompletion(&state,NULL,REFRESH_WRITE);

    init_state(&state,buf,sizeof(buf),"",80,-1);
    CHECK(linenoiseEditInsert(&state,"x",1) == -1,
          "insert reports terminal write errors");

    clear_history();
    CHECK(linenoiseHistoryAdd("one"),"add real history entry");
    state.history_temp_active = 0;
    addHistoryPlaceholder(&state);
    CHECK(state.history_temp_active && history_len == 2,
          "add session-owned history entry");
    discardHistoryPlaceholder(&state);
    CHECK(history_len == 1 && strcmp(history[0],"one") == 0,
          "temporary history entry is discarded");

#ifdef _WIN32
    {
        char encoded[4];
        CHECK(utf8EncodeChar(0x1f600,encoded) == 4,
              "supplementary code point encodes to four bytes");
        CHECK((unsigned char)encoded[0] == 0xf0 &&
              (unsigned char)encoded[1] == 0x9f &&
              (unsigned char)encoded[2] == 0x98 &&
              (unsigned char)encoded[3] == 0x80,
              "supplementary code point uses UTF-8");
    }
#endif

    completionCallback = NULL;
    clear_history();
    CHECK(clear_columns_override(),"clear column override");
    return 0;
}
