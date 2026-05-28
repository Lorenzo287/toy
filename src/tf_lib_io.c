#include "tf_lib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "tf_obj.h"
#include "tf_exec.h"
#include "tf_alloc.h"
#include "tf_console.h"
#include "tf_lexer.h"

tf_ret tf_printf(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 1) return TF_ERR;
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
        return TF_ERR;
    }

    o = tf_stack_pop(ctx);
    tf_obj **args = NULL;
    if (count > 0) {
        args = tf_xmalloc(count * sizeof(tf_obj *));
        for (size_t i = 0; i < count; i++) {
            args[count - 1 - i] = tf_stack_pop(ctx);
        }
    }

    size_t arg_idx = 0;
    for (size_t i = 0; i < fmt_len; i++) {
        if (fmt[i] == '{') {
            if (i + 1 < fmt_len && fmt[i + 1] == '}') {
                tf_obj_print_value(args[arg_idx++]);
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

    if (args) {
        for (size_t i = 0; i < count; i++) {
            tf_obj_release(args[i]);
        }
        free(args);
    }
    tf_obj_release(o);
    return TF_OK;
}

tf_ret tf_print(tf_ctx *ctx) {
    tf_ret res = tf_printf(ctx);
    if (res == TF_OK) printf("\n");
    return res;
}

tf_ret tf_dot(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = tf_stack_peek(ctx, 0);
    tf_obj_print_value(o);
    printf("\n");
    return TF_OK;
}

tf_ret tf_cr(tf_ctx *ctx) {
    printf("\n");
    (void)ctx;
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
    if (c == EOF) return TF_ERR;
    tf_stack_push(ctx, tf_obj_new_int(c));
    return TF_OK;
}

#define MAX_BUF_LEN 1023
tf_ret tf_input(tf_ctx *ctx) {
    char buf[MAX_BUF_LEN + 1];
    if (!fgets(buf, sizeof buf, stdin)) return TF_ERR;
    buf[strcspn(buf, "\n")] = '\0';
    tf_stack_push(ctx, tf_obj_new_string(buf, strlen(buf)));
    return TF_OK;
}

tf_ret tf_load(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = tf_stack_peek(ctx, 0);
    if (path->type != TF_OBJ_TYPE_STR) return TF_ERR;

    path = tf_stack_pop(ctx);
    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        tf_console_runtime_errorf("failed to load '%s'\n", path->str.ptr);
        tf_obj_release(path);
        return TF_ERR;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        tf_obj_release(path);
        return TF_ERR;
    }

    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        tf_obj_release(path);
        return TF_ERR;
    }
    rewind(fp);

    char *source = tf_xmalloc((size_t)size + 1);
    size_t n_read = fread(source, 1, (size_t)size, fp);
    source[n_read] = '\0';
    fclose(fp);

    tf_obj *prg = tf_lexer_parse(source);
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
    if (tf_stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        tf_obj_release(path);
        return TF_ERR;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        tf_obj_release(path);
        return TF_ERR;
    }
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        tf_obj_release(path);
        return TF_ERR;
    }
    rewind(fp);

    char *buf = tf_xmalloc((size_t)size + 1);
    size_t n_read = fread(buf, 1, (size_t)size, fp);
    buf[n_read] = '\0';
    fclose(fp);

    tf_stack_push(ctx, tf_obj_new_string(buf, n_read));
    free(buf);
    tf_obj_release(path);
    return TF_OK;
}

tf_ret tf_writef(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 2) return TF_ERR;
    tf_obj *content = tf_stack_peek(ctx, 0);
    tf_obj *path = tf_stack_peek(ctx, 1);
    if (content->type != TF_OBJ_TYPE_STR || path->type != TF_OBJ_TYPE_STR) {
        return TF_ERR;
    }
    content = tf_stack_pop(ctx);
    path = tf_stack_pop(ctx);

    FILE *fp = fopen(path->str.ptr, "wb");
    if (!fp) {
        tf_obj_release(content);
        tf_obj_release(path);
        return TF_ERR;
    }

    size_t n_written = fwrite(content->str.ptr, 1, content->str.len, fp);
    fclose(fp);

    tf_ret res = (n_written == content->str.len) ? TF_OK : TF_ERR;
    tf_obj_release(content);
    tf_obj_release(path);
    return res;
}

tf_ret tf_delf(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    int res = remove(path->str.ptr);
    tf_obj_release(path);
    return (res == 0) ? TF_OK : TF_ERR;
}

tf_ret tf_readl(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        tf_obj_release(path);
        return TF_ERR;
    }

    tf_obj *lines = tf_obj_new_list();
    char buf[MAX_BUF_LEN + 1];
    while (fgets(buf, sizeof buf, fp)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        tf_list_push(lines, tf_obj_new_string(buf, strlen(buf)));
    }
    fclose(fp);

    tf_stack_push(ctx, lines);
    tf_obj_release(path);
    return TF_OK;
}

tf_ret tf_exists_q(tf_ctx *ctx) {
    if (tf_stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = tf_stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    FILE *fp = fopen(path->str.ptr, "rb");
    bool exists = (fp != NULL);
    if (fp) fclose(fp);

    tf_stack_push(ctx, tf_obj_new_bool(exists));
    tf_obj_release(path);
    return TF_OK;
}
