#include "tf_exec.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tf_alloc.h"

_Static_assert((TF_WORD_LOOKUP_CACHE_CAP & (TF_WORD_LOOKUP_CACHE_CAP - 1)) == 0,
               "word lookup cache capacity must be a power of two");

/* === Word Dictionary === */

static unsigned long dict_hash(const char *name, size_t len) {
    unsigned long hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)name[i];
    }
    return hash;
}

static void dict_resize(tf_ctx *ctx) {
    size_t *old_buckets = ctx->words.buckets;

    ctx->words.capacity *= 2;
    ctx->words.buckets = tf_xcalloc(ctx->words.capacity, sizeof(size_t));

    for (size_t i = 0; i < ctx->words.count; i++) {
        tf_word *word = &ctx->words.entries[i];
        unsigned long h = dict_hash(word->name, word->name_len);
        size_t idx = h % ctx->words.capacity;
        while (ctx->words.buckets[idx]) {
            idx = (idx + 1) % ctx->words.capacity;
        }
        ctx->words.buckets[idx] = i + 1;
    }
    free(old_buckets);
}
static tf_word *dict_insert_word(tf_ctx *ctx, const char *name, size_t name_len,
                                 bool copy_name, size_t module_index,
                                 bool exported) {
    if (ctx->words.count >= ctx->words.capacity * 0.7) {
        dict_resize(ctx);
    }

    unsigned long h = dict_hash(name, name_len);
    size_t idx = h % ctx->words.capacity;
    while (ctx->words.buckets[idx]) {
        idx = (idx + 1) % ctx->words.capacity;
    }

    if (ctx->words.count >= ctx->words.entry_capacity) {
        ctx->words.entry_capacity *= 2;
        ctx->words.entries =
            tf_xrealloc(ctx->words.entries,
                        sizeof(tf_word) * ctx->words.entry_capacity);
    }
    size_t entry_idx = ctx->words.count++;
    tf_word *f = &ctx->words.entries[entry_idx];
    if (copy_name) {
        char *owned_name = tf_xmalloc(name_len + 1);
        memcpy(owned_name, name, name_len);
        owned_name[name_len] = '\0';
        f->name = owned_name;
    } else {
        f->name = name;
    }
    f->name_len = name_len;
    f->owns_name = copy_name;
    f->module_index = module_index;
    f->exported = exported;
    f->type = TF_WORD_NATIVE;
    f->native_impl = NULL;
    ctx->words.buckets[idx] = entry_idx + 1;
    return f;
}

static void dict_set_native(tf_ctx *ctx, const char *name, tf_native_fn cb,
                            bool copy_name) {
    size_t name_len = strlen(name);
    tf_word *f = tf_dict_lookup_name(ctx, name, name_len);
    if (f) {  // overwrite if name is already taken
        if (f->type == TF_WORD_USER) tf_obj_release(f->user_impl);
    } else {  // allocate if name is not taken
        f = dict_insert_word(ctx, name, name_len, copy_name, TF_ROOT_MODULE,
                             true);
    }
    f->module_index = TF_ROOT_MODULE;
    f->exported = true;
    f->type = TF_WORD_NATIVE;
    f->native_impl = cb;
}

void tf_dict_set_native(tf_ctx *ctx, const char *name, tf_native_fn cb) {
    dict_set_native(ctx, name, cb, false);
}

void tf_dict_set_native_copy(tf_ctx *ctx, const char *name, tf_native_fn cb) {
    dict_set_native(ctx, name, cb, true);
}

void tf_dict_add_native_scoped(tf_ctx *ctx, const char *name, size_t name_len,
                               size_t module_index, tf_native_fn cb) {
    assert(tf_dict_lookup_name(ctx, name, name_len) == NULL);
    tf_word *word = dict_insert_word(ctx, name, name_len, true, module_index,
                                     true);
    word->type = TF_WORD_NATIVE;
    word->native_impl = cb;
}

static bool name_is_qualified(const char *name, size_t len) {
    return memchr(name, '.', len) != NULL;
}

static bool split_alias_request(const char *name, size_t len,
                                size_t *alias_len, const char **local_name,
                                size_t *local_len) {
    size_t separator = (size_t)-1;
    for (size_t i = 0; i < len; i++) {
        if (name[i] != '.') continue;
        if (separator != (size_t)-1) return false;
        separator = i;
    }
    if (separator == (size_t)-1 || separator == 0 || separator + 1 >= len) {
        return false;
    }
    *alias_len = separator;
    *local_name = name + separator + 1;
    *local_len = len - separator - 1;
    return true;
}

static size_t alias_target_for_request(tf_ctx *ctx, size_t owner_module_index,
                                       const char *name, size_t len,
                                       const char **local_name,
                                       size_t *local_len) {
    size_t alias_len = 0;
    if (!split_alias_request(name, len, &alias_len, local_name, local_len)) {
        return (size_t)-1;
    }
    return tf_module_alias_find(ctx, owner_module_index, name, alias_len);
}

