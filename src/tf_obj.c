#include "tf_obj.h"
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tf_alloc.h"

#define TF_MAP_INITIAL_CAP 4
#define TF_MAP_INITIAL_BUCKET_CAP 8
#define TF_SET_INITIAL_CAP 4
#define TF_SET_INITIAL_BUCKET_CAP 8
#define TF_DEQUE_INITIAL_CAP 4
#define TF_PQUEUE_INITIAL_CAP 4
#define TF_OBJ_CACHE_LIMIT 256

struct tf_source_file {
    int refcount;
    char *filename;
};

typedef struct tf_obj_cache_node {
    struct tf_obj_cache_node *next;
} tf_obj_cache_node;

static tf_obj_cache_node *obj_cache = NULL;
static size_t obj_cache_len = 0;

static tf_obj *obj_storage_acquire(void) {
    if (!obj_cache) return tf_xmalloc(sizeof(tf_obj));
    tf_obj_cache_node *node = obj_cache;
    obj_cache = node->next;
    obj_cache_len--;
    return (tf_obj *)node;
}

static void obj_storage_release(tf_obj *o) {
    if (obj_cache_len >= TF_OBJ_CACHE_LIMIT) {
        free(o);
        return;
    }
    tf_obj_cache_node *node = (tf_obj_cache_node *)o;
    node->next = obj_cache;
    obj_cache = node;
    obj_cache_len++;
}

static void string_set_heap_capacity(tf_obj *s, size_t capacity) {
    /* Heap strings do not use inline_buf; memcpy also avoids alignment assumptions. */
    memcpy(s->str.inline_buf, &capacity, sizeof(capacity));
}

static size_t string_heap_capacity(tf_obj *s) {
    size_t capacity = 0;
    memcpy(&capacity, s->str.inline_buf, sizeof(capacity));
    return capacity;
}

/* === Object Creation === */

tf_obj *tf_obj_new(int type) {
    tf_obj *o = obj_storage_acquire();
    o->type = type;
    o->refcount = 1;
    o->span = (tf_source_span){0};
    return o;
}

tf_obj *tf_obj_new_vector(void) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_VECTOR);
    o->vector.len = 0;
    o->vector.cap = TF_VECTOR_INLINE_CAP;
    o->vector.elem = o->vector.inline_elem;
    return o;
}

tf_obj *tf_obj_new_vector_with_capacity(size_t capacity) {
    tf_obj *o = tf_obj_new_vector();
    tf_vector_reserve(o, capacity);
    return o;
}

tf_obj *tf_obj_new_list(void) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_LIST);
    o->list.head = NULL;
    o->list.len = 0;
    return o;
}

tf_obj *tf_obj_new_map(void) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_MAP);
    o->map.len = 0;
    o->map.cap = 0;
    o->map.entries = NULL;
    o->map.bucket_cap = 0;
    o->map.buckets = NULL;
    return o;
}

tf_obj *tf_obj_new_set(void) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_SET);
    o->set.len = 0;
    o->set.cap = 0;
    o->set.entries = NULL;
    o->set.bucket_cap = 0;
    o->set.buckets = NULL;
    return o;
}

tf_obj *tf_obj_new_deque(void) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_DEQUE);
    o->deque.len = 0;
    o->deque.cap = 0;
    o->deque.head = 0;
    o->deque.elem = NULL;
    return o;
}

tf_obj *tf_obj_new_pqueue(void) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_PQUEUE);
    o->pqueue.len = 0;
    o->pqueue.cap = 0;
    o->pqueue.next_order = 0;
    o->pqueue.entries = NULL;
    return o;
}

tf_obj *tf_obj_new_int(int64_t i) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_INT);
    o->i = i;
    return o;
}

tf_obj *tf_obj_new_bool(bool b) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_BOOL);
    o->b = b;
    return o;
}

tf_obj *tf_obj_new_float(double f) {
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
    tf_obj *o = tf_obj_new_string_uninitialized(len);
    memcpy(o->str.ptr, s, len);
    return o;
}

tf_obj *tf_obj_new_string_uninitialized(size_t len) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_STR);
    o->str.len = len;
    o->str.quoted = false;
    o->str.ptr = len <= TF_STRING_INLINE_CAP
                     ? o->str.inline_buf
                     : tf_xmalloc(len + 1);
    if (o->str.ptr != o->str.inline_buf) string_set_heap_capacity(o, len);
    o->str.ptr[len] = 0;
    return o;
}

tf_obj *tf_obj_new_string_take(char *s, size_t len) {
    tf_obj *o = tf_obj_new(TF_OBJ_TYPE_STR);
    o->str.len = len;
    o->str.quoted = false;
    if (len <= TF_STRING_INLINE_CAP) {
        o->str.ptr = o->str.inline_buf;
        if (len > 0) memcpy(o->str.ptr, s, len);
        o->str.ptr[len] = 0;
        free(s);
    } else {
        o->str.ptr = s;
        string_set_heap_capacity(o, len);
        o->str.ptr[len] = 0;
    }
    return o;
}

tf_source_file *tf_source_file_new(const char *filename) {
    tf_source_file *source = tf_xmalloc(sizeof(*source));
    source->refcount = 1;
    source->filename = tf_xstrdup(filename ? filename : "<unknown>");
    return source;
}

void tf_source_file_retain(tf_source_file *source) {
    if (source) source->refcount++;
}

void tf_source_file_release(tf_source_file *source) {
    if (!source) return;
    assert(source->refcount > 0);
    source->refcount--;
    if (source->refcount != 0) return;
    free(source->filename);
    free(source);
}

const char *tf_source_file_name(tf_source_file *source) {
    return source ? source->filename : NULL;
}

