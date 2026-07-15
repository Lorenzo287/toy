#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#define FIXTURE_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#else
#define FIXTURE_EXPORT
#endif

FIXTURE_EXPORT int32_t toy_ffi_add_i32(int32_t left, int32_t right) {
    return left + right;
}

FIXTURE_EXPORT int8_t toy_ffi_negative_i8(void) {
    return -7;
}

FIXTURE_EXPORT uint32_t toy_ffi_large_u32(void) {
    return UINT32_MAX;
}

FIXTURE_EXPORT double toy_ffi_scale_f64(double value, int32_t scale) {
    return value * scale + 0.5;
}

FIXTURE_EXPORT size_t toy_ffi_text_length(const char *text) {
    return strlen(text);
}

FIXTURE_EXPORT const char *toy_ffi_greeting(void) {
    return "hello from C";
}

FIXTURE_EXPORT bool toy_ffi_not(bool value) {
    return !value;
}

FIXTURE_EXPORT void toy_ffi_ignore_i32(int32_t value) {
    (void)value;
}

FIXTURE_EXPORT uint64_t toy_ffi_too_large(void) {
    return UINT64_MAX;
}
