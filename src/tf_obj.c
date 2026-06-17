#include "tf_obj.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tf_alloc.h"

/* === Object Creation === */

tf_obj *tf_obj_new(int type) {
    tf_obj *o = tf_xmalloc(sizeof(tf_obj));
    o->type = type;
    o->refcount = 1;
    o->span = (tf_source_span){0};
    return o;
}

tf_obj *tf_obj_new_list(void) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_LIST);
    o->list.len = 0;
    o->list.cap = 32;
    o->list.elem = tf_xmalloc(sizeof(tf_obj *) * o->list.cap);
    return o;
}

tf_obj *tf_obj_new_map(void) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_MAP);
    o->map.len = 0;
    o->map.cap = 16;
    o->map.entries = tf_xmalloc(sizeof(tf_map_entry) * o->map.cap);
    o->map.bucket_cap = 32;
    o->map.buckets = tf_xcalloc(o->map.bucket_cap, sizeof(size_t));
    return o;
}

tf_obj *tf_obj_new_set(void) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_SET);
    o->set.len = 0;
    o->set.cap = 16;
    o->set.entries = tf_xmalloc(sizeof(tf_set_entry) * o->set.cap);
    o->set.bucket_cap = 32;
    o->set.buckets = tf_xcalloc(o->set.bucket_cap, sizeof(size_t));
    return o;
}

tf_obj *tf_obj_new_int(int i) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_INT);
    o->i = i;
    return o;
}

tf_obj *tf_obj_new_bool(bool b) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_BOOL);
    o->b = b;
    return o;
}

tf_obj *tf_obj_new_float(float f) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_FLOAT);
    o->f = f;
    return o;
}

tf_obj *tf_obj_new_symbol(const char *s, size_t len) {
    tf_obj *o = tf_obj_new_string(s, len);
    o->type = TF_OBJ_TYPE_SYMBOL;
    return o;
}

tf_obj *tf_obj_new_quoted_symbol(const char *s, size_t len) {
    tf_obj *o = tf_obj_new_symbol(s, len);
    o->str.quoted = true;
    return o;
}

tf_obj *tf_obj_new_string(const char *s, size_t len) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_STR);
    o->str.ptr = tf_xmalloc(len + 1);
    o->str.len = len;
    o->str.quoted = false;
    memcpy(o->str.ptr, s, len);
    o->str.ptr[len] = 0;
    return o;
}

void tf_obj_set_span(tf_obj *o, tf_source_span span) {
    if (!o) return;
    if (o->span.valid && o->span.filename) { free((char *)o->span.filename); }
    o->span = span;
    if (span.valid && span.filename) { o->span.filename = tf_xstrdup(span.filename); }
}

/* === Object Utilities === */

int tf_obj_compare_string(tf_obj *a, tf_obj *b) {
    size_t min_len = a->str.len < b->str.len ? a->str.len : b->str.len;
    int cmp = memcmp(a->str.ptr, b->str.ptr, min_len);
    if (cmp == 0) {
        if (a->str.len == b->str.len)
            return 0;
        else if (a->str.len > b->str.len)
            return 1;
        else
            return -1;
    } else {
        if (cmp < 0) return -1;
        return 1;
    }
}

void tf_list_push(tf_obj *l, tf_obj *elem) {
    if (l->list.len >= l->list.cap) {
        l->list.cap *= 2;
        l->list.elem = tf_xrealloc(l->list.elem, sizeof(tf_obj *) * l->list.cap);
    }
    l->list.elem[l->list.len++] = elem;
}

// pop object only if the type is correct
tf_obj *tf_list_pop_type(tf_obj *l, tf_type type) {
    if (l->list.len == 0) return NULL;
    tf_obj *o = l->list.elem[l->list.len - 1];
    if (o->type != type) return NULL;
    return tf_list_pop(l);
}

tf_obj *tf_list_pop(tf_obj *l) {
    if (l->list.len == 0) return NULL;
    tf_obj *o = l->list.elem[l->list.len - 1];

    l->list.len--;
    if (l->list.len < l->list.cap / 4 && l->list.cap > 32) {
        l->list.cap /= 2;
        l->list.elem = tf_xrealloc(l->list.elem, sizeof(tf_obj *) * l->list.cap);
    }
    return o;
}