static tf_word *dict_lookup_scoped_exact(tf_ctx *ctx, size_t module_index,
                                         const char *name, size_t name_len) {
    if (module_index == TF_ROOT_MODULE) {
        return tf_dict_lookup_name(ctx, name, name_len);
    }
    const tf_module *module = tf_module_get(ctx, module_index);
    if (!module) return NULL;

    size_t qualified_len = module->name_len + 1 + name_len;
    char *qualified = tf_xmalloc(qualified_len + 1);
    memcpy(qualified, module->name, module->name_len);
    qualified[module->name_len] = '.';
    memcpy(qualified + module->name_len + 1, name, name_len);
    qualified[qualified_len] = '\0';
    tf_word *word = tf_dict_lookup_name(ctx, qualified, qualified_len);
    free(qualified);
    return word;
}

static tf_word *dict_lookup_alias(tf_ctx *ctx, size_t owner_module_index,
                                  const char *name, size_t name_len,
                                  bool *alias_bound) {
    const char *local_name = NULL;
    size_t local_len = 0;
    size_t target = alias_target_for_request(
        ctx, owner_module_index, name, name_len, &local_name, &local_len);
    *alias_bound = target != (size_t)-1;
    if (!*alias_bound) return NULL;
    return dict_lookup_scoped_exact(ctx, target, local_name, local_len);
}

static char *qualified_word_name(tf_ctx *ctx, size_t module_index,
                                 const char *name, size_t name_len,
                                 size_t *qualified_len) {
    const tf_module *module = tf_module_get(ctx, module_index);
    if (!module) return NULL;
    *qualified_len = module->name_len + 1 + name_len;
    char *qualified = tf_xmalloc(*qualified_len + 1);
    memcpy(qualified, module->name, module->name_len);
    qualified[module->name_len] = '.';
    memcpy(qualified + module->name_len + 1, name, name_len);
    qualified[*qualified_len] = '\0';
    return qualified;
}

bool tf_dict_set_user(tf_ctx *ctx, tf_obj *name, tf_obj *uf) {
    if (name_is_qualified(name->str.ptr, name->str.len)) {
        tf_ctx_runtime_errorf(ctx,
                              "'def' names must be local to the current module\n");
        return false;
    }

    size_t module_index = tf_current_module_index(ctx);
    tf_word *f = dict_lookup_scoped_exact(ctx, module_index, name->str.ptr,
                                          name->str.len);
    if (f) {
        if (f->module_index != module_index) {
            tf_ctx_runtime_errorf(ctx,
                                  "module word '%s' conflicts with a native word\n",
                                  f->name);
            return false;
        }
        if (f->type == TF_WORD_USER) tf_obj_release(f->user_impl);
    } else {
        if (module_index == TF_ROOT_MODULE) {
            f = dict_insert_word(ctx, name->str.ptr, name->str.len, true,
                                 module_index, true);
        } else {
            size_t qualified_len = 0;
            char *qualified = qualified_word_name(
                ctx, module_index, name->str.ptr, name->str.len,
                &qualified_len);
            if (!qualified) return false;
            f = dict_insert_word(ctx, qualified, qualified_len, true,
                                 module_index, false);
            free(qualified);
        }
    }
    f->type = TF_WORD_USER;
    f->user_impl = uf;
    tf_obj_retain(uf);
    return true;
}

bool tf_dict_export(tf_ctx *ctx, tf_obj *name) {
    size_t module_index = tf_current_module_index(ctx);
    if (module_index == TF_ROOT_MODULE) {
        tf_ctx_runtime_errorf(ctx, "'export' is only valid while loading a module\n");
        return false;
    }
    if (name_is_qualified(name->str.ptr, name->str.len)) {
        tf_ctx_runtime_errorf(ctx, "'export' expected a local word name\n");
        return false;
    }

    tf_word *word = dict_lookup_scoped_exact(ctx, module_index, name->str.ptr,
                                             name->str.len);
    if (!word) {
        tf_ctx_runtime_errorf(ctx, "cannot export undefined module word '%s'\n",
                              name->str.ptr);
        return false;
    }
    word->exported = true;
    return true;
}

static bool word_visible(tf_ctx *ctx, tf_word *word,
                         size_t current_module) {
    if (word->module_index == current_module) return true;
    if (word->module_index == TF_ROOT_MODULE) {
        return word->type == TF_WORD_NATIVE || current_module == TF_ROOT_MODULE;
    }
    const tf_module *module = tf_module_get(ctx, word->module_index);
    return module && module->state == TF_MODULE_LOADED && word->exported;
}

bool tf_dict_word_visible(tf_ctx *ctx, tf_word *word) {
    return word && word_visible(ctx, word, tf_current_module_index(ctx));
}