void tf_obj_set_span(tf_obj *o, tf_source_span span) {
    if (!o) return;
    tf_source_file_retain(span.source);
    tf_source_file_release(o->span.source);
    o->span = span;
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

int tf_obj_compare_number(tf_obj *a, tf_obj *b) {
    if (a->type == TF_OBJ_TYPE_INT && b->type == TF_OBJ_TYPE_INT) {
        return (a->i > b->i) - (a->i < b->i);
    }
    if (a->type == TF_OBJ_TYPE_FLOAT && b->type == TF_OBJ_TYPE_FLOAT) {
        return (a->f > b->f) - (a->f < b->f);
    }

    tf_obj *int_obj = a->type == TF_OBJ_TYPE_INT ? a : b;
    tf_obj *float_obj = a->type == TF_OBJ_TYPE_FLOAT ? a : b;
    double value = float_obj->f;
    if (isnan(value)) return 0;
    int order = 0;
    if (value >= 9223372036854775808.0) {
        order = -1;
    } else if (value < -9223372036854775808.0) {
        order = 1;
    } else {
        double truncated = trunc(value);
        int64_t converted = (int64_t)truncated;
        if (int_obj->i < converted)
            order = -1;
        else if (int_obj->i > converted)
            order = 1;
        else if (value > truncated)
            order = -1;
        else if (value < truncated)
            order = 1;
    }
    return int_obj == a ? order : -order;
}

void tf_format_double(char *buf, size_t size, double value) {
    if (!isfinite(value)) {
        snprintf(buf, size, "%.17g", value);
        return;
    }

    double magnitude = fabs(value);
    if (magnitude == 0.0 || (magnitude >= 1e-4 && magnitude < 1e16)) {
        for (int decimals = 0; decimals <= 17; decimals++) {
            snprintf(buf, size, "%.*f", decimals, value);
            char *end = NULL;
            double parsed = strtod(buf, &end);
            if (end && *end == '\0' &&
                memcmp(&parsed, &value, sizeof value) == 0) {
                return;
            }
        }
    }

    for (int precision = 1; precision <= 17; precision++) {
        snprintf(buf, size, "%.*g", precision, value);
        char *end = NULL;
        double parsed = strtod(buf, &end);
        if (end && *end == '\0' &&
            memcmp(&parsed, &value, sizeof value) == 0) {
            return;
        }
    }
}

void tf_string_reserve(tf_obj *s, size_t capacity) {
    assert(s->type == TF_OBJ_TYPE_STR);
    bool is_inline = s->str.ptr == s->str.inline_buf;
    size_t current_cap = is_inline ? TF_STRING_INLINE_CAP
                                   : string_heap_capacity(s);
    if (capacity <= current_cap) return;

    size_t new_cap = current_cap;
    while (new_cap < capacity) {
        if (new_cap > SIZE_MAX / 2) {
            new_cap = capacity;
            break;
        }
        new_cap *= 2;
    }

    if (is_inline) {
        char *new_ptr = tf_xmalloc(new_cap + 1);
        memcpy(new_ptr, s->str.inline_buf, s->str.len + 1);
        s->str.ptr = new_ptr;
    } else {
        s->str.ptr = tf_xrealloc(s->str.ptr, new_cap + 1);
    }
    string_set_heap_capacity(s, new_cap);
}

void tf_vector_reserve(tf_obj *v, size_t capacity) {
    if (capacity <= v->vector.cap) return;

    size_t new_cap = v->vector.cap;
    while (new_cap < capacity) new_cap *= 2;

    if (v->vector.elem == v->vector.inline_elem) {
        tf_obj **new_elem = tf_xmalloc(sizeof(tf_obj *) * new_cap);
        memcpy(new_elem, v->vector.inline_elem,
               sizeof(tf_obj *) * v->vector.len);
        v->vector.elem = new_elem;
    } else {
        v->vector.elem =
            tf_xrealloc(v->vector.elem, sizeof(tf_obj *) * new_cap);
    }
    v->vector.cap = new_cap;
}

void tf_vector_push(tf_obj *v, tf_obj *elem) {
    tf_vector_reserve(v, v->vector.len + 1);
    v->vector.elem[v->vector.len++] = elem;
}

// pop object only if the type is correct
tf_obj *tf_vector_pop_type(tf_obj *v, tf_type type) {
    if (v->vector.len == 0) return NULL;
    tf_obj *o = v->vector.elem[v->vector.len - 1];
    if (o->type != type) return NULL;
    return tf_vector_pop(v);
}

tf_obj *tf_vector_pop(tf_obj *v) {
    if (v->vector.len == 0) return NULL;
    /* Stack pops retain capacity so removal never reallocates. */
    return v->vector.elem[--v->vector.len];
}

tf_obj *tf_vector_clone(tf_obj *src) {
    tf_obj *result = tf_obj_new_vector_with_capacity(src->vector.len);
    for (size_t i = 0; i < src->vector.len; i++) {
        tf_obj *elem = src->vector.elem[i];
        tf_obj_retain(elem);
        tf_vector_push(result, elem);
    }
    return result;
}

static void list_node_retain(tf_list_node *node) {
    if (node) node->refcount++;
}

static void list_node_release(tf_list_node *node) {
    while (node) {
        assert(node->refcount > 0);
        node->refcount--;
        if (node->refcount != 0) return;

        tf_list_node *next = node->next;
        tf_obj_release(node->value);
        free(node);
        node = next;
    }
}

static tf_list_node *list_node_new(tf_obj *value, tf_list_node *next) {
    tf_list_node *node = tf_xmalloc(sizeof(tf_list_node));
    node->refcount = 1;
    node->value = value;
    node->next = next;
    tf_obj_retain(value);
    list_node_retain(next);
    return node;
}

void tf_list_builder_init(tf_list_builder *builder) {
    builder->head = NULL;
    builder->next = &builder->head;
    builder->len = 0;
}

void tf_list_builder_push_owned(tf_list_builder *builder, tf_obj *item) {
    tf_list_node *node = tf_xmalloc(sizeof(tf_list_node));
    node->refcount = 1;
    node->value = item;
    node->next = NULL;
    *builder->next = node;
    builder->next = &node->next;
    builder->len++;
}

tf_obj *tf_list_builder_finish(tf_list_builder *builder) {
    tf_obj *result = tf_obj_new_list();
    result->list.head = builder->head;
    result->list.len = builder->len;
    tf_list_builder_init(builder);
    return result;
}

void tf_list_builder_cleanup(tf_list_builder *builder) {
    list_node_release(builder->head);
    tf_list_builder_init(builder);
}

static tf_list_node *list_copy_prefix_nodes(tf_list_node *source, size_t count,
                                            tf_list_node *tail) {
    tf_list_node *head = NULL;
    tf_list_node **next = &head;

    for (size_t i = 0; i < count && source; i++, source = source->next) {
        tf_list_node *copy = list_node_new(source->value, NULL);
        *next = copy;
        next = &copy->next;
    }

    if (tail) {
        list_node_retain(tail);
        *next = tail;
    }
    return head;
}

tf_obj *tf_list_from_vector(tf_obj *vector) {
    tf_list_builder builder;
    tf_list_builder_init(&builder);
    for (size_t i = 0; i < vector->vector.len; i++) {
        tf_obj *item = vector->vector.elem[i];
        tf_obj_retain(item);
        tf_list_builder_push_owned(&builder, item);
    }
    return tf_list_builder_finish(&builder);
}

tf_obj *tf_list_cons_obj(tf_obj *head, tf_obj *tail) {
    tf_obj *result = tf_obj_new_list();
    result->list.head = list_node_new(head, tail->list.head);
    result->list.len = tail->list.len + 1;
    return result;
}

tf_obj *tf_list_rest_obj(tf_obj *list) {
    tf_obj *result = tf_obj_new_list();
    if (list->list.head) {
        result->list.head = list->list.head->next;
        result->list.len = list->list.len - 1;
        list_node_retain(result->list.head);
    }
    return result;
}

tf_obj *tf_list_take_obj(tf_obj *list, size_t count) {
    if (count > list->list.len) count = list->list.len;
    tf_obj *result = tf_obj_new_list();
    result->list.head = list_copy_prefix_nodes(list->list.head, count, NULL);
    result->list.len = count;
    return result;
}

tf_obj *tf_list_drop_obj(tf_obj *list, size_t count) {
    if (count > list->list.len) count = list->list.len;
    tf_obj *result = tf_obj_new_list();
    tf_list_node *node = list->list.head;
    for (size_t i = 0; i < count; i++) node = node->next;
    result->list.head = node;
    result->list.len = list->list.len - count;
    list_node_retain(node);
    return result;
}

tf_obj *tf_list_concat_obj(tf_obj *left, tf_obj *right) {
    tf_obj *result = tf_obj_new_list();
    result->list.head = list_copy_prefix_nodes(
        left->list.head, left->list.len, right->list.head);
    result->list.len = left->list.len + right->list.len;
    return result;
}

tf_obj *tf_list_push_back_obj(tf_obj *list, tf_obj *item) {
    tf_list_node *tail = list_node_new(item, NULL);
    tf_obj *result = tf_obj_new_list();
    result->list.head =
        list_copy_prefix_nodes(list->list.head, list->list.len, tail);
    result->list.len = list->list.len + 1;
    list_node_release(tail);
    return result;
}

tf_obj *tf_list_reverse_obj(tf_obj *list) {
    tf_obj *result = tf_obj_new_list();
    for (tf_list_node *node = list->list.head; node; node = node->next) {
        tf_list_node *old_head = result->list.head;
        result->list.head = list_node_new(node->value, old_head);
        result->list.len++;
        list_node_release(old_head);
    }
    return result;
}

tf_obj *tf_list_get(tf_obj *list, size_t idx) {
    if (!list || list->type != TF_OBJ_TYPE_LIST || idx >= list->list.len) {
        return NULL;
    }
    tf_list_node *node = list->list.head;
    for (size_t i = 0; i < idx; i++) node = node->next;
    return node->value;
}

void tf_sequence_iter_init(tf_sequence_iter *iter, tf_obj *sequence) {
    iter->sequence = sequence;
    iter->index = 0;
    iter->node = sequence->type == TF_OBJ_TYPE_LIST ? sequence->list.head : NULL;
}

tf_obj *tf_sequence_iter_next_owned(tf_sequence_iter *iter) {
    tf_obj *sequence = iter->sequence;
    if (sequence->type == TF_OBJ_TYPE_VECTOR) {
        if (iter->index >= sequence->vector.len) return NULL;
        tf_obj *item = sequence->vector.elem[iter->index++];
        tf_obj_retain(item);
        return item;
    }
    if (sequence->type == TF_OBJ_TYPE_LIST) {
        if (!iter->node) return NULL;
        tf_obj *item = iter->node->value;
        iter->node = iter->node->next;
        iter->index++;
        tf_obj_retain(item);
        return item;
    }
    if (sequence->type == TF_OBJ_TYPE_STR) {
        if (iter->index >= sequence->str.len) return NULL;
        return tf_obj_new_string(sequence->str.ptr + iter->index++, 1);
    }
    return NULL;
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
        return hash_mix_u64(h, (uint64_t)o->i);
    case TF_OBJ_TYPE_STR:
    case TF_OBJ_TYPE_SYMBOL:
        return fnv1a_bytes(o->str.ptr, o->str.len, h);
    default:
        return h;
    }
}

