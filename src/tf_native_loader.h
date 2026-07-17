#ifndef TF_NATIVE_LOADER_H
#define TF_NATIVE_LOADER_H

#include "tf_exec.h"

typedef enum {
    TF_NATIVE_MODULE_LOADED,
    TF_NATIVE_MODULE_NOT_FOUND,
    TF_NATIVE_MODULE_ERROR
} tf_native_module_status;

tf_native_module_status tf_native_module_load(tf_ctx *ctx, const char *name,
                                              size_t name_len,
                                              const char *source_directory);
void tf_native_modules_close(tf_ctx *ctx);

#endif  // TF_NATIVE_LOADER_H