static bool word_matches_request(tf_ctx *ctx, tf_word *word,
                                 size_t current_module, const char *name,
                                 size_t name_len) {
    if (!word_visible(ctx, word, current_module)) return false;
    if (name_is_qualified(name, name_len)) {
        const char *local_name = NULL;
        size_t local_len = 0;
        size_t target = alias_target_for_request(
            ctx, current_module, name, name_len, &local_name, &local_len);
        if (target != (size_t)-1) {
            const tf_module *module = tf_module_get(ctx, target);
            size_t prefix_len = module ? module->name_len + 1 : 0;
            return module && word->module_index == target &&
                   word->name_len == prefix_len + local_len &&
                   memcmp(word->name, module->name, module->name_len) == 0 &&
                   word->name[module->name_len] == '.' &&
                   memcmp(word->name + prefix_len, local_name, local_len) == 0;
        }
        return word->name_len == name_len &&
               memcmp(word->name, name, name_len) == 0;
    }
    if (word->module_index == TF_ROOT_MODULE) {
        return word->name_len == name_len &&
               memcmp(word->name, name, name_len) == 0;
    }
    if (word->module_index != current_module) return false;
    const tf_module *module = tf_module_get(ctx, current_module);
    size_t prefix_len = module ? module->name_len + 1 : 0;
    return word->name_len == prefix_len + name_len &&
           memcmp(word->name + prefix_len, name, name_len) == 0;
}

tf_word *tf_dict_lookup(tf_ctx *ctx, tf_obj *name) {
    if (!name || (name->type != TF_OBJ_TYPE_SYMBOL &&
                  name->type != TF_OBJ_TYPE_CALL)) {
        return NULL;
    }

    size_t current_module = tf_current_module_index(ctx);
    uintptr_t object_key = (uintptr_t)name;
    uintptr_t mixed_key = (object_key >> 4) ^ (uintptr_t)current_module;
    mixed_key ^= mixed_key >> 7;
    mixed_key ^= mixed_key >> 13;
    size_t slot = mixed_key & (TF_WORD_LOOKUP_CACHE_CAP - 1);
    uintptr_t cached_key = ctx->words.lookup_cache[slot].key;
    size_t cached_module = ctx->words.lookup_cache[slot].module_index;
    size_t entry_index = ctx->words.lookup_cache[slot].entry_index;
    if (cached_key == object_key && cached_module == current_module &&
        entry_index < ctx->words.count) {
        tf_word *word = &ctx->words.entries[entry_index];
        if (word_matches_request(ctx, word, current_module, name->str.ptr,
                                 name->str.len)) {
            return word;
        }
    }

    tf_word *word = NULL;
    if (name_is_qualified(name->str.ptr, name->str.len)) {
        bool alias_bound = false;
        word = dict_lookup_alias(ctx, current_module, name->str.ptr,
                                 name->str.len, &alias_bound);
        if (!alias_bound) {
            word = tf_dict_lookup_name(ctx, name->str.ptr, name->str.len);
        }
        if (word && !word_visible(ctx, word, current_module)) word = NULL;
    } else {
        if (current_module != TF_ROOT_MODULE) {
            word = dict_lookup_scoped_exact(ctx, current_module, name->str.ptr,
                                            name->str.len);
        }
        if (!word) {
            tf_word *root = tf_dict_lookup_name(ctx, name->str.ptr,
                                                name->str.len);
            if (root && word_visible(ctx, root, current_module)) word = root;
        }
    }
    if (word) {
        ctx->words.lookup_cache[slot].key = object_key;
        ctx->words.lookup_cache[slot].module_index = current_module;
        ctx->words.lookup_cache[slot].entry_index =
            (size_t)(word - ctx->words.entries);
    } else {
        ctx->words.lookup_cache[slot].key = 0;
    }
    return word;
}

tf_word *tf_dict_lookup_name(tf_ctx *ctx, const char *name, size_t len) {
    if (ctx->words.capacity == 0) return NULL;
    unsigned long h = dict_hash(name, len);
    size_t idx = h % ctx->words.capacity;
    // linear probing
    while (ctx->words.buckets[idx]) {
        tf_word *word =
            &ctx->words.entries[ctx->words.buckets[idx] - 1];
        if (word->name_len == len && memcmp(word->name, name, len) == 0) {
            return word;
        }
        idx = (idx + 1) % ctx->words.capacity;
    }
    return NULL;
}

tf_word *tf_dict_namespace_conflict(tf_ctx *ctx, const char *module_name,
                                    size_t module_name_len) {
    if (!ctx || !module_name) return NULL;
    for (size_t i = 0; i < ctx->words.count; i++) {
        tf_word *word = &ctx->words.entries[i];
        if (word->module_index != TF_ROOT_MODULE) continue;
        if (word->name_len <= module_name_len + 1) continue;
        if (memcmp(word->name, module_name, module_name_len) == 0 &&
            word->name[module_name_len] == '.') {
            return word;
        }
    }
    return NULL;
}
