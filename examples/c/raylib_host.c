#include "toy.h"
#include "toy_raylib.h"

#include <stdio.h>
#include <string.h>

static int reportError(toy_state *state, const char *operation,
                       toy_status status) {
    const char *message = toy_get_error(state);
    fprintf(stderr, "%s failed (status %d): %s\n", operation, (int)status,
            message ? message : "no diagnostic available");
    return 1;
}

static void closeWindow(toy_state *state) {
    toy_clear_error(state);
    (void)toy_call(state, "raylib.close-window");
}

int main(int argc, char **argv) {
    const char *script = argc > 1 ? argv[1] : "examples/toy/raylib.toy";
    toy_state *state = toy_state_new(NULL);
    if (!state) {
        fputs("failed to create Toy state\n", stderr);
        return 1;
    }

    toy_status status = toy_raylib_register(state);
    if (status != TOY_OK) {
        int result = reportError(state, "raylib registration", status);
        toy_state_free(state);
        return result;
    }

    for (int i = 2; i < argc; i++) {
        status = toy_push_string(state, argv[i], strlen(argv[i]));
        if (status != TOY_OK) {
            int result = reportError(state, "example argument", status);
            toy_state_free(state);
            return result;
        }
    }

    status = toy_push_string(state, script, strlen(script));
    if (status == TOY_OK) status = toy_call(state, "load");
    if (status != TOY_OK) {
        int result = reportError(state, "Toy program", status);
        closeWindow(state);
        toy_state_free(state);
        return result;
    }

    closeWindow(state);
    toy_state_free(state);
    return 0;
}
