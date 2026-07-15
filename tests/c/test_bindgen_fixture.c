#include "test_bindgen_fixture.h"

#include <string.h>

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
