#ifdef _WIN32
#include <stdlib.h>
#else
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#endif

#include "toy.h"

#include <stdio.h>
#include <string.h>

#ifdef STB_LEAKCHECK
#include "tf_alloc.h"
#endif

#define CHECK(condition, message)                                         \
    do {                                                                  \
        if (!(condition)) {                                               \
            fprintf(stderr, "raylib module check failed: %s\n", message); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

static void discard_diagnostic(void *userdata, const char *data,
                               size_t length) {
    (void)userdata;
    (void)data;
    (void)length;
}

static bool set_module_path(const char *path) {
#ifdef _WIN32
    return _putenv_s("TOY_MODULE_PATH", path) == 0;
#else
    return setenv("TOY_MODULE_PATH", path, 1) == 0;
#endif
}

int main(int argc, char **argv) {
    CHECK(argc == 2, "module directory argument");
    CHECK(set_module_path(argv[1]), "set module search path");

    toy_state_config config = {
        .diagnostic = discard_diagnostic,
    };
    toy_state *state = toy_state_new(&config);
    CHECK(state, "state creation");
    CHECK(toy_eval(state, "<raylib-require>",
                   "\"raylib\" 'rl require-as") == TOY_OK,
          "load Raylib module");

    CHECK(toy_eval(state, "<raylib-init>",
                   "640 480 \"stub window\" rl.init-window "
                   "60 rl.set-target-fps") == TOY_OK,
          "window initialization");
    CHECK(toy_eval(state, "<raylib-double-init>",
                   "320 200 \"duplicate\" rl.init-window") == TOY_ERROR,
          "reject duplicate window initialization");
    CHECK(toy_pop(state, 3), "clear rejected window inputs");

    CHECK(toy_eval(state, "<raylib-color>", "1 2 3 4 rl.rgba") == TOY_OK,
          "RGBA construction");
    int64_t integer = 0;
    CHECK(toy_get_int(state, 0, &integer) && integer == 0x01020304,
          "RGBA packed value");
    CHECK(toy_pop(state, 1), "pop RGBA value");

    CHECK(toy_eval(state, "<raylib-draw>",
                   "rl.begin-drawing "
                   "245 246 247 248 rl.rgba rl.clear-background "
                   "10 20 12.5 1 2 3 4 rl.rgba rl.draw-circle "
                   "30 40 50 60 5 6 7 8 rl.rgba rl.draw-rectangle "
                   "\"hello\" 70 80 24 9 10 11 12 rl.rgba "
                   "rl.draw-text rl.end-drawing") == TOY_OK,
          "drawing calls");
    CHECK(toy_stack_size(state) == 0, "drawing words consume inputs");

    CHECK(toy_eval(state, "<raylib-load-texture>",
                   "\"sprite.png\" rl.load-texture") == TOY_OK,
          "load texture resource");
    CHECK(toy_stack_type(state, 0) == TOY_TYPE_RESOURCE,
          "texture uses a resource value");
    const char *resource_type = NULL;
    CHECK(toy_get_resource_type(state, 0, &resource_type) &&
              strcmp(resource_type, "raylib.texture") == 0,
          "texture resource type");
    CHECK(toy_eval(state, "<raylib-draw-texture>",
                   "dup 90 100 20 30 40 255 rl.rgba rl.draw-texture") ==
              TOY_OK,
          "draw texture resource");
    CHECK(toy_pop(state, 1), "release retained texture");

    int foreign_texture = 0;
    CHECK(toy_push_resource(state, "host.texture", &foreign_texture, NULL,
                            NULL) == TOY_OK,
          "push mismatched texture resource");
    CHECK(toy_eval(state, "<raylib-wrong-texture>",
                   "1 2 3 4 5 255 rl.rgba rl.draw-texture") == TOY_ERROR,
          "reject resource with wrong type tag");
    CHECK(toy_get_error(state) &&
              strstr(toy_get_error(state), "expected a raylib texture"),
          "wrong texture type diagnostic");
    CHECK(toy_pop(state, 4), "clear rejected texture draw inputs");

    CHECK(toy_eval(state, "<raylib-texture-nul>",
                   "\"bad\\x00path\" rl.load-texture") == TOY_ERROR,
          "reject embedded NUL texture path");
    CHECK(toy_pop(state, 1), "pop rejected texture path");
    CHECK(toy_eval(state, "<raylib-texture-failure>",
                   "\"missing.png\" rl.load-texture") == TOY_ERROR,
          "report texture load failure");
    CHECK(toy_stack_size(state) == 0,
          "failed texture load consumed valid path");

    CHECK(toy_eval(state, "<raylib-input>",
                   "rl.window-should-close? rl.mouse-x rl.mouse-y "
                   "rl.frame-time") == TOY_OK,
          "query words");
    bool should_close = true;
    double frame_time = 0.0;
    CHECK(toy_get_float(state, 0, &frame_time) && frame_time == 0.25,
          "frame time result");
    CHECK(toy_get_int(state, 1, &integer) && integer == 234,
          "mouse y result");
    CHECK(toy_get_int(state, 2, &integer) && integer == 123,
          "mouse x result");
    CHECK(toy_get_bool(state, 3, &should_close) && !should_close,
          "window close result");
    CHECK(toy_pop(state, 4), "pop query results");

    CHECK(toy_eval(state, "<raylib-invalid-color>",
                   "0 0 0 256 rl.rgba") == TOY_ERROR,
          "invalid color rejected");
    CHECK(toy_pop(state, 4), "pop rejected color inputs");
    CHECK(toy_eval(state, "<raylib-embedded-nul>",
                   "\"a\\x00b\" 0 0 12 1 2 3 4 rl.rgba "
                   "rl.draw-text") == TOY_ERROR,
          "embedded NUL rejected");
    CHECK(toy_pop(state, 5), "pop rejected text inputs");

    CHECK(toy_eval(state, "<raylib-close>", "rl.close-window") == TOY_OK,
          "close window");
    CHECK(toy_eval(state, "<raylib-close-again>", "rl.close-window") ==
              TOY_OK,
          "close window is idempotent");
    CHECK(toy_eval(state, "<raylib-init-failure>",
                   "320 200 \"failure\" rl.init-window") == TOY_ERROR,
          "initialization failure reported");
    CHECK(toy_stack_size(state) == 0,
          "failed initialization consumed valid inputs");

    toy_state_free(state);
#ifdef STB_LEAKCHECK
    stb_leakcheck_dumpmem();
#endif
    return 0;
}