static size_t map_find_slot(tf_obj *map, tf_obj *key, uint64_t hash,
                            bool *found) {
    assert(map->map.bucket_cap > 0);
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
    assert(set->set.bucket_cap > 0);
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
    if (bucket_cap != map->map.bucket_cap) {
        free(map->map.buckets);
        map->map.bucket_cap = bucket_cap;
        map->map.buckets = tf_xcalloc(bucket_cap, sizeof(size_t));
    } else {
        memset(map->map.buckets, 0, bucket_cap * sizeof(size_t));
    }
    for (size_t i = 0; i < map->map.len; i++) {
        bool found = false;
        size_t slot = map_find_slot(map, map->map.entries[i].key,
                                    map->map.entries[i].hash, &found);
        map->map.buckets[slot] = i + 1;
    }
}

static void set_rebuild_buckets(tf_obj *set, size_t bucket_cap) {
    if (bucket_cap != set->set.bucket_cap) {
        free(set->set.buckets);
        set->set.bucket_cap = bucket_cap;
        set->set.buckets = tf_xcalloc(bucket_cap, sizeof(size_t));
    } else {
        memset(set->set.buckets, 0, bucket_cap * sizeof(size_t));
    }
    for (size_t i = 0; i < set->set.len; i++) {
        bool found = false;
        size_t slot = set_find_slot(set, set->set.entries[i].item,
                                    set->set.entries[i].hash, &found);
        set->set.buckets[slot] = i + 1;
    }
}

