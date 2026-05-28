#ifndef TF_ALLOC_H
#define TF_ALLOC_H

#include <stddef.h>

#ifdef STB_LEAKCHECK
#include "stb_leakcheck/stb_leakcheck.h"
#endif

/* Checked allocation helpers. They terminate the process on allocation failure. */
void *tf_xmalloc(size_t size);
void *tf_xrealloc(void *ptr, size_t size);
void *tf_xcalloc(size_t nmemb, size_t size);
char *tf_xstrdup(const char *s);

#endif  // TF_ALLOC_H
