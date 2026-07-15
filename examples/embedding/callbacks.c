#include "toy.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CAPTURE_CAPACITY 4096

typedef struct {
    unsigned char data[CAPTURE_CAPACITY];
    size_t length;
    bool truncated;
} CaptureBuffer;

static void captureWrite(void *userdata, const char *data, size_t length) {
    CaptureBuffer *buffer = userdata;
    size_t available = CAPTURE_CAPACITY - buffer->length;
    size_t copied = length < available ? length : available;
    memcpy(buffer->data + buffer->length, data, copied);
    buffer->length += copied;
    if (copied < length) buffer->truncated = true;
}

static void printCapture(const char *label, const CaptureBuffer *buffer) {
    printf("%s (%zu bytes%s):\n", label, buffer->length,
           buffer->truncated ? ", truncated" : "");
    for (size_t i = 0; i < buffer->length; i++) {
        unsigned char byte = buffer->data[i];
        if (byte == '\n') {
            putchar('\n');
        } else if (byte == '\0') {
            fputs("<NUL>", stdout);
        } else if (isprint(byte)) {
            putchar(byte);
        } else {
            printf("<%02X>", byte);
        }
    }
    if (buffer->length == 0 || buffer->data[buffer->length - 1] != '\n') {
        putchar('\n');
    }
}

int main(void) {
    CaptureBuffer output = {0};
    CaptureBuffer diagnostic = {0};
    toy_state_config config = {
        .output = captureWrite,
        .output_userdata = &output,
        .diagnostic = captureWrite,
        .diagnostic_userdata = &diagnostic,
    };

    toy_state *state = toy_state_new(&config);
    if (!state) {
        fputs("failed to create Toy state\n", stderr);
        return 1;
    }

    const char *program =
        "\"hello from Toy\" print\n"
        "42 \"answer={}\\n\" printf\n"
        "\"binary: A\\x00B\" print";
    toy_status status = toy_eval(state, "callback-demo.toy", program);
    if (status != TOY_OK || output.truncated) {
        fputs("output demonstration failed\n", stderr);
        toy_state_free(state);
        return 1;
    }
    printCapture("Captured Toy output", &output);

    status = toy_eval(state, "broken-demo.toy", "[ 1 2");
    if (status != TOY_ERROR || diagnostic.truncated) {
        fputs("diagnostic demonstration failed\n", stderr);
        toy_state_free(state);
        return 1;
    }

    printCapture("Captured Toy diagnostic", &diagnostic);
    printf("toy_get_error(): %s\n",
           toy_get_error(state) ? toy_get_error(state) : "<none>");

    toy_state_free(state);
    return 0;
}