void tf_map_reserve(tf_obj *map, size_t capacity) {
    assert(map->type == TF_OBJ_TYPE_MAP);
    if (capacity == 0) return;

    if (capacity > map->map.cap) {
        size_t new_cap = map->map.cap ? map->map.cap : TF_MAP_INITIAL_CAP;
        while (new_cap < capacity) new_cap *= 2;
        map->map.entries = tf_xrealloc(map->map.entries,
                                       sizeof(tf_map_entry) * new_cap);
        map->map.cap = new_cap;
    }

    size_t bucket_cap = map->map.bucket_cap
                            ? map->map.bucket_cap
                            : TF_MAP_INITIAL_BUCKET_CAP;
    while (bucket_cap / 2 < capacity) bucket_cap *= 2;
    if (bucket_cap != map->map.bucket_cap) {
        map_rebuild_buckets(map, bucket_cap);
    }
}

tf_obj *tf_map_clone(tf_obj *map) {
    assert(map->type == TF_OBJ_TYPE_MAP);
    tf_obj *result = tf_obj_new_map();
    result->map.len = map->map.len;
    result->map.cap = map->map.cap;
    result->map.bucket_cap = map->map.bucket_cap;

    if (map->map.cap > 0) {
        result->map.entries =
            tf_xmalloc(sizeof(tf_map_entry) * map->map.cap);
        memcpy(result->map.entries, map->map.entries,
               sizeof(tf_map_entry) * map->map.len);
    }
    if (map->map.bucket_cap > 0) {
        result->map.buckets =
            tf_xmalloc(sizeof(size_t) * map->map.bucket_cap);
        memcpy(result->map.buckets, map->map.buckets,
               sizeof(size_t) * map->map.bucket_cap);
    }

    for (size_t i = 0; i < map->map.len; i++) {
        tf_obj_retain(result->map.entries[i].key);
        tf_obj_retain(result->map.entries[i].value);
    }
    return result;
}

bool tf_map_has(tf_obj *map, tf_obj *key) {
    if (!map || map->type != TF_OBJ_TYPE_MAP || !tf_obj_hashable(key)) {
        return false;
    }
    if (map->map.len == 0) return false;
    bool found = false;
    (void)map_find_slot(map, key, tf_obj_hash(key), &found);
    return found;
}

bool tf_map_get(tf_obj *map, tf_obj *key, tf_obj **out) {
    if (!map || map->type != TF_OBJ_TYPE_MAP || !tf_obj_hashable(key)) {
        return false;
    }
    if (map->map.len == 0) return false;
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

    uint64_t hash = tf_obj_hash(key);
    bool found = false;
    size_t slot = 0;
    if (map->map.bucket_cap > 0) {
        slot = map_find_slot(map, key, hash, &found);
    }
    if (found) {
        tf_map_entry *entry = &map->map.entries[map->map.buckets[slot] - 1];
        tf_obj_retain(value);
        tf_obj_release(entry->value);
        entry->value = value;
        return true;
    }

    size_t old_bucket_cap = map->map.bucket_cap;
    tf_map_reserve(map, map->map.len + 1);
    if (map->map.bucket_cap != old_bucket_cap) {
        slot = map_find_slot(map, key, hash, &found);
    }
    size_t entry_idx = map->map.len++;
    map->map.entries[entry_idx] = (tf_map_entry){key, value, hash};
    tf_obj_retain(key);
    tf_obj_retain(value);
    map->map.buckets[slot] = entry_idx + 1;
    return true;
}

bool tf_map_remove(tf_obj *map, tf_obj *key) {
    if (!map || map->type != TF_OBJ_TYPE_MAP || !tf_obj_hashable(key) ||
        map->map.len == 0) {
        return false;
    }

    bool found = false;
    size_t slot = map_find_slot(map, key, tf_obj_hash(key), &found);
    if (!found) return false;

    size_t entry_idx = map->map.buckets[slot] - 1;
    tf_obj_release(map->map.entries[entry_idx].key);
    tf_obj_release(map->map.entries[entry_idx].value);
    if (entry_idx + 1 < map->map.len) {
        memmove(&map->map.entries[entry_idx],
                &map->map.entries[entry_idx + 1],
                sizeof(tf_map_entry) * (map->map.len - entry_idx - 1));
    }
    map->map.len--;
    map_rebuild_buckets(map, map->map.bucket_cap);
    return true;
}

void tf_set_reserve(tf_obj *set, size_t capacity) {
    assert(set->type == TF_OBJ_TYPE_SET);
    if (capacity == 0) return;

    if (capacity > set->set.cap) {
        size_t new_cap = set->set.cap ? set->set.cap : TF_SET_INITIAL_CAP;
        while (new_cap < capacity) new_cap *= 2;
        set->set.entries = tf_xrealloc(set->set.entries,
                                       sizeof(tf_set_entry) * new_cap);
        set->set.cap = new_cap;
    }

    size_t bucket_cap = set->set.bucket_cap
                            ? set->set.bucket_cap
                            : TF_SET_INITIAL_BUCKET_CAP;
    while (bucket_cap / 2 < capacity) bucket_cap *= 2;
    if (bucket_cap != set->set.bucket_cap) {
        set_rebuild_buckets(set, bucket_cap);
    }
}

