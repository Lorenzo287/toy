#include "raylib.h"
#include "toy.h"
#include "toy_raylib.h"

#include <stdio.h>
#include <string.h>

#ifdef STB_LEAKCHECK
#include "tf_alloc.h"
#endif

#define CHECK(condition, message)                                           \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "raylib module check failed: %s\n", message);  \
            return 1;                                                       \
        }                                                                   \
    } while (0)

static bool init_succeeds = true;
static bool window_ready = false;
static bool window_should_close = false;
static int window_width = 0;
static int window_height = 0;
static char window_title[64];
static int close_count = 0;
static int target_fps = 0;
static int begin_count = 0;
static int end_count = 0;
static Color background;
static int circle_x = 0;
static int circle_y = 0;
static float circle_radius = 0.0f;
static Color circle_color;
static int rectangle_x = 0;
static int rectangle_y = 0;
static int rectangle_width = 0;
static int rectangle_height = 0;
static Color rectangle_color;
static char drawn_text[64];
static int text_x = 0;
static int text_y = 0;
static int text_size = 0;
static Color text_color;
static bool texture_load_succeeds = true;
static int texture_load_count = 0;
static char texture_path[64];
static int texture_unload_count = 0;
static unsigned int unloaded_texture_id = 0;
static int texture_draw_count = 0;
static unsigned int drawn_texture_id = 0;
static int texture_x = 0;
static int texture_y = 0;
static Color texture_tint;

void InitWindow(int width, int height, const char *title) {
    window_width = width;
    window_height = height;
    snprintf(window_title, sizeof(window_title), "%s", title);
    window_ready = init_succeeds;
}

void CloseWindow(void) {
    close_count++;
    window_ready = false;
}

bool WindowShouldClose(void) {
    return window_should_close;
}

bool IsWindowReady(void) {
    return window_ready;
}

void SetTargetFPS(int fps) {
    target_fps = fps;
}

void BeginDrawing(void) {
    begin_count++;
}

void EndDrawing(void) {
    end_count++;
}

void ClearBackground(Color color) {
    background = color;
}

void DrawCircle(int center_x, int center_y, float radius, Color color) {
    circle_x = center_x;
    circle_y = center_y;
    circle_radius = radius;
    circle_color = color;
}

void DrawRectangle(int x, int y, int width, int height, Color color) {
    rectangle_x = x;
    rectangle_y = y;
    rectangle_width = width;
    rectangle_height = height;
    rectangle_color = color;
}

void DrawText(const char *text, int x, int y, int font_size, Color color) {
    snprintf(drawn_text, sizeof(drawn_text), "%s", text);
    text_x = x;
    text_y = y;
    text_size = font_size;
    text_color = color;
}

Texture2D LoadTexture(const char *file_name) {
    texture_load_count++;
    snprintf(texture_path, sizeof(texture_path), "%s", file_name);
    if (!texture_load_succeeds) return (Texture2D){0};
    return (Texture2D){.id = 77,
                       .width = 32,
                       .height = 16,
                       .mipmaps = 1,
                       .format = 7};
}

void UnloadTexture(Texture2D texture) {
    texture_unload_count++;
    unloaded_texture_id = texture.id;
}

void DrawTexture(Texture2D texture, int x, int y, Color tint) {
    texture_draw_count++;
    drawn_texture_id = texture.id;
    texture_x = x;
    texture_y = y;
    texture_tint = tint;
}

int GetMouseX(void) {
    return 123;
}

int GetMouseY(void) {
    return 234;
}

float GetFrameTime(void) {
    return 0.25f;
}

static bool color_is(Color color, int red, int green, int blue, int alpha) {
    return color.r == red && color.g == green && color.b == blue &&
           color.a == alpha;
}

