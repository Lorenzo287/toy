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

#endif  // TEST_BINDGEN_FIXTURE_H