tf_obj *tf_set_clone(tf_obj *set) {
    assert(set->type == TF_OBJ_TYPE_SET);
    tf_obj *result = tf_obj_new_set();
    result->set.len = set->set.len;
    result->set.cap = set->set.cap;
    result->set.bucket_cap = set->set.bucket_cap;

    if (set->set.cap > 0) {
        result->set.entries =
            tf_xmalloc(sizeof(tf_set_entry) * set->set.cap);
        memcpy(result->set.entries, set->set.entries,
               sizeof(tf_set_entry) * set->set.len);
    }
    if (set->set.bucket_cap > 0) {
        result->set.buckets =
            tf_xmalloc(sizeof(size_t) * set->set.bucket_cap);
        memcpy(result->set.buckets, set->set.buckets,
               sizeof(size_t) * set->set.bucket_cap);
    }

    for (size_t i = 0; i < set->set.len; i++) {
        tf_obj_retain(result->set.entries[i].item);
    }
    return result;
}

bool tf_set_has(tf_obj *set, tf_obj *item) {
    if (!set || set->type != TF_OBJ_TYPE_SET || !tf_obj_hashable(item)) {
        return false;
    }
    if (set->set.len == 0) return false;
    bool found = false;
    (void)set_find_slot(set, item, tf_obj_hash(item), &found);
    return found;
}

bool tf_set_add(tf_obj *set, tf_obj *item) {
    if (!set || set->type != TF_OBJ_TYPE_SET || !tf_obj_hashable(item)) {
        return false;
    }

    uint64_t hash = tf_obj_hash(item);
    bool found = false;
    size_t slot = 0;
    if (set->set.bucket_cap > 0) {
        slot = set_find_slot(set, item, hash, &found);
    }
    if (found) return true;

    size_t old_bucket_cap = set->set.bucket_cap;
    tf_set_reserve(set, set->set.len + 1);
    if (set->set.bucket_cap != old_bucket_cap) {
        slot = set_find_slot(set, item, hash, &found);
    }
    size_t entry_idx = set->set.len++;
    set->set.entries[entry_idx] = (tf_set_entry){item, hash};
    tf_obj_retain(item);
    set->set.buckets[slot] = entry_idx + 1;
    return true;
}

bool tf_set_remove(tf_obj *set, tf_obj *item) {
    if (!set || set->type != TF_OBJ_TYPE_SET || !tf_obj_hashable(item) ||
        set->set.len == 0) {
        return false;
    }

    bool found = false;
    size_t slot = set_find_slot(set, item, tf_obj_hash(item), &found);
    if (!found) return false;

    size_t entry_idx = set->set.buckets[slot] - 1;
    tf_obj_release(set->set.entries[entry_idx].item);
    if (entry_idx + 1 < set->set.len) {
        memmove(&set->set.entries[entry_idx],
                &set->set.entries[entry_idx + 1],
                sizeof(tf_set_entry) * (set->set.len - entry_idx - 1));
    }
    set->set.len--;
    set_rebuild_buckets(set, set->set.bucket_cap);
    return true;
}

static size_t deque_slot(tf_obj *deque, size_t idx) {
    return (deque->deque.head + idx) % deque->deque.cap;
}

static void deque_ensure_capacity(tf_obj *deque, size_t needed) {
    if (needed <= deque->deque.cap) return;

    size_t new_cap = deque->deque.cap ? deque->deque.cap : TF_DEQUE_INITIAL_CAP;
    while (new_cap < needed) new_cap *= 2;

    tf_obj **new_elem = tf_xmalloc(sizeof(tf_obj *) * new_cap);
    for (size_t i = 0; i < deque->deque.len; i++) {
        new_elem[i] = deque->deque.elem[deque_slot(deque, i)];
    }
    free(deque->deque.elem);
    deque->deque.elem = new_elem;
    deque->deque.cap = new_cap;
    deque->deque.head = 0;
}

void tf_deque_reserve(tf_obj *deque, size_t capacity) {
    assert(deque->type == TF_OBJ_TYPE_DEQUE);
    deque_ensure_capacity(deque, capacity);
}

tf_obj *tf_deque_clone(tf_obj *src) {
    tf_obj *result = tf_obj_new_deque();
    deque_ensure_capacity(result, src->deque.cap);
    result->deque.len = src->deque.len;
    for (size_t i = 0; i < src->deque.len; i++) {
        tf_obj *item = tf_deque_get(src, i);
        result->deque.elem[i] = item;
        tf_obj_retain(item);
    }
    return result;
}

tf_obj *tf_deque_get(tf_obj *deque, size_t idx) {
    if (!deque || deque->type != TF_OBJ_TYPE_DEQUE || idx >= deque->deque.len) {
        return NULL;
    }
    return deque->deque.elem[deque_slot(deque, idx)];
}

void tf_deque_push_front(tf_obj *deque, tf_obj *value) {
    deque_ensure_capacity(deque, deque->deque.len + 1);
    deque->deque.head =
        (deque->deque.head + deque->deque.cap - 1) % deque->deque.cap;
    deque->deque.elem[deque->deque.head] = value;
    deque->deque.len++;
    tf_obj_retain(value);
}

void tf_deque_push_back(tf_obj *deque, tf_obj *value) {
    deque_ensure_capacity(deque, deque->deque.len + 1);
    deque->deque.elem[deque_slot(deque, deque->deque.len)] = value;
    deque->deque.len++;
    tf_obj_retain(value);
}

tf_obj *tf_deque_pop_front(tf_obj *deque) {
    if (!deque || deque->type != TF_OBJ_TYPE_DEQUE || deque->deque.len == 0) {
        return NULL;
    }

    tf_obj *value = deque->deque.elem[deque->deque.head];
    deque->deque.head = (deque->deque.head + 1) % deque->deque.cap;
    deque->deque.len--;
    if (deque->deque.len == 0) deque->deque.head = 0;
    return value;
}

