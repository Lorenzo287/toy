#include "test_bindgen_fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct test_bindgen_box {
    int64_t value;
    char label[48];
};

struct test_bindgen_child {
    test_bindgen_box *parent;
};

static int32_t box_destroy_count = 0;
static int32_t box_live_count = 0;
static int32_t child_destroy_count = 0;
static int32_t child_parent_alive_count = 0;
static const unsigned char binary_value[] = {'A', 0, 'B', 0xff};

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

int32_t test_bindgen_hidden_arguments(int32_t value, int32_t offset,
                                      const void *pointer, int32_t bias) {
    return pointer ? INT32_MIN : value + offset + bias;
}

uint32_t test_bindgen_byte_sum(const char *data, uint32_t length) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += (unsigned char)data[i];
    }
    return sum;
}

const unsigned char *test_bindgen_binary(void) {
    return binary_value;
}

uint32_t test_bindgen_binary_length(void) {
    return (uint32_t)sizeof(binary_value);
}

int32_t test_bindgen_even_status(int64_t value) {
    return value % 2 == 0 ? 1 : 0;
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

const char *test_bindgen_box_error(test_bindgen_box *box) {
    return box->label;
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

int32_t test_bindgen_child_open(test_bindgen_box *parent,
                                test_bindgen_child **output) {
    test_bindgen_child *child = malloc(sizeof(*child));
    if (!child) {
        *output = NULL;
        return 12;
    }
    child->parent = parent;
    *output = child;
    return 0;
}

int32_t test_bindgen_child_open_fail(test_bindgen_box *parent,
                                     test_bindgen_child **output) {
    int32_t result = test_bindgen_child_open(parent, output);
    return result == 0 ? 17 : result;
}

int64_t test_bindgen_child_value(test_bindgen_child *child) {
    return child->parent->value;
}

void test_bindgen_child_destroy(test_bindgen_child *child) {
    child_destroy_count++;
    if (box_live_count > 0) child_parent_alive_count++;
    free(child);
}

int32_t test_bindgen_child_destroy_count(void) {
    return child_destroy_count;
}

int32_t test_bindgen_child_parent_alive_count(void) {
    return child_parent_alive_count;
}
