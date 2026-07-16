#ifndef TEST_BINDGEN_FIXTURE_H
#define TEST_BINDGEN_FIXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#ifdef TEST_BINDGEN_FIXTURE_BUILD
#define TEST_BINDGEN_EXPORT __declspec(dllexport)
#else
#define TEST_BINDGEN_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define TEST_BINDGEN_EXPORT __attribute__((visibility("default")))
#else
#define TEST_BINDGEN_EXPORT
#endif

typedef struct test_bindgen_box test_bindgen_box;
typedef struct test_bindgen_child test_bindgen_child;

#define TEST_BINDGEN_BIAS 5

TEST_BINDGEN_EXPORT int32_t test_bindgen_add_i32(int32_t left,
                                                int32_t right);
TEST_BINDGEN_EXPORT int8_t test_bindgen_negative_i8(void);
TEST_BINDGEN_EXPORT double test_bindgen_scale_f64(double value,
                                                 int32_t scale);
TEST_BINDGEN_EXPORT size_t test_bindgen_text_length(const char *text);
TEST_BINDGEN_EXPORT const char *test_bindgen_greeting(void);
TEST_BINDGEN_EXPORT bool test_bindgen_not(bool value);
TEST_BINDGEN_EXPORT void test_bindgen_ignore_i32(int32_t value);
TEST_BINDGEN_EXPORT uint64_t test_bindgen_too_large(void);
TEST_BINDGEN_EXPORT int32_t test_bindgen_hidden_arguments(
    int32_t value, int32_t offset, const void *pointer, int32_t bias);
TEST_BINDGEN_EXPORT uint32_t test_bindgen_byte_sum(const char *data,
                                                  uint32_t length);
TEST_BINDGEN_EXPORT const unsigned char *test_bindgen_binary(void);
TEST_BINDGEN_EXPORT uint32_t test_bindgen_binary_length(void);
TEST_BINDGEN_EXPORT int32_t test_bindgen_even_status(int64_t value);
TEST_BINDGEN_EXPORT test_bindgen_box *test_bindgen_box_new(int64_t value);
TEST_BINDGEN_EXPORT int32_t test_bindgen_box_open(
    int64_t value, test_bindgen_box **output);
TEST_BINDGEN_EXPORT int32_t test_bindgen_box_open_alternate(
    int64_t value, test_bindgen_box **output);
TEST_BINDGEN_EXPORT int32_t test_bindgen_box_open_fail(
    int64_t value, test_bindgen_box **output);
TEST_BINDGEN_EXPORT int32_t test_bindgen_box_open_empty(
    test_bindgen_box **output);
TEST_BINDGEN_EXPORT int64_t test_bindgen_box_value(test_bindgen_box *box);
TEST_BINDGEN_EXPORT const char *test_bindgen_box_label(
    test_bindgen_box *box);
TEST_BINDGEN_EXPORT int32_t test_bindgen_box_set(test_bindgen_box *box,
                                                int64_t value);
TEST_BINDGEN_EXPORT const char *test_bindgen_box_error(test_bindgen_box *box);
TEST_BINDGEN_EXPORT void test_bindgen_box_destroy(test_bindgen_box *box);
TEST_BINDGEN_EXPORT int32_t test_bindgen_box_destroy_count(void);
TEST_BINDGEN_EXPORT int32_t test_bindgen_box_live_count(void);
TEST_BINDGEN_EXPORT int32_t test_bindgen_child_open(
    test_bindgen_box *parent, test_bindgen_child **output);
TEST_BINDGEN_EXPORT int32_t test_bindgen_child_open_fail(
    test_bindgen_box *parent, test_bindgen_child **output);
TEST_BINDGEN_EXPORT int64_t test_bindgen_child_value(
    test_bindgen_child *child);
TEST_BINDGEN_EXPORT void test_bindgen_child_destroy(test_bindgen_child *child);
TEST_BINDGEN_EXPORT int32_t test_bindgen_child_destroy_count(void);
TEST_BINDGEN_EXPORT int32_t test_bindgen_child_parent_alive_count(void);

#endif  // TEST_BINDGEN_FIXTURE_H