tf_obj *tf_deque_pop_back(tf_obj *deque) {
    if (!deque || deque->type != TF_OBJ_TYPE_DEQUE || deque->deque.len == 0) {
        return NULL;
    }

    size_t idx = deque_slot(deque, deque->deque.len - 1);
    tf_obj *value = deque->deque.elem[idx];
    deque->deque.len--;
    if (deque->deque.len == 0) deque->deque.head = 0;
    return value;
}

static bool pqueue_entry_less(tf_pqueue_entry a, tf_pqueue_entry b) {
    int order = tf_obj_compare_number(a.priority, b.priority);
    if (order < 0) return true;
    if (order > 0) return false;
    return a.order < b.order;
}

static void pqueue_swap(tf_pqueue_entry *a, tf_pqueue_entry *b) {
    tf_pqueue_entry tmp = *a;
    *a = *b;
    *b = tmp;
}

static void pqueue_sift_up(tf_obj *pqueue, size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (!pqueue_entry_less(pqueue->pqueue.entries[idx],
                               pqueue->pqueue.entries[parent])) {
            break;
        }
        pqueue_swap(&pqueue->pqueue.entries[idx],
                    &pqueue->pqueue.entries[parent]);
        idx = parent;
    }
}

static void pqueue_sift_down(tf_obj *pqueue, size_t idx) {
    while (true) {
        size_t left = idx * 2 + 1;
        size_t right = left + 1;
        size_t smallest = idx;

        if (left < pqueue->pqueue.len &&
            pqueue_entry_less(pqueue->pqueue.entries[left],
                              pqueue->pqueue.entries[smallest])) {
            smallest = left;
        }
        if (right < pqueue->pqueue.len &&
            pqueue_entry_less(pqueue->pqueue.entries[right],
                              pqueue->pqueue.entries[smallest])) {
            smallest = right;
        }
        if (smallest == idx) break;

        pqueue_swap(&pqueue->pqueue.entries[idx],
                    &pqueue->pqueue.entries[smallest]);
        idx = smallest;
    }
}

static void pqueue_ensure_capacity(tf_obj *pqueue, size_t needed) {
    if (needed <= pqueue->pqueue.cap) return;

    if (pqueue->pqueue.cap == 0) pqueue->pqueue.cap = TF_PQUEUE_INITIAL_CAP;
    while (pqueue->pqueue.cap < needed) pqueue->pqueue.cap *= 2;
    pqueue->pqueue.entries =
        tf_xrealloc(pqueue->pqueue.entries,
                    sizeof(tf_pqueue_entry) * pqueue->pqueue.cap);
}

void tf_pqueue_reserve(tf_obj *pqueue, size_t capacity) {
    assert(pqueue->type == TF_OBJ_TYPE_PQUEUE);
    pqueue_ensure_capacity(pqueue, capacity);
}

tf_obj *tf_pqueue_clone(tf_obj *src) {
    tf_obj *result = tf_obj_new_pqueue();
    pqueue_ensure_capacity(result, src->pqueue.cap);
    result->pqueue.len = src->pqueue.len;
    result->pqueue.next_order = src->pqueue.next_order;
    for (size_t i = 0; i < src->pqueue.len; i++) {
        result->pqueue.entries[i] = src->pqueue.entries[i];
        tf_obj_retain(result->pqueue.entries[i].priority);
        tf_obj_retain(result->pqueue.entries[i].value);
    }
    return result;
}

void tf_pqueue_append(tf_obj *pqueue, tf_obj *priority, tf_obj *value) {
    pqueue_ensure_capacity(pqueue, pqueue->pqueue.len + 1);
    size_t idx = pqueue->pqueue.len++;
    pqueue->pqueue.entries[idx] =
        (tf_pqueue_entry){priority, pqueue->pqueue.next_order++, value};
    tf_obj_retain(priority);
    tf_obj_retain(value);
}

void tf_pqueue_heapify(tf_obj *pqueue) {
    assert(pqueue->type == TF_OBJ_TYPE_PQUEUE);
    for (size_t i = pqueue->pqueue.len / 2; i > 0; i--) {
        pqueue_sift_down(pqueue, i - 1);
    }
}

void tf_pqueue_push(tf_obj *pqueue, tf_obj *priority, tf_obj *value) {
    tf_pqueue_append(pqueue, priority, value);
    pqueue_sift_up(pqueue, pqueue->pqueue.len - 1);
}

bool tf_pqueue_peek(tf_obj *pqueue, tf_obj **priority, tf_obj **value) {
    if (!pqueue || pqueue->type != TF_OBJ_TYPE_PQUEUE ||
        pqueue->pqueue.len == 0) {
        return false;
    }

    if (priority) *priority = pqueue->pqueue.entries[0].priority;
    if (value) *value = pqueue->pqueue.entries[0].value;
    return true;
}

