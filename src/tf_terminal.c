#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "tf_terminal.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

static bool terminal_color_enabled = false;

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

void tf_terminal_init(void) {
#ifdef _WIN32
    bool out_ok = enable_vt(GetStdHandle(STD_OUTPUT_HANDLE));
    bool err_ok = enable_vt(GetStdHandle(STD_ERROR_HANDLE));
    terminal_color_enabled = out_ok || err_ok;
#else
    terminal_color_enabled = isatty(fileno(stdout)) || isatty(fileno(stderr));
#endif
}

bool tf_terminal_use_color(void) {
    return terminal_color_enabled;
}

const char *tf_terminal_color(const char *code) {
    return terminal_color_enabled ? code : "";
}

size_t tf_terminal_width(void) {
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
