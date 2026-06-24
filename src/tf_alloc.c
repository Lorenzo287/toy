#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef STB_LEAKCHECK
#define STB_LEAKCHECK_IMPLEMENTATION
#endif

#include "tf_alloc.h"

#ifdef TF_ALLOC_STATS
static size_t malloc_calls = 0;
static size_t calloc_calls = 0;
static size_t realloc_calls = 0;
static size_t requested_bytes = 0;

static void record_allocation(size_t *counter, size_t size) {
    (*counter)++;
    requested_bytes += size;
}
#endif

void *tf_xmalloc(size_t size) {
#ifdef TF_ALLOC_STATS
    record_allocation(&malloc_calls, size);
#endif
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Out of memory allocating %zu bytes\n", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *tf_xrealloc(void *ptr, size_t size) {
#ifdef TF_ALLOC_STATS
    record_allocation(&realloc_calls, size);
#endif
    ptr = realloc(ptr, size);
    if (!ptr) {
        fprintf(stderr, "Out of memory reallocating %zu bytes\n", size);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *tf_xcalloc(size_t nmemb, size_t size) {
#ifdef TF_ALLOC_STATS
    record_allocation(&calloc_calls, nmemb * size);
#endif
#ifdef STB_LEAKCHECK
    void *ptr = malloc(nmemb * size);
    if (!ptr) {
        fprintf(stderr, "Out of memory allocating %zu bytes\n", nmemb * size);
        exit(EXIT_FAILURE);
    }
    memset(ptr, 0, nmemb * size);
    return ptr;
#else
    void *ptr = calloc(nmemb, size);
    if (!ptr) {
        fprintf(stderr, "Out of memory allocating %zu bytes\n", nmemb * size);
        exit(EXIT_FAILURE);
    }
    return ptr;
#endif
}

char *tf_xstrdup(const char *s) {
    size_t len = strlen(s);
    char *ptr = tf_xmalloc(len + 1);
    memcpy(ptr, s, len + 1);
    return ptr;
}

#ifdef TF_ALLOC_STATS
void tf_alloc_stats_dump(void) {
    size_t total_calls = malloc_calls + calloc_calls + realloc_calls;
    fprintf(stderr,
            "allocations: %zu (malloc %zu, calloc %zu, realloc %zu)\n"
            "requested bytes: %zu\n",
            total_calls, malloc_calls, calloc_calls, realloc_calls,
            requested_bytes);
}
#endif