bool tf_pqueue_pop(tf_obj *pqueue, tf_obj **priority, tf_obj **value) {
    if (!pqueue || pqueue->type != TF_OBJ_TYPE_PQUEUE ||
        pqueue->pqueue.len == 0) {
        return false;
    }

    tf_pqueue_entry root = pqueue->pqueue.entries[0];
    pqueue->pqueue.len--;
    if (pqueue->pqueue.len > 0) {
        pqueue->pqueue.entries[0] = pqueue->pqueue.entries[pqueue->pqueue.len];
        pqueue_sift_down(pqueue, 0);
    }

    if (priority) {
        *priority = root.priority;
    } else {
        tf_obj_release(root.priority);
    }
    if (value) {
        *value = root.value;
    } else {
        tf_obj_release(root.value);
    }
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
    case TF_OBJ_TYPE_VECTOR:
    case TF_OBJ_TYPE_VARLIST:
        if (a->vector.len != b->vector.len) return false;
        for (size_t i = 0; i < a->vector.len; i++) {
            if (!obj_equal_inner(a->vector.elem[i], b->vector.elem[i], depth + 1)) {
                return false;
            }
        }
        return true;
    case TF_OBJ_TYPE_LIST: {
        if (a->list.len != b->list.len) return false;
        tf_list_node *left = a->list.head;
        tf_list_node *right = b->list.head;
        while (left && right) {
            if (!obj_equal_inner(left->value, right->value, depth + 1)) {
                return false;
            }
            left = left->next;
            right = right->next;
        }
        return left == NULL && right == NULL;
    }
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
    case TF_OBJ_TYPE_DEQUE:
        if (a->deque.len != b->deque.len) return false;
        for (size_t i = 0; i < a->deque.len; i++) {
            if (!obj_equal_inner(tf_deque_get(a, i), tf_deque_get(b, i),
                                 depth + 1)) {
                return false;
            }
        }
        return true;
    case TF_OBJ_TYPE_PQUEUE: {
        if (a->pqueue.len != b->pqueue.len) return false;

        tf_obj *left = tf_pqueue_clone(a);
        tf_obj *right = tf_pqueue_clone(b);
        while (left->pqueue.len > 0) {
            tf_obj *left_priority = NULL;
            tf_obj *right_priority = NULL;
            tf_obj *left_value = NULL;
            tf_obj *right_value = NULL;
            bool left_ok = tf_pqueue_pop(left, &left_priority, &left_value);
            bool right_ok = tf_pqueue_pop(right, &right_priority, &right_value);
            bool same = left_ok && right_ok &&
                        tf_obj_compare_number(left_priority, right_priority) == 0 &&
                        obj_equal_inner(left_value, right_value, depth + 1);
            if (left_priority) tf_obj_release(left_priority);
            if (right_priority) tf_obj_release(right_priority);
            if (left_value) tf_obj_release(left_value);
            if (right_value) tf_obj_release(right_value);
            if (!same) {
                tf_obj_release(left);
                tf_obj_release(right);
                return false;
            }
        }
        tf_obj_release(left);
        tf_obj_release(right);
        return true;
    }
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
    tf_source_file_release(o->span.source);
    switch (o->type) {
    case TF_OBJ_TYPE_VARLIST:
    case TF_OBJ_TYPE_VECTOR:
        for (size_t i = 0; i < o->vector.len; i++) tf_obj_release(o->vector.elem[i]);
        if (o->vector.elem != o->vector.inline_elem) free(o->vector.elem);
        break;
    case TF_OBJ_TYPE_LIST:
        list_node_release(o->list.head);
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
    case TF_OBJ_TYPE_DEQUE:
        for (size_t i = 0; i < o->deque.len; i++) {
            tf_obj_release(tf_deque_get(o, i));
        }
        free(o->deque.elem);
        break;
    case TF_OBJ_TYPE_PQUEUE:
        for (size_t i = 0; i < o->pqueue.len; i++) {
            tf_obj_release(o->pqueue.entries[i].priority);
            tf_obj_release(o->pqueue.entries[i].value);
        }
        free(o->pqueue.entries);
        break;
    case TF_OBJ_TYPE_VARFETCH:
    case TF_OBJ_TYPE_SYMBOL:
    case TF_OBJ_TYPE_STR:
        if (o->str.ptr != o->str.inline_buf) free(o->str.ptr);
        break;
    default:
        break;
    }
    obj_storage_release(o);
}

void tf_obj_cache_clear(void) {
    while (obj_cache) {
        tf_obj_cache_node *next = obj_cache->next;
        free(obj_cache);
        obj_cache = next;
    }
    obj_cache_len = 0;
}

static void print_escaped_string(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
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
            if (c < 0x20 || c >= 0x7f) {
                printf("\\x%02x", c);
            } else {
                putchar(c);
            }
            break;
        }
    }
}

// Debug printer: includes type tags and is intended for developer inspection
void tf_obj_print(tf_obj *o, size_t *count) {
    (*count)++;
    switch (o->type) {
    case TF_OBJ_TYPE_INT:
        printf("(int:%" PRId64 ")", o->i);
        break;
    case TF_OBJ_TYPE_FLOAT: {
        char buf[64];
        tf_format_double(buf, sizeof buf, o->f);
        printf("(float:%s)", buf);
        break;
    }
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
        for (size_t i = 0; i < o->vector.len; i++) {
            tf_obj_print(o->vector.elem[i], count);
            if (i != o->vector.len - 1) printf(" ");
        }
        printf("|");
        break;
    case TF_OBJ_TYPE_VECTOR:
        (*count)--;
        printf("[");
        for (size_t i = 0; i < o->vector.len; i++) {
            tf_obj_print(o->vector.elem[i], count);
            if (i != o->vector.len - 1) {
                if (*count % 6 == 0)
                    printf("\n");
                else
                    printf(" ");
            }
        }
        printf("]");
        break;
    case TF_OBJ_TYPE_LIST: {
        (*count)--;
        printf("(");
        tf_list_node *node = o->list.head;
        while (node) {
            tf_obj_print(node->value, count);
            node = node->next;
            if (node) printf(" ");
        }
        printf(")");
        break;
    }
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
    case TF_OBJ_TYPE_DEQUE:
        (*count)--;
        printf("deque[");
        for (size_t i = 0; i < o->deque.len; i++) {
            if (i > 0) printf(" ");
            tf_obj_print(tf_deque_get(o, i), count);
        }
        printf("]");
        break;
    case TF_OBJ_TYPE_PQUEUE: {
        (*count)--;
        printf("pqueue[");
        tf_obj *tmp = tf_pqueue_clone(o);
        for (size_t i = 0; tmp->pqueue.len > 0; i++) {
            tf_obj *priority = NULL;
            tf_obj *value = NULL;
            tf_pqueue_pop(tmp, &priority, &value);
            if (i > 0) printf(" ");
            printf("[");
            tf_obj_print(priority, count);
            printf(" ");
            tf_obj_print(value, count);
            printf("]");
            tf_obj_release(priority);
            tf_obj_release(value);
        }
        tf_obj_release(tmp);
        printf("]");
        break;
    }
    default:
        printf("?");
    }
}

