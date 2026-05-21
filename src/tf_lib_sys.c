#include "tf_lib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "tf_obj.h"
#include "tf_alloc.h"
#include "tf_exec.h"
#include "tf_console.h"
#include "tf_lexer.h"

tf_ret tf_rand(tf_ctx *ctx) {
    stack_push(ctx, create_int_obj(rand()));
    return TF_OK;
}

tf_ret tf_sleep(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *ms_obj = stack_peek(ctx, 0);
    if (ms_obj->type != TF_OBJ_TYPE_INT) return TF_ERR;
    if (ms_obj->i < 0) return TF_ERR;
    ms_obj = stack_pop(ctx);
#ifdef _WIN32
    Sleep(ms_obj->i);
#else
    struct timespec req = {.tv_sec = ms_obj->i / 1000,
                           .tv_nsec = (long)(ms_obj->i % 1000) * 1000000L};
    nanosleep(&req, NULL);
#endif
    release_obj(ms_obj);
    return TF_OK;
}

tf_ret tf_time(tf_ctx *ctx) {
    stack_push(ctx, create_int_obj((int)time(NULL)));
    return TF_OK;
}

tf_ret tf_clock(tf_ctx *ctx) {
    stack_push(ctx, create_int_obj((int)clock()));
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

static int tf_func_name_cmp(const void *a, const void *b) {
    tf_func *const *fa = a;
    tf_func *const *fb = b;
    return compare_string_obj((*fa)->name, (*fb)->name);
}

tf_ret tf_words(tf_ctx *ctx) {
    size_t count = ctx->functions.count;
    if (count == 0) {
        printf("\n");
        return TF_OK;
    }
    tf_func **funcs = xmalloc(sizeof(tf_func *) * count);
    size_t j = 0;
    for (size_t i = 0; i < ctx->functions.capacity; i++) {
        tf_func *f = ctx->functions.buckets[i];
        if (f != NULL) funcs[j++] = f;
    }
    qsort(funcs, j, sizeof(tf_func *), tf_func_name_cmp);
    for (size_t i = 0; i < j; i++) {
        printf("%s", funcs[i]->name->str.ptr);
        printf(i + 1 < j ? " " : "\n");
    }
    free(funcs);
    return TF_OK;
}

tf_ret tf_see(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *name = stack_peek(ctx, 0);
    if (name->type != TF_OBJ_TYPE_SYMBOL) return TF_ERR;

    name = stack_pop(ctx);

    tf_func *func = get_func(ctx, name);
    if (!func) {
        release_obj(name);
        return TF_ERR;
    }

    if (func->type == TF_FUNC_TYPE_NATIVE) {
        printf("%s is a native word\n", func->name->str.ptr);
        release_obj(name);
        return TF_OK;
    }

    printf("'%s [ ", func->name->str.ptr);
    for (size_t i = 0; i < func->user_impl->list.len; i++) {
        print_source_obj(func->user_impl->list.elem[i]);
        if (i + 1 < func->user_impl->list.len) printf(" ");
    }
    printf(" ] def\n");
    release_obj(name);
    return TF_OK;
}

tf_ret tf_load_r(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *path = stack_peek(ctx, 0);
    if (path->type != TF_OBJ_TYPE_STR) return TF_ERR;

    path = stack_pop(ctx);
    FILE *fp = fopen(path->str.ptr, "rb");
    if (!fp) {
        tf_console_runtime_errorf("failed to load '%s'\n", path->str.ptr);
        release_obj(path);
        return TF_ERR;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        release_obj(path);
        return TF_ERR;
    }

    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        release_obj(path);
        return TF_ERR;
    }
    rewind(fp);

    char *source = xmalloc((size_t)size + 1);
    size_t n_read = fread(source, 1, (size_t)size, fp);
    source[n_read] = '\0';
    fclose(fp);

    tf_obj *prg = lexer(source);
    free(source);
    if (!prg) {
        release_obj(path);
        return TF_ERR;
    }

    tf_ret result = exec(ctx, prg);
    release_obj(prg);
    release_obj(path);
    return result;
}

tf_ret tf_exit(tf_ctx *ctx) {
    (void)ctx;
    exit(0);
    return TF_OK;
}

tf_ret tf_colon(tf_ctx *ctx) {
    if (ctx->cstack_len == 0) return TF_ERR;
    tf_frame *f = &ctx->call_stack[ctx->cstack_len - 1];

    if (f->pc >= f->prg->list.len) return TF_ERR;
    tf_obj *func_name = f->prg->list.elem[f->pc];
    if (func_name->type != TF_OBJ_TYPE_SYMBOL) return TF_ERR;

    tf_obj *body = init_list_obj();
    f->pc++;
    while (f->pc < f->prg->list.len) {
        tf_obj *o = f->prg->list.elem[f->pc];
        if (o->type == TF_OBJ_TYPE_SYMBOL && strcmp(o->str.ptr, ";") == 0) break;
        push_obj(body, o);
        retain_obj(o);
        f->pc++;
    }

    if (f->pc >= f->prg->list.len) {
        release_obj(body);
        return TF_ERR;
    }

    set_user_func(ctx, func_name, body);
    release_obj(body);
    f->pc++;
    return TF_OK;
}

tf_ret tf_def(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *body = stack_peek(ctx, 0);
    tf_obj *func_name = stack_peek(ctx, 1);
    if (body->type != TF_OBJ_TYPE_LIST || func_name->type != TF_OBJ_TYPE_SYMBOL) {
        return TF_ERR;
    }

    body = stack_pop(ctx);
    func_name = stack_pop(ctx);

    set_user_func(ctx, func_name, body);

    release_obj(body);
    release_obj(func_name);
    return TF_OK;
}
