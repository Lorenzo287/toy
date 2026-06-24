#include "tf_lib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "tf_obj.h"
#include "tf_exec.h"
#include "tf_alloc.h"
#include "tf_lexer.h"

static int read_line_dynamic(FILE *fp, char **buf, size_t *cap,
                             size_t *line_len) {
    *line_len = 0;
    int c = 0;
    while ((c = fgetc(fp)) != EOF && c != '\n') {
        size_t needed = *line_len + 2;
        if (needed > *cap) {
            size_t new_cap = *cap == 0 ? 128 : *cap;
            while (new_cap < needed) new_cap *= 2;
            *buf = tf_xrealloc(*buf, new_cap);
            *cap = new_cap;
        }
        (*buf)[(*line_len)++] = (char)c;
    }

    if (c == EOF && ferror(fp)) return -1;
    if (c == EOF && *line_len == 0) return 0;
    if (*line_len > 0 && (*buf)[*line_len - 1] == '\r') (*line_len)--;
    if (*buf) (*buf)[*line_len] = '\0';
    return 1;
}

tf_ret tf_printf(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_peek(ctx, 0);

    if (o->type != TF_OBJ_TYPE_STR) {
        o = tf_stack_pop(ctx);
        tf_obj_print_value(o);
        tf_obj_release(o);
        return TF_OK;
    }

    const char *fmt = o->str.ptr;
    size_t fmt_len = o->str.len;

    // Count placeholders and check for escapes
    size_t count = 0;
    bool has_format = false;
    for (size_t i = 0; i < fmt_len; i++) {
        if (fmt[i] == '{') {
            if (i + 1 < fmt_len && fmt[i + 1] == '}') {
                count++;
                has_format = true;
                i++;
            } else if (i + 1 < fmt_len && fmt[i + 1] == '{') {
                has_format = true;
                i++;
            }
        } else if (fmt[i] == '}') {
            if (i + 1 < fmt_len && fmt[i + 1] == '}') {
                has_format = true;
                i++;
            }
        }
    }

    if (!has_format) {
        o = tf_stack_pop(ctx);
        tf_obj_print_value(o);
        tf_obj_release(o);
        return TF_OK;
    }

    if (tf_stack_len(ctx) - 1 < count) {
        tf_ctx_runtime_errorf(ctx,
                              "'printf' expected %zu format argument%s, found %zu\n",
                              count, count == 1 ? "" : "s",
                              tf_stack_len(ctx) - 1);
        return TF_ERR;
    }

    o = tf_stack_pop(ctx);
    size_t arg_idx = 0;
    for (size_t i = 0; i < fmt_len; i++) {
        if (fmt[i] == '{') {
            if (i + 1 < fmt_len && fmt[i + 1] == '}') {
                tf_obj_print_value(tf_stack_peek(ctx, count - 1 - arg_idx));
                arg_idx++;
                i++;
            } else if (i + 1 < fmt_len && fmt[i + 1] == '{') {
                putchar('{');
                i++;
            } else {
                putchar('{');
            }
        } else if (fmt[i] == '}') {
            if (i + 1 < fmt_len && fmt[i + 1] == '}') {
                putchar('}');
                i++;
            } else {
                putchar('}');
            }
        } else {
            putchar(fmt[i]);
        }
    }

    for (size_t i = 0; i < count; i++) {
        tf_obj *arg = tf_stack_pop(ctx);
        tf_obj_release(arg);
    }
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_print(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_pop(ctx);
    tf_obj_print_value(o);
    putchar('\n');
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_dot(tf_ctx *ctx) {
    if (!tf_ctx_require_stack(ctx, 1)) return TF_ERR;
    tf_obj *o = tf_stack_peek(ctx, 0);
    tf_obj_print_value(o);
    printf("\n");
    return TF_OK;
}

tf_ret tf_stack(tf_ctx *ctx) {
    size_t len = tf_stack_len(ctx);
    printf("<%zu> ", len);
    for (size_t i = 0; i < len; i++) {
        tf_obj_print_value(tf_stack_peek(ctx, len - 1 - i));
        printf(" ");
    }
    printf("\n");
    return TF_OK;
}

tf_ret tf_stack_source(tf_ctx *ctx) {
    size_t len = tf_stack_len(ctx);
    printf("<%zu> ", len);
    for (size_t i = 0; i < len; i++) {
        tf_obj_print_source(tf_stack_peek(ctx, len - 1 - i));
        printf(" ");
    }
    printf("\n");
    return TF_OK;
}

tf_ret tf_clear(tf_ctx *ctx) {
    (void)ctx;
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(out, &info)) {
        DWORD cell_count = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
        COORD home = {0, 0};
        DWORD written = 0;

        if (FillConsoleOutputCharacterA(out, ' ', cell_count, home, &written) &&
            FillConsoleOutputAttribute(out, info.wAttributes, cell_count, home,
                                       &written) &&
            SetConsoleCursorPosition(out, home)) {
            return TF_OK;
        }
    }
#endif

    printf("\x1b[H\x1b[2J");
    fflush(stdout);
    return TF_OK;
}

tf_ret tf_key(tf_ctx *ctx) {
    int c = getchar();
    if (c == EOF) {
        tf_ctx_runtime_errorf(ctx, "'read-key' failed to read input\n");
        return TF_ERR;
    }
    unsigned char byte = (unsigned char)c;
    tf_stack_push(ctx, tf_obj_new_string((const char *)&byte, 1));
    return TF_OK;
}

tf_ret tf_input(tf_ctx *ctx) {
    char *buf = NULL;
    size_t cap = 0;
    size_t len = 0;
    int status = read_line_dynamic(stdin, &buf, &cap, &len);
    if (status <= 0) {
        free(buf);
        tf_ctx_runtime_errorf(ctx, "'read-line' failed to read input\n");
        return TF_ERR;
    }
    tf_stack_push(ctx, tf_obj_new_string_take(buf, len));
    return TF_OK;
}

tf_ret tf_load(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;

    tf_obj *path = tf_stack_pop(ctx);
    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        tf_ctx_runtime_errorf(ctx, "failed to load '%s'\n", path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        tf_ctx_runtime_errorf(ctx, "'load' failed to seek '%s'\n", path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }

    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        tf_ctx_runtime_errorf(ctx, "'load' failed to read size for '%s'\n",
                              path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }
    rewind(fp);

    char *source = tf_xmalloc((size_t)size + 1);
    size_t n_read = fread(source, 1, (size_t)size, fp);
    if (n_read != (size_t)size) {
        fclose(fp);
        free(source);
        tf_ctx_runtime_errorf(ctx, "'load' failed to read all bytes from '%s'\n",
                              path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }
    source[n_read] = '\0';
    fclose(fp);

    tf_obj *prg = tf_lexer_parse(path->str.ptr, source);
    free(source);
    if (!prg) {
        tf_obj_release(path);
        return TF_ERR;
    }

    tf_frame_push_program(ctx, prg);
    tf_obj_release(prg);
    tf_obj_release(path);
    return TF_OK;
}

tf_ret tf_readf(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        tf_ctx_runtime_errorf(ctx, "'read-file' failed to open '%s'\n", path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        tf_ctx_runtime_errorf(ctx, "'read-file' failed to seek '%s'\n", path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        tf_ctx_runtime_errorf(ctx, "'read-file' failed to read size for '%s'\n",
                              path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }
    rewind(fp);

    char *buf = tf_xmalloc((size_t)size + 1);
    size_t n_read = fread(buf, 1, (size_t)size, fp);
    if (n_read != (size_t)size) {
        fclose(fp);
        free(buf);
        tf_ctx_runtime_errorf(ctx, "'read-file' failed to read all bytes from '%s'\n",
                              path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }
    buf[n_read] = '\0';
    fclose(fp);

    tf_stack_push(ctx, tf_obj_new_string_take(buf, n_read));
    tf_obj_release(path);
    return TF_OK;
}

tf_ret tf_writef(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_STR) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }
    tf_obj *content = tf_stack_pop(ctx);
    tf_obj *path = tf_stack_pop(ctx);

    FILE *fp = fopen(path->str.ptr, "wb");
    if (!fp) {
        tf_ctx_runtime_errorf(ctx, "'write-file' failed to open '%s'\n", path->str.ptr);
        tf_obj_release(content);
        tf_obj_release(path);
        return TF_ERR;
    }

    size_t n_written = fwrite(content->str.ptr, 1, content->str.len, fp);
    fclose(fp);

    tf_ret res = (n_written == content->str.len) ? TF_OK : TF_ERR;
    if (res == TF_ERR) {
        tf_ctx_runtime_errorf(ctx, "'write-file' failed to write all bytes to '%s'\n",
                              path->str.ptr);
    }
    tf_obj_release(content);
    tf_obj_release(path);
    return res;
}

tf_ret tf_delf(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

    int res = remove(path->str.ptr);
    if (res != 0) {
        tf_ctx_runtime_errorf(ctx, "'delete-file' failed to delete '%s'\n", path->str.ptr);
    }
    tf_obj_release(path);
    return (res == 0) ? TF_OK : TF_ERR;
}

tf_ret tf_readl(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        tf_ctx_runtime_errorf(ctx, "'read-lines' failed to open '%s'\n", path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }

    tf_obj *lines = tf_obj_new_vector();
    char *buf = NULL;
    size_t cap = 0;
    size_t line_len = 0;
    int status = 0;
    while ((status = read_line_dynamic(fp, &buf, &cap, &line_len)) > 0) {
        tf_vector_push(lines, tf_obj_new_string(buf, line_len));
    }
    free(buf);
    if (status < 0) {
        fclose(fp);
        tf_ctx_runtime_errorf(ctx, "'read-lines' failed while reading '%s'\n",
                              path->str.ptr);
        tf_obj_release(lines);
        tf_obj_release(path);
        return TF_ERR;
    }
    fclose(fp);

    tf_stack_push(ctx, lines);
    tf_obj_release(path);
    return TF_OK;
}

tf_ret tf_exists_q(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);

#ifdef _WIN32
    struct _stat info;
    bool exists = _stat(path->str.ptr, &info) == 0;
#else
    struct stat info;
    bool exists = stat(path->str.ptr, &info) == 0;
#endif

    tf_stack_push(ctx, tf_obj_new_bool(exists));
    tf_obj_release(path);
    return TF_OK;
}