// Runtime printer: prints values the way user-facing words like `print` expect
void tf_obj_print_value(tf_obj *o) {
    switch (o->type) {
    case TF_OBJ_TYPE_INT:
        printf("%" PRId64, o->i);
        break;
    case TF_OBJ_TYPE_FLOAT: {
        char buf[64];
        tf_format_double(buf, sizeof buf, o->f);
        printf("%s", buf);
        break;
    }
    case TF_OBJ_TYPE_SYMBOL:
        printf("%s", o->str.ptr);
        break;
    case TF_OBJ_TYPE_STR:
        fwrite(o->str.ptr, 1, o->str.len, stdout);
        break;
    case TF_OBJ_TYPE_BOOL:
        printf("%s", o->b ? "true" : "false");
        break;
    case TF_OBJ_TYPE_VARFETCH:
        printf("$%s", o->str.ptr);
        break;
    case TF_OBJ_TYPE_VARLIST:
        printf("|");
        for (size_t i = 0; i < o->vector.len; i++) {
            tf_obj_print_value(o->vector.elem[i]);
            if (i != o->vector.len - 1) printf(" ");
        }
        printf("|");
        break;
    case TF_OBJ_TYPE_VECTOR:
        printf("[");
        for (size_t i = 0; i < o->vector.len; i++) {
            tf_obj_print_value(o->vector.elem[i]);
            if (i != o->vector.len - 1) printf(" ");
        }
        printf("]");
        break;
    case TF_OBJ_TYPE_LIST: {
        printf("(");
        tf_list_node *node = o->list.head;
        while (node) {
            tf_obj_print_value(node->value);
            node = node->next;
            if (node) printf(" ");
        }
        printf(")");
        break;
    }
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
    case TF_OBJ_TYPE_DEQUE:
        printf("deque[");
        for (size_t i = 0; i < o->deque.len; i++) {
            if (i > 0) printf(" ");
            tf_obj_print_value(tf_deque_get(o, i));
        }
        printf("]");
        break;
    case TF_OBJ_TYPE_PQUEUE: {
        printf("pqueue[");
        tf_obj *tmp = tf_pqueue_clone(o);
        for (size_t i = 0; tmp->pqueue.len > 0; i++) {
            tf_obj *priority = NULL;
            tf_obj *value = NULL;
            tf_pqueue_pop(tmp, &priority, &value);
            if (i > 0) printf(" ");
            printf("[");
            tf_obj_print_value(priority);
            printf(" ");
            tf_obj_print_value(value);
            printf("]");
            tf_obj_release(priority);
            tf_obj_release(value);
        }
        tf_obj_release(tmp);
        printf("]");
        break;
    }
    default:
        printf("?");
    }
}

// Source printer: reconstructs objects in a source-like form for words like
// `see`
static void print_source_like(tf_obj *o, bool display) {
    switch (o->type) {
    case TF_OBJ_TYPE_INT:
        printf("%" PRId64, o->i);
        break;
    case TF_OBJ_TYPE_FLOAT: {
        char buf[64];
        tf_format_double(buf, sizeof buf, o->f);
        printf("%s", buf);
        if (isfinite(o->f) && !strpbrk(buf, ".eE")) printf(".0");
        break;
    }
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
        for (size_t i = 0; i < o->vector.len; i++) {
            print_source_like(o->vector.elem[i], display);
            if (i + 1 < o->vector.len) printf(" ");
        }
        printf("|");
        break;
    case TF_OBJ_TYPE_VECTOR:
        printf("[");
        for (size_t i = 0; i < o->vector.len; i++) {
            print_source_like(o->vector.elem[i], display);
            if (i + 1 < o->vector.len) printf(" ");
        }
        printf("]");
        break;
    case TF_OBJ_TYPE_LIST: {
        printf("(");
        tf_list_node *node = o->list.head;
        while (node) {
            print_source_like(node->value, display);
            node = node->next;
            if (node) printf(" ");
        }
        printf(")");
        break;
    }
    case TF_OBJ_TYPE_MAP:
        printf("{");
        for (size_t i = 0; i < o->map.len; i++) {
            if (i > 0) printf(" ");
            print_source_like(o->map.entries[i].key, display);
            printf(" ");
            print_source_like(o->map.entries[i].value, display);
        }
        printf("}");
        break;
    case TF_OBJ_TYPE_SET:
        printf("#{");
        for (size_t i = 0; i < o->set.len; i++) {
            if (i > 0) printf(" ");
            print_source_like(o->set.entries[i].item, display);
        }
        printf("}");
        break;
    case TF_OBJ_TYPE_DEQUE:
        printf(display ? "deque[" : "[");
        for (size_t i = 0; i < o->deque.len; i++) {
            if (i > 0) printf(" ");
            print_source_like(tf_deque_get(o, i), display);
        }
        printf(display ? "]" : "] >deque");
        break;
    case TF_OBJ_TYPE_PQUEUE: {
        printf(display ? "pqueue[" : "[");
        tf_obj *tmp = tf_pqueue_clone(o);
        for (size_t i = 0; tmp->pqueue.len > 0; i++) {
            tf_obj *priority = NULL;
            tf_obj *value = NULL;
            tf_pqueue_pop(tmp, &priority, &value);
            if (i > 0) printf(" ");
            printf("[");
            print_source_like(priority, display);
            printf(" ");
            print_source_like(value, display);
            printf("]");
            tf_obj_release(priority);
            tf_obj_release(value);
        }
        tf_obj_release(tmp);
        printf(display ? "]" : "] >pqueue");
        break;
    }
    default:
        printf("?");
        break;
    }
}

void tf_obj_print_display(tf_obj *o) {
    print_source_like(o, true);
}

void tf_obj_print_source(tf_obj *o) {
    print_source_like(o, false);
}
