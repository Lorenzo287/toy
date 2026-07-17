#include "tf_exec.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tf_alloc.h"

bool tf_module_name_valid(const char *name, size_t name_len) {
    if (!name || name_len == 0) return false;
    bool segment_start = true;
    for (size_t i = 0; i < name_len;) {
        if (name[i] == '.') {
            if (segment_start) return false;
            segment_start = true;
            i++;
            continue;
        }
        unsigned char c = (unsigned char)name[i];
        if (segment_start) {
            if (!isalpha(c) && c != '_') return false;
            segment_start = false;
        } else if (!isalnum(c) && c != '_' && c != '-') {
            return false;
        }
        i++;
    }
    return !segment_start;
}

bool tf_module_word_name_valid(const char *name, size_t name_len) {
    if (!name || name_len == 0) return false;
    const char *operators = "+-*%<>=!?";
    unsigned char first = (unsigned char)name[0];
    if (!isalpha(first) && first != '_' && !strchr(operators, first)) {
        return false;
    }
    for (size_t i = 1; i < name_len; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!isalnum(c) && c != '_' && !strchr(operators, c)) return false;
    }
    return true;
}

size_t tf_module_find(tf_ctx *ctx, const char *name, size_t name_len) {
    if (!ctx || !name) return (size_t)-1;
    for (size_t i = 1; i < ctx->modules.len; i++) {
        tf_module *module = &ctx->modules.entries[i];
        if (module->name_len == name_len &&
            memcmp(module->name, name, name_len) == 0) {
            return i;
        }
    }
    return (size_t)-1;
}

static size_t module_add(tf_ctx *ctx, const char *name, size_t name_len,
                         const char *path, tf_module_state state) {
    if (ctx->modules.len >= ctx->modules.cap) {
        ctx->modules.cap *= 2;
        ctx->modules.entries =
            tf_xrealloc(ctx->modules.entries,
                        sizeof(tf_module) * ctx->modules.cap);
    }

    size_t index = ctx->modules.len++;
    tf_module *module = &ctx->modules.entries[index];
    module->name = tf_xmalloc(name_len + 1);
    memcpy(module->name, name, name_len);
    module->name[name_len] = '\0';
    module->name_len = name_len;
    module->path = path ? tf_xstrdup(path) : NULL;
    module->state = state;
    return index;
}

size_t tf_module_begin(tf_ctx *ctx, const char *name, size_t name_len,
                       const char *path) {
    return module_add(ctx, name, name_len, path, TF_MODULE_LOADING);
}

size_t tf_module_add_native(tf_ctx *ctx, const char *name, size_t name_len) {
    return module_add(ctx, name, name_len, NULL, TF_MODULE_LOADED);
}

void tf_module_finish(tf_ctx *ctx, size_t module_index, tf_ret status) {
    if (!ctx || module_index == TF_ROOT_MODULE ||
        module_index >= ctx->modules.len) {
        return;
    }
    ctx->modules.entries[module_index].state =
        status == TF_OK ? TF_MODULE_LOADED : TF_MODULE_FAILED;
}

const tf_module *tf_module_get(tf_ctx *ctx, size_t module_index) {
    if (!ctx || module_index >= ctx->modules.len) return NULL;
    return &ctx->modules.entries[module_index];
}

size_t tf_module_alias_find(tf_ctx *ctx, size_t owner_module_index,
                            const char *name, size_t name_len) {
    if (!ctx || !name) return (size_t)-1;
    for (size_t i = 0; i < ctx->module_aliases.len; i++) {
        tf_module_alias *alias = &ctx->module_aliases.entries[i];
        if (alias->owner_module_index == owner_module_index &&
            alias->name_len == name_len &&
            memcmp(alias->name, name, name_len) == 0) {
            return alias->target_module_index;
        }
    }
    return (size_t)-1;
}

bool tf_module_alias_add(tf_ctx *ctx, size_t owner_module_index,
                         const char *name, size_t name_len,
                         size_t target_module_index) {
    size_t existing = tf_module_alias_find(ctx, owner_module_index, name,
                                           name_len);
    if (existing != (size_t)-1) return existing == target_module_index;

    if (ctx->module_aliases.len >= ctx->module_aliases.cap) {
        ctx->module_aliases.cap *= 2;
        ctx->module_aliases.entries =
            tf_xrealloc(ctx->module_aliases.entries,
                        sizeof(tf_module_alias) * ctx->module_aliases.cap);
    }

    tf_module_alias *alias =
        &ctx->module_aliases.entries[ctx->module_aliases.len++];
    alias->name = tf_xmalloc(name_len + 1);
    memcpy(alias->name, name, name_len);
    alias->name[name_len] = '\0';
    alias->name_len = name_len;
    alias->owner_module_index = owner_module_index;
    alias->target_module_index = target_module_index;
    return true;
}

void tf_module_alias_remove(tf_ctx *ctx, size_t owner_module_index,
                            const char *name, size_t name_len,
                            size_t target_module_index) {
    if (!ctx || !name) return;
    for (size_t i = 0; i < ctx->module_aliases.len; i++) {
        tf_module_alias *alias = &ctx->module_aliases.entries[i];
        if (alias->owner_module_index != owner_module_index ||
            alias->target_module_index != target_module_index ||
            alias->name_len != name_len ||
            memcmp(alias->name, name, name_len) != 0) {
            continue;
        }
        free(alias->name);
        ctx->module_aliases.len--;
        if (i != ctx->module_aliases.len) {
            ctx->module_aliases.entries[i] =
                ctx->module_aliases.entries[ctx->module_aliases.len];
        }
        return;
    }
}
