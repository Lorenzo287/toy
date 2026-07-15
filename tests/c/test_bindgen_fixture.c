#include "test_bindgen_fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct test_bindgen_box {
    int64_t value;
    char label[48];
};

static int32_t box_destroy_count = 0;
static int32_t box_live_count = 0;

static test_bindgen_box *allocate_box(int64_t value) {
    test_bindgen_box *box = malloc(sizeof(*box));
    if (!box) return NULL;
    box->value = value;
    snprintf(box->label, sizeof(box->label), "box-%lld", (long long)value);
    box_live_count++;
    return box;
}

int32_t test_bindgen_add_i32(int32_t left, int32_t right) {
    return left + right;
}

int8_t test_bindgen_negative_i8(void) {
    return -7;
}

double test_bindgen_scale_f64(double value, int32_t scale) {
    return value * scale + 0.5;
}

size_t test_bindgen_text_length(const char *text) {
    return strlen(text);
}

const char *test_bindgen_greeting(void) {
    return "hello from generated C";
}

bool test_bindgen_not(bool value) {
    return !value;
}

void test_bindgen_ignore_i32(int32_t value) {
    (void)value;
}

uint64_t test_bindgen_too_large(void) {
    return UINT64_MAX;
}

test_bindgen_box *test_bindgen_box_new(int64_t value) {
    return value == -1 ? NULL : allocate_box(value);
}

int32_t test_bindgen_box_open(int64_t value, test_bindgen_box **output) {
    *output = allocate_box(value);
    return *output ? 0 : 12;
}

int32_t test_bindgen_box_open_alternate(int64_t value,
                                        test_bindgen_box **output) {
    *output = allocate_box(value);
    return *output ? 1 : 12;
}

int32_t test_bindgen_box_open_fail(int64_t value,
                                   test_bindgen_box **output) {
    *output = allocate_box(value);
    return 17;
}

int32_t test_bindgen_box_open_empty(test_bindgen_box **output) {
    *output = NULL;
    return 0;
}

int64_t test_bindgen_box_value(test_bindgen_box *box) {
    return box->value;
}

const char *test_bindgen_box_label(test_bindgen_box *box) {
    return box->label;
}

int32_t test_bindgen_box_set(test_bindgen_box *box, int64_t value) {
    box->value = value;
    snprintf(box->label, sizeof(box->label), "box-%lld", (long long)value);
    return 0;
}

void test_bindgen_box_destroy(test_bindgen_box *box) {
    box_destroy_count++;
    box_live_count--;
    free(box);
}

int32_t test_bindgen_box_destroy_count(void) {
    return box_destroy_count;
}

int32_t test_bindgen_box_live_count(void) {
    return box_live_count;
}
