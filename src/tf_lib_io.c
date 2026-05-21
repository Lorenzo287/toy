#include "tf_lib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tf_obj.h"
#include "tf_exec.h"
#include "tf_alloc.h"

tf_ret tf_printf(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    print_value(o);
    release_obj(o);
    return TF_OK;
}

tf_ret tf_print(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_pop(ctx);
    print_value(o);
    printf("\n");
    release_obj(o);
    return TF_OK;
}

tf_ret tf_dot(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *o = stack_peek(ctx, 0);
    print_value(o);
    printf("\n");
    return TF_OK;
}

tf_ret tf_cr(tf_ctx *ctx) {
    printf("\n");
    (void)ctx;
    return TF_OK;
}

tf_ret tf_stack(tf_ctx *ctx) {
    size_t len = stack_len(ctx);
    printf("<%zu> ", len);
    for (size_t i = 0; i < len; i++) {
        print_value(stack_peek(ctx, len - 1 - i));
        printf(" ");
    }
    printf("\n");
    return TF_OK;
}

tf_ret tf_key(tf_ctx *ctx) {
    int c = getchar();
    if (c == EOF) return TF_ERR;
    stack_push(ctx, create_int_obj(c));
    return TF_OK;
}

#define MAX_BUF_LEN 1023
tf_ret tf_input(tf_ctx *ctx) {
    char buf[MAX_BUF_LEN + 1];
    if (!fgets(buf, sizeof buf, stdin)) return TF_ERR;
    buf[strcspn(buf, "\n")] = '\0';
    stack_push(ctx, create_string_obj(buf, strlen(buf)));
    return TF_OK;
}

tf_ret tf_readf(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        release_obj(path);
        return TF_ERR;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buf = xmalloc((size_t)size + 1);
    size_t n_read = fread(buf, 1, (size_t)size, fp);
    buf[n_read] = '\0';
    fclose(fp);

    stack_push(ctx, create_string_obj(buf, n_read));
    free(buf);
    release_obj(path);
    return TF_OK;
}

tf_ret tf_writef(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *content = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    tf_obj *path = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!content || !path) {
        if (content) release_obj(content);
        if (path) release_obj(path);
        return TF_ERR;
    }

    FILE *fp = fopen(path->str.ptr, "wb");
    if (!fp) {
        release_obj(content);
        release_obj(path);
        return TF_ERR;
    }

    size_t n_written = fwrite(content->str.ptr, 1, content->str.len, fp);
    fclose(fp);

    tf_ret res = (n_written == content->str.len) ? TF_OK : TF_ERR;
    release_obj(content);
    release_obj(path);
    return res;
}

tf_ret tf_delf(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    int res = remove(path->str.ptr);
    release_obj(path);
    return (res == 0) ? TF_OK : TF_ERR;
}

tf_ret tf_readl(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        release_obj(path);
        return TF_ERR;
    }

    tf_obj *lines = init_list_obj();
    char buf[MAX_BUF_LEN + 1];
    while (fgets(buf, sizeof buf, fp)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        push_obj(lines, create_string_obj(buf, strlen(buf)));
    }
    fclose(fp);

    stack_push(ctx, lines);
    release_obj(path);
    return TF_OK;
}

tf_ret tf_exists_q(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!path) return TF_ERR;

    FILE *fp = fopen(path->str.ptr, "rb");
    bool exists = (fp != NULL);
    if (fp) fclose(fp);

    stack_push(ctx, create_bool_obj(exists));
    release_obj(path);
    return TF_OK;
}