static uint64_t fnv1a_bytes(const void *data, size_t len, uint64_t seed) {
    const unsigned char *bytes = data;
    uint64_t h = seed;
    for (size_t i = 0; i < len; i++) {
        h ^= bytes[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static uint64_t hash_mix_u64(uint64_t h, uint64_t value) {
    h ^= value;
    h *= 1099511628211ULL;
    return h;
}

bool tf_obj_hashable(tf_obj *o) {
    if (!o) return false;
    return o->type == TF_OBJ_TYPE_BOOL || o->type == TF_OBJ_TYPE_INT ||
           o->type == TF_OBJ_TYPE_STR || o->type == TF_OBJ_TYPE_SYMBOL;
}

uint64_t tf_obj_hash(tf_obj *o) {
    uint64_t h = 1469598103934665603ULL;
    h = hash_mix_u64(h, (uint64_t)o->type);
    switch (o->type) {
    case TF_OBJ_TYPE_BOOL:
        return hash_mix_u64(h, o->b ? 1 : 0);
    case TF_OBJ_TYPE_INT:
        return hash_mix_u64(h, (uint32_t)o->i);
    case TF_OBJ_TYPE_STR:
    case TF_OBJ_TYPE_SYMBOL:
        return fnv1a_bytes(o->str.ptr, o->str.len, h);
    default:
        return h;
    }
}

static size_t map_find_slot(tf_obj *map, tf_obj *key, uint64_t hash,
                            bool *found) {
    size_t idx = (size_t)(hash % map->map.bucket_cap);
    while (map->map.buckets[idx] != 0) {
        size_t entry_idx = map->map.buckets[idx] - 1;
        tf_map_entry *entry = &map->map.entries[entry_idx];
        if (entry->hash == hash && tf_obj_equal(entry->key, key)) {
            *found = true;
            return idx;
        }
        idx = (idx + 1) % map->map.bucket_cap;
    }
    *found = false;
    return idx;
}

static size_t set_find_slot(tf_obj *set, tf_obj *item, uint64_t hash,
                            bool *found) {
    size_t idx = (size_t)(hash % set->set.bucket_cap);
    while (set->set.buckets[idx] != 0) {
        size_t entry_idx = set->set.buckets[idx] - 1;
        tf_set_entry *entry = &set->set.entries[entry_idx];
        if (entry->hash == hash && tf_obj_equal(entry->item, item)) {
            *found = true;
            return idx;
        }
        idx = (idx + 1) % set->set.bucket_cap;
    }
    *found = false;
    return idx;
}

static void map_rebuild_buckets(tf_obj *map, size_t bucket_cap) {
    free(map->map.buckets);
    map->map.bucket_cap = bucket_cap;
    map->map.buckets = tf_xcalloc(bucket_cap, sizeof(size_t));
    for (size_t i = 0; i < map->map.len; i++) {
        bool found = false;
        size_t slot = map_find_slot(map, map->map.entries[i].key,
                                    map->map.entries[i].hash, &found);
        map->map.buckets[slot] = i + 1;
    }
}

static void set_rebuild_buckets(tf_obj *set, size_t bucket_cap) {
    free(set->set.buckets);
    set->set.bucket_cap = bucket_cap;
    set->set.buckets = tf_xcalloc(bucket_cap, sizeof(size_t));
    for (size_t i = 0; i < set->set.len; i++) {
        bool found = false;
        size_t slot = set_find_slot(set, set->set.entries[i].item,
                                    set->set.entries[i].hash, &found);
        set->set.buckets[slot] = i + 1;
    }
}

bool tf_map_has(tf_obj *map, tf_obj *key) {
    if (!map || map->type != TF_OBJ_TYPE_MAP || !tf_obj_hashable(key)) {
        return false;
    }
    bool found = false;
    (void)map_find_slot(map, key, tf_obj_hash(key), &found);
    return found;
}

bool tf_map_get(tf_obj *map, tf_obj *key, tf_obj **out) {
    if (!map || map->type != TF_OBJ_TYPE_MAP || !tf_obj_hashable(key)) {
        return false;
    }
    bool found = false;
    size_t slot = map_find_slot(map, key, tf_obj_hash(key), &found);
    if (!found) return false;
    if (out) *out = map->map.entries[map->map.buckets[slot] - 1].value;
    return true;
}

bool tf_map_set(tf_obj *map, tf_obj *key, tf_obj *value) {
    if (!map || map->type != TF_OBJ_TYPE_MAP || !tf_obj_hashable(key)) {
        return false;
    }

    if ((map->map.len + 1) * 2 >= map->map.bucket_cap) {
        map_rebuild_buckets(map, map->map.bucket_cap * 2);
    }

    uint64_t hash = tf_obj_hash(key);
    bool found = false;
    size_t slot = map_find_slot(map, key, hash, &found);
    if (found) {
        tf_map_entry *entry = &map->map.entries[map->map.buckets[slot] - 1];
        tf_obj_retain(value);
        tf_obj_release(entry->value);
        entry->value = value;
        return true;
    }

    if (map->map.len >= map->map.cap) {
        map->map.cap *= 2;
        map->map.entries =
            tf_xrealloc(map->map.entries, sizeof(tf_map_entry) * map->map.cap);
    }
    size_t entry_idx = map->map.len++;
    map->map.entries[entry_idx] = (tf_map_entry){key, value, hash};
    tf_obj_retain(key);
    tf_obj_retain(value);
    map->map.buckets[slot] = entry_idx + 1;
    return true;
}

bool tf_set_has(tf_obj *set, tf_obj *item) {
    if (!set || set->type != TF_OBJ_TYPE_SET || !tf_obj_hashable(item)) {
        return false;
    }
    bool found = false;
    (void)set_find_slot(set, item, tf_obj_hash(item), &found);
    return found;
}

bool tf_set_add(tf_obj *set, tf_obj *item) {
    if (!set || set->type != TF_OBJ_TYPE_SET || !tf_obj_hashable(item)) {
        return false;
    }

    if ((set->set.len + 1) * 2 >= set->set.bucket_cap) {
        set_rebuild_buckets(set, set->set.bucket_cap * 2);
    }

    uint64_t hash = tf_obj_hash(item);
    bool found = false;
    size_t slot = set_find_slot(set, item, hash, &found);
    if (found) return true;

    if (set->set.len >= set->set.cap) {
        set->set.cap *= 2;
        set->set.entries =
            tf_xrealloc(set->set.entries, sizeof(tf_set_entry) * set->set.cap);
    }
    size_t entry_idx = set->set.len++;
    set->set.entries[entry_idx] = (tf_set_entry){item, hash};
    tf_obj_retain(item);
    set->set.buckets[slot] = entry_idx + 1;
    return true;
}

#define TF_EQUAL_MAX_DEPTH 1024

static bool obj_equal_inner(tf_obj *a, tf_obj *b, size_t depth) {
    if (a == b) return true;
    if (!a || !b || depth > TF_EQUAL_MAX_DEPTH) return false;
    if (a->type != b->type) return false;

    switch (a->type) {
    case TF_OBJ_TYPE_BOOL:
        return a->b == b->b;
    case TF_OBJ_TYPE_INT:
        return a->i == b->i;
    case TF_OBJ_TYPE_FLOAT:
        return a->f == b->f;
    case TF_OBJ_TYPE_STR:
    case TF_OBJ_TYPE_SYMBOL:
    case TF_OBJ_TYPE_VARFETCH:
        return tf_obj_compare_string(a, b) == 0;
    case TF_OBJ_TYPE_LIST:
    case TF_OBJ_TYPE_VARLIST:
        if (a->list.len != b->list.len) return false;
        for (size_t i = 0; i < a->list.len; i++) {
            if (!obj_equal_inner(a->list.elem[i], b->list.elem[i], depth + 1)) {
                return false;
            }
        }
        return true;
    case TF_OBJ_TYPE_MAP:
        if (a->map.len != b->map.len) return false;
        for (size_t i = 0; i < a->map.len; i++) {
            tf_obj *other = NULL;
            if (!tf_map_get(b, a->map.entries[i].key, &other)) return false;
            if (!obj_equal_inner(a->map.entries[i].value, other, depth + 1)) {
                return false;
            }
        }
        return true;
    case TF_OBJ_TYPE_SET:
        if (a->set.len != b->set.len) return false;
        for (size_t i = 0; i < a->set.len; i++) {
            if (!tf_set_has(b, a->set.entries[i].item)) return false;
        }
        return true;
    default:
        return a == b;
    }
}

bool tf_obj_equal(tf_obj *a, tf_obj *b) {
    return obj_equal_inner(a, b, 0);
}

void tf_obj_retain(tf_obj *o) {
    o->refcount++;
}

void tf_obj_release(tf_obj *o) {
    assert(o->refcount > 0);
    o->refcount--;
    if (o->refcount == 0) tf_obj_free(o);
}

void tf_obj_free(tf_obj *o) {
    if (o->span.valid && o->span.filename) { free((char *)o->span.filename); }
    switch (o->type) {
    case TF_OBJ_TYPE_VARLIST:
    case TF_OBJ_TYPE_LIST:
        for (size_t i = 0; i < o->list.len; i++) tf_obj_release(o->list.elem[i]);
        free(o->list.elem);
        break;
    case TF_OBJ_TYPE_MAP:
        for (size_t i = 0; i < o->map.len; i++) {
            tf_obj_release(o->map.entries[i].key);
            tf_obj_release(o->map.entries[i].value);
        }
        free(o->map.entries);
        free(o->map.buckets);
        break;
    case TF_OBJ_TYPE_SET:
        for (size_t i = 0; i < o->set.len; i++) {
            tf_obj_release(o->set.entries[i].item);
        }
        free(o->set.entries);
        free(o->set.buckets);
        break;
    case TF_OBJ_TYPE_VARFETCH:
    case TF_OBJ_TYPE_SYMBOL:
    case TF_OBJ_TYPE_STR:
        free(o->str.ptr);
        break;
    default:
        break;
    }
    free(o);
}

static void print_escaped_string(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        switch (s[i]) {
        case '\\':
            printf("\\\\");
            break;
        case '"':
            printf("\\\"");
            break;
        case '\n':
            printf("\\n");
            break;
        case '\r':
            printf("\\r");
            break;
        case '\t':
            printf("\\t");
            break;
        default:
            putchar((unsigned char)s[i]);
            break;
        }
    }
}

// Debug printer: includes type tags and is intended for developer inspection
void tf_obj_print(tf_obj *o, size_t *count) {
    (*count)++;
    switch (o->type) {
    case TF_OBJ_TYPE_INT:
        printf("(int:%d)", o->i);
        break;
    case TF_OBJ_TYPE_FLOAT:
        printf("(float:%g)", o->f);
        break;
    case TF_OBJ_TYPE_SYMBOL:
        printf("(symbol:%s%s)", o->str.quoted ? "'" : "", o->str.ptr);
        break;
    case TF_OBJ_TYPE_STR:
        printf("(string:\"");
        print_escaped_string(o->str.ptr, o->str.len);
        printf("\")");
        break;
    case TF_OBJ_TYPE_BOOL:
        printf("(bool:%d)", o->b);
        break;
    case TF_OBJ_TYPE_VARFETCH:
        printf("(fetch:$%s)", o->str.ptr);
        break;
    case TF_OBJ_TYPE_VARLIST:
        (*count)--;
        printf("|");
        for (size_t i = 0; i < o->list.len; i++) {
            tf_obj_print(o->list.elem[i], count);
            if (i != o->list.len - 1) printf(" ");
        }
        printf("|");
        break;
    case TF_OBJ_TYPE_LIST:
        (*count)--;
        printf("[");
        for (size_t i = 0; i < o->list.len; i++) {
            tf_obj_print(o->list.elem[i], count);
            if (i != o->list.len - 1) {
                if (*count % 6 == 0)
                    printf("\n");
                else
                    printf(" ");
            }
        }
        printf("]");
        break;
    case TF_OBJ_TYPE_MAP:
        (*count)--;
        printf("{");
        for (size_t i = 0; i < o->map.len; i++) {
            if (i > 0) printf(" ");
            tf_obj_print(o->map.entries[i].key, count);
            printf(" ");
            tf_obj_print(o->map.entries[i].value, count);
        }
        printf("}");
        break;
    case TF_OBJ_TYPE_SET:
        (*count)--;
        printf("#{");
        for (size_t i = 0; i < o->set.len; i++) {
            if (i > 0) printf(" ");
            tf_obj_print(o->set.entries[i].item, count);
        }
        printf("}");
        break;
    default:
        printf("?");
    }
}

// Runtime printer: prints values the way user-facing words like `print` expect
void tf_obj_print_value(tf_obj *o) {
    switch (o->type) {
    case TF_OBJ_TYPE_INT:
        printf("%d", o->i);
        break;
    case TF_OBJ_TYPE_FLOAT:
        printf("%g", o->f);
        break;
    case TF_OBJ_TYPE_SYMBOL:
        printf("%s", o->str.ptr);
        break;
    case TF_OBJ_TYPE_STR:
        printf("%s", o->str.ptr);
        break;
    case TF_OBJ_TYPE_BOOL:
        printf("%s", o->b ? "true" : "false");
        break;
    case TF_OBJ_TYPE_VARFETCH:
        printf("$%s", o->str.ptr);
        break;
    case TF_OBJ_TYPE_VARLIST:
        printf("|");
        for (size_t i = 0; i < o->list.len; i++) {
            tf_obj_print_value(o->list.elem[i]);
            if (i != o->list.len - 1) printf(" ");
        }
        printf("|");
        break;
    case TF_OBJ_TYPE_LIST:
        printf("[");
        for (size_t i = 0; i < o->list.len; i++) {
            tf_obj_print_value(o->list.elem[i]);
            if (i != o->list.len - 1) printf(" ");
        }
        printf("]");
        break;
    case TF_OBJ_TYPE_MAP:
        printf("{");
        for (size_t i = 0; i < o->map.len; i++) {
            if (i > 0) printf(" ");
            tf_obj_print_value(o->map.entries[i].key);
            printf(" ");
            tf_obj_print_value(o->map.entries[i].value);
        }
        printf("}");
        break;
    case TF_OBJ_TYPE_SET:
        printf("#{");
        for (size_t i = 0; i < o->set.len; i++) {
            if (i > 0) printf(" ");
            tf_obj_print_value(o->set.entries[i].item);
        }
        printf("}");
        break;
    default:
        printf("?");
    }
}

// Source printer: reconstructs objects in a source-like form for words like
// `see`
void tf_obj_print_source(tf_obj *o) {
    switch (o->type) {
    case TF_OBJ_TYPE_INT:
        printf("%d", o->i);
        break;
    case TF_OBJ_TYPE_FLOAT:
        printf("%g", o->f);
        break;
    case TF_OBJ_TYPE_SYMBOL:
        if (o->str.quoted) printf("'");
        printf("%s", o->str.ptr);
        break;
    case TF_OBJ_TYPE_STR:
        printf("\"");
        print_escaped_string(o->str.ptr, o->str.len);
        printf("\"");
        break;
    case TF_OBJ_TYPE_BOOL:
        printf("%s", o->b ? "true" : "false");
        break;
    case TF_OBJ_TYPE_VARFETCH:
        printf("$%s", o->str.ptr);
        break;
    case TF_OBJ_TYPE_VARLIST:
        printf("|");
        for (size_t i = 0; i < o->list.len; i++) {
            tf_obj_print_source(o->list.elem[i]);
            if (i + 1 < o->list.len) printf(" ");
        }
        printf("|");
        break;
    case TF_OBJ_TYPE_LIST:
        printf("[");
        for (size_t i = 0; i < o->list.len; i++) {
            tf_obj_print_source(o->list.elem[i]);
            if (i + 1 < o->list.len) printf(" ");
        }
        printf("]");
        break;
    case TF_OBJ_TYPE_MAP:
        printf("{");
        for (size_t i = 0; i < o->map.len; i++) {
            if (i > 0) printf(" ");
            tf_obj_print_source(o->map.entries[i].key);
            printf(" ");
            tf_obj_print_source(o->map.entries[i].value);
        }
        printf("}");
        break;
    case TF_OBJ_TYPE_SET:
        printf("#{");
        for (size_t i = 0; i < o->set.len; i++) {
            if (i > 0) printf(" ");
            tf_obj_print_source(o->set.entries[i].item);
        }
        printf("}");
        break;
    default:
        printf("?");
        break;
    }
}
