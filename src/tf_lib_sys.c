#include "tf_lib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define SETENV _putenv_s
#else
#include <unistd.h>
#define SETENV setenv
#endif
#include "tf_obj.h"
#include "tf_alloc.h"
#include "tf_exec.h"

tf_ret tf_setenv(tf_ctx *ctx) {
    if (stack_len(ctx) < 2) return TF_ERR;
    tf_obj *val = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    tf_obj *name = stack_pop_type(ctx, TF_OBJ_TYPE_STR);
    if (!val || !name) {
        if (val) release_obj(val);
        if (name) release_obj(name);
        return TF_ERR;
    }

#ifdef _WIN32
    int res = SETENV(name->str.ptr, val->str.ptr);
#else
    int res = SETENV(name->str.ptr, val->str.ptr, 1);
#endif

    stack_push(ctx, create_bool_obj(res == 0));
    release_obj(val);
    release_obj(name);
    return TF_OK;
}

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

tf_ret tf_shell(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *cmd_obj = stack_peek(ctx, 0);
    if (cmd_obj->type != TF_OBJ_TYPE_STR) return TF_ERR;

    cmd_obj = stack_pop(ctx);
    FILE *fp = popen(cmd_obj->str.ptr, "r");
    if (!fp) {
        release_obj(cmd_obj);
        return TF_ERR;
    }

    char *output = NULL;
    size_t total_size = 0;
    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), fp)) {
        size_t len = strlen(buffer);
        output = xrealloc(output, total_size + len + 1);
        memcpy(output + total_size, buffer, len);
        total_size += len;
        output[total_size] = '\0';
    }

    int status = pclose(fp);
    if (output) {
        stack_push(ctx, create_string_obj(output, total_size));
        free(output);
    } else {
        stack_push(ctx, init_list_obj()); // Push empty list as a "not found" indicator
        // stack_push(ctx, create_string_obj("", 0));
    }
    
    // We could also push the status code, but for now let's just return the output
    (void)status; 

    release_obj(cmd_obj);
    return TF_OK;
}

tf_ret tf_argc(tf_ctx *ctx) {
    stack_push(ctx, create_int_obj(ctx->argc > 0 ? ctx->argc : 0));
    return TF_OK;
}

tf_ret tf_argv(tf_ctx *ctx) {
    tf_obj *list = init_list_obj();
    for (int i = 0; i < ctx->argc; i++) {
        tf_obj *str = create_string_obj(ctx->argv[i], strlen(ctx->argv[i]));
        push_obj(list, str);
        release_obj(str);
    }
    stack_push(ctx, list);
    return TF_OK;
}

tf_ret tf_pwd(tf_ctx *ctx) {
    char buf[1024];
#ifdef _WIN32
    if (_getcwd(buf, sizeof(buf))) {
#else
    if (getcwd(buf, sizeof(buf))) {
#endif
        stack_push(ctx, create_string_obj(buf, strlen(buf)));
        return TF_OK;
    }
    return TF_ERR;
}

tf_ret tf_getenv(tf_ctx *ctx) {
    if (stack_len(ctx) < 1) return TF_ERR;
    tf_obj *key = stack_peek(ctx, 0);
    if (key->type != TF_OBJ_TYPE_STR) return TF_ERR;

    key = stack_pop(ctx);
    char *val = getenv(key->str.ptr);
    if (val) {
        stack_push(ctx, create_string_obj(val, strlen(val)));
    } else {
        stack_push(ctx, init_list_obj()); // Push empty list as a "not found" indicator
        // stack_push(ctx, create_string_obj("", 0));
    }
    release_obj(key);
    return TF_OK;
}

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

tf_ret tf_exit(tf_ctx *ctx) {
    (void)ctx;
    exit(0);
    return TF_OK;
}
