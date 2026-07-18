#ifndef TF_NATIVE_LOADER_H
#define TF_NATIVE_LOADER_H

#include "tf_exec.h"

/* Load the exact shared library named by a package manifest. */
tf_ret tf_native_package_load(tf_ctx *ctx, size_t package_index,
                              const char *path);
void tf_native_packages_close(tf_ctx *ctx);

#endif  // TF_NATIVE_LOADER_H
