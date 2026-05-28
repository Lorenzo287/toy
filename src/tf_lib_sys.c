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
    if (!tf_ctx_require_type(ctx, 1, TF_OBJ_TYPE_STR) ||
        !tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) {
        return TF_ERR;
    }

    tf_obj *val = tf_stack_pop(ctx);
    tf_obj *name = tf_stack_pop(ctx);

#ifdef _WIN32
    int res = SETENV(name->str.ptr, val->str.ptr);
#else
    int res = SETENV(name->str.ptr, val->str.ptr, 1);
#endif

    tf_stack_push(ctx, tf_obj_new_bool(res == 0));
    tf_obj_release(val);
    tf_obj_release(name);
    return TF_OK;
}

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

tf_ret tf_shell(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;

    tf_obj *cmd_obj = tf_stack_pop(ctx);
    FILE *fp = popen(cmd_obj->str.ptr, "r");
    if (!fp) {
        tf_ctx_runtime_errorf(ctx, "'shell' failed to start command\n");
        tf_obj_release(cmd_obj);
        return TF_ERR;
    }

    char *output = NULL;
    size_t total_size = 0;
    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), fp)) {
        size_t len = strlen(buffer);
        output = tf_xrealloc(output, total_size + len + 1);
        memcpy(output + total_size, buffer, len);
        total_size += len;
        output[total_size] = '\0';
    }

    int status = pclose(fp);
    tf_stack_push(ctx, tf_obj_new_string(output ? output : "", total_size));
    free(output);
    
    // We could also push the status code, but for now let's just return the output
    (void)status; 

    tf_obj_release(cmd_obj);
    return TF_OK;
}

tf_ret tf_argc(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_int(ctx->argc > 0 ? ctx->argc : 0));
    return TF_OK;
}

tf_ret tf_argv(tf_ctx *ctx) {
    tf_obj *list = tf_obj_new_list();
    for (int i = 0; i < ctx->argc; i++) {
        tf_obj *str = tf_obj_new_string(ctx->argv[i], strlen(ctx->argv[i]));
        tf_list_push(list, str);
    }
    tf_stack_push(ctx, list);
    return TF_OK;
}

tf_ret tf_pwd(tf_ctx *ctx) {
    char buf[1024];
#ifdef _WIN32
    if (_getcwd(buf, sizeof(buf))) {
#else
    if (getcwd(buf, sizeof(buf))) {
#endif
        tf_stack_push(ctx, tf_obj_new_string(buf, strlen(buf)));
        return TF_OK;
    }
    tf_ctx_runtime_errorf(ctx, "'pwd' failed to read current directory\n");
    return TF_ERR;
}

tf_ret tf_getenv(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;

    tf_obj *key = tf_stack_pop(ctx);
    char *val = getenv(key->str.ptr);
    if (!val) {
        tf_ctx_runtime_errorf(ctx, "'getenv' variable is not set: %s\n",
                              key->str.ptr);
        tf_obj_release(key);
        return TF_ERR;
    }
    tf_stack_push(ctx, tf_obj_new_string(val, strlen(val)));
    tf_obj_release(key);
    return TF_OK;
}

tf_ret tf_env_q(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_STR)) return TF_ERR;

    tf_obj *key = tf_stack_pop(ctx);
    tf_stack_push(ctx, tf_obj_new_bool(getenv(key->str.ptr) != NULL));
    tf_obj_release(key);
    return TF_OK;
}

tf_ret tf_rand(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_int(rand()));
    return TF_OK;
}

tf_ret tf_sleep(tf_ctx *ctx) {
    if (!tf_ctx_require_type(ctx, 0, TF_OBJ_TYPE_INT)) return TF_ERR;
    tf_obj *ms_obj = tf_stack_peek(ctx, 0);
    if (ms_obj->i < 0) {
        tf_ctx_runtime_errorf(ctx, "'sleep' duration must be non-negative\n");
        return TF_ERR;
    }
    ms_obj = tf_stack_pop(ctx);
#ifdef _WIN32
    Sleep(ms_obj->i);
#else
    struct timespec req = {.tv_sec = ms_obj->i / 1000,
                           .tv_nsec = (long)(ms_obj->i % 1000) * 1000000L};
    nanosleep(&req, NULL);
#endif
    tf_obj_release(ms_obj);
    return TF_OK;
}

tf_ret tf_time(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_int((int)time(NULL)));
    return TF_OK;
}

tf_ret tf_clock(tf_ctx *ctx) {
    tf_stack_push(ctx, tf_obj_new_int((int)clock()));
    return TF_OK;
}

tf_ret tf_exit(tf_ctx *ctx) {
    (void)ctx;
    exit(0);
    return TF_OK;
}
