#ifndef TF_OBJ_H
#define TF_OBJ_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TF_OBJ_TYPE_BOOL,
    TF_OBJ_TYPE_INT,
    TF_OBJ_TYPE_FLOAT,
    TF_OBJ_TYPE_STR,
    TF_OBJ_TYPE_SYMBOL,
    TF_OBJ_TYPE_LIST,
    TF_OBJ_TYPE_VARLIST,
    TF_OBJ_TYPE_VARFETCH
} tf_type;

typedef struct {
    const char *filename;
    size_t offset;
    size_t line;
    size_t col;
    size_t len;
    bool valid;
} tf_source_span;

/*
 * Refcounted runtime value.
 *
 * Lists back both data lists and executable quotations. Symbols use the same
 * string storage as strings, plus a quoted flag that preserves source/eval
 * mode for parsed programs and source printing.
 */
typedef struct tf_obj {
    int refcount;
    tf_type type;
    tf_source_span span;
    union {
        int i;
        float f;
        bool b;
        struct {
            char *ptr;
            size_t len;
            bool quoted;
        } str;
        struct {
            struct tf_obj **elem;
            size_t len;
            size_t cap;
        } list;
    };
} tf_obj;

/* Object constructors. Returned objects start with refcount 1. */
tf_obj *tf_obj_new(int type);
tf_obj *tf_obj_new_list(void);
tf_obj *tf_obj_new_int(int i);
tf_obj *tf_obj_new_bool(bool b);
tf_obj *tf_obj_new_float(float f);
tf_obj *tf_obj_new_symbol(const char *s, size_t len);
tf_obj *tf_obj_new_quoted_symbol(const char *s, size_t len);
tf_obj *tf_obj_new_string(const char *s, size_t len);
void tf_obj_set_span(tf_obj *o, tf_source_span span);

/* String/symbol comparison and list storage helpers. */
int tf_obj_compare_string(tf_obj *a, tf_obj *b);
void tf_list_push(tf_obj *l, tf_obj *elem);
tf_obj *tf_list_pop_type(tf_obj *l, tf_type type);
tf_obj *tf_list_pop(tf_obj *l);

/* Reference-count ownership. */
void tf_obj_retain(tf_obj *o);
void tf_obj_release(tf_obj *o);
void tf_obj_free(tf_obj *o);

/* Debug, runtime-value, and source-form printers. */
void tf_obj_print(tf_obj *o, size_t *count);
void tf_obj_print_value(tf_obj *o);
void tf_obj_print_source(tf_obj *o);

#endif  // TF_OBJ_H