int main(void) {
    toy_state *state = toy_state_new(NULL);
    CHECK(state, "state creation");
    CHECK(toy_raylib_register(state) == TOY_OK, "module registration");

    CHECK(toy_eval(state, "<raylib-init>",
                   "\"raylib\" 'rl require-as "
                   "640 480 \"stub window\" rl.init-window "
                   "60 rl.set-target-fps") == TOY_OK,
          "window initialization");
    CHECK(window_ready && window_width == 640 && window_height == 480,
          "window dimensions");
    CHECK(strcmp(window_title, "stub window") == 0, "copied window title");
    CHECK(target_fps == 60, "target FPS");

    CHECK(toy_eval(state, "<raylib-color>", "1 2 3 4 rl.rgba") ==
              TOY_OK,
          "RGBA construction");
    int64_t packed = 0;
    CHECK(toy_get_int(state, 0, &packed) && packed == 0x01020304,
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
    CHECK(begin_count == 1 && end_count == 1, "drawing boundary");
    CHECK(color_is(background, 245, 246, 247, 248), "background color");
    CHECK(circle_x == 10 && circle_y == 20 && circle_radius == 12.5f &&
              color_is(circle_color, 1, 2, 3, 4),
          "circle arguments");
    CHECK(rectangle_x == 30 && rectangle_y == 40 &&
              rectangle_width == 50 && rectangle_height == 60 &&
              color_is(rectangle_color, 5, 6, 7, 8),
          "rectangle arguments");
    CHECK(strcmp(drawn_text, "hello") == 0 && text_x == 70 && text_y == 80 &&
              text_size == 24 && color_is(text_color, 9, 10, 11, 12),
          "text arguments");
    CHECK(toy_stack_size(state) == 0, "drawing words consume inputs");

    CHECK(toy_eval(state, "<raylib-load-texture>",
                   "\"sprite.png\" rl.load-texture") == TOY_OK,
          "load texture resource");
    CHECK(texture_load_count == 1 && strcmp(texture_path, "sprite.png") == 0,
          "texture load arguments");
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
    CHECK(texture_draw_count == 1 && drawn_texture_id == 77 &&
              texture_x == 90 && texture_y == 100 &&
              color_is(texture_tint, 20, 30, 40, 255),
          "texture draw arguments");
    CHECK(texture_unload_count == 0,
          "retained texture remains loaded after drawing");
    CHECK(toy_pop(state, 1), "release retained texture");
    CHECK(texture_unload_count == 1 && unloaded_texture_id == 77,
          "texture resource destructor unloads once");

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
    CHECK(toy_get_error(state) &&
              strstr(toy_get_error(state), "embedded NUL byte"),
          "embedded NUL texture diagnostic");
    CHECK(toy_pop(state, 1), "pop rejected texture path");

    texture_load_succeeds = false;
    CHECK(toy_eval(state, "<raylib-texture-failure>",
                   "\"missing.png\" rl.load-texture") == TOY_ERROR,
          "report texture load failure");
    CHECK(toy_get_error(state) &&
              strstr(toy_get_error(state), "failed to load"),
          "texture load failure diagnostic");
    CHECK(toy_stack_size(state) == 0,
          "failed texture load consumed valid path");
    CHECK(texture_unload_count == 1,
          "failed texture was not passed to the destructor");

    window_should_close = true;
    CHECK(toy_eval(state, "<raylib-input>",
                   "rl.window-should-close? rl.mouse-x rl.mouse-y "
                   "rl.frame-time") == TOY_OK,
          "query words");
    bool should_close = false;
    int64_t integer = 0;
    double frame_time = 0.0;
    CHECK(toy_get_float(state, 0, &frame_time) && frame_time == 0.25,
          "frame time result");
    CHECK(toy_get_int(state, 1, &integer) && integer == 234,
          "mouse y result");
    CHECK(toy_get_int(state, 2, &integer) && integer == 123,
          "mouse x result");
    CHECK(toy_get_bool(state, 3, &should_close) && should_close,
          "window close result");
    CHECK(toy_pop(state, 4), "pop query results");

    CHECK(toy_eval(state, "<raylib-invalid-color>",
                   "0 0 0 256 rl.rgba") == TOY_ERROR,
          "invalid color rejected");
    CHECK(toy_get_error(state) && strstr(toy_get_error(state), "0 and 255"),
          "invalid color diagnostic");
    CHECK(toy_pop(state, 4), "pop rejected color inputs");

    CHECK(toy_eval(state, "<raylib-embedded-nul>",
                   "\"a\\x00b\" 0 0 12 1 2 3 4 rl.rgba "
                   "rl.draw-text") == TOY_ERROR,
          "embedded NUL rejected");
    CHECK(toy_get_error(state) && strstr(toy_get_error(state), "NUL byte"),
          "embedded NUL diagnostic");
    CHECK(toy_pop(state, 5), "pop rejected text inputs");

    CHECK(toy_eval(state, "<raylib-close>", "rl.close-window") == TOY_OK,
          "close window");
    CHECK(!window_ready && close_count == 1, "window closed once");
    CHECK(toy_eval(state, "<raylib-close-again>", "rl.close-window") ==
              TOY_OK,
          "close window is idempotent");
    CHECK(close_count == 1, "closed window was not closed again");

    init_succeeds = false;
    CHECK(toy_eval(state, "<raylib-init-failure>",
                   "320 200 \"failure\" rl.init-window") == TOY_ERROR,
          "initialization failure reported");
    CHECK(toy_get_error(state) &&
              strstr(toy_get_error(state), "failed to initialize"),
          "initialization failure diagnostic");
    CHECK(toy_stack_size(state) == 0,
          "failed initialization consumed valid inputs");

    toy_state_free(state);
#ifdef STB_LEAKCHECK
    stb_leakcheck_dumpmem();
#endif
    return 0;
}
