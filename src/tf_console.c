#include "tf_console.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

static bool console_color_enabled = false;

static void console_vmessage(const char *label, const char *color,
                             const char *fmt, va_list args) {
    fprintf(stderr, "%s%s:%s ", tf_console_clr(color), label,
            tf_console_clr(TF_CLR_RESET));
    vfprintf(stderr, fmt, args);
}

#ifdef _WIN32
static bool enable_vt(HANDLE handle) {
    if (handle == INVALID_HANDLE_VALUE || handle == NULL) return false;

    DWORD mode = 0;
    if (!GetConsoleMode(handle, &mode)) return false;
    if (!SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        return false;
    }
    return true;
}
#endif

void tf_console_init(void) {
#ifdef _WIN32
    bool out_ok = enable_vt(GetStdHandle(STD_OUTPUT_HANDLE));
    bool err_ok = enable_vt(GetStdHandle(STD_ERROR_HANDLE));
    console_color_enabled = out_ok || err_ok;
#else
    console_color_enabled = isatty(fileno(stdout)) || isatty(fileno(stderr));
#endif
}

bool tf_console_use_color(void) {
    return console_color_enabled;
}

const char *tf_console_clr(const char *code) {
    return console_color_enabled ? code : "";
}

size_t tf_console_width(void) {
    const char *cols_env = getenv("LINENOISE_COLS");
    if (cols_env) {
        char *end = NULL;
        long cols = strtol(cols_env, &end, 10);
        if (end != cols_env && *end == '\0' && cols > 0) return (size_t)cols;
    }

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        return (size_t)(info.srWindow.Right - info.srWindow.Left + 1);
    }
#else
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
        return size.ws_col;
    }
#endif
    return 80;
}

void tf_console_lexer_errorf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    console_vmessage("parsing error", TF_CLR_ERR, fmt, args);
    va_end(args);
}

void tf_console_interruptf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    console_vmessage("interrupt", TF_CLR_WARN, fmt, args);
    va_end(args);
}

void tf_console_contextf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    console_vmessage("context", TF_CLR_ERR, fmt, args);
    va_end(args);
}
