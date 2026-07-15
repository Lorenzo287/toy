#include "raylib.h"

#include <string.h>

static bool window_ready = false;

void InitWindow(int width, int height, const char *title) {
    window_ready = width > 0 && height > 0 && strcmp(title, "failure") != 0;
}

void CloseWindow(void) {
    window_ready = false;
}

bool WindowShouldClose(void) {
    return false;
}

bool IsWindowReady(void) {
    return window_ready;
}

void SetTargetFPS(int fps) {
    (void)fps;
}

void BeginDrawing(void) {}
void EndDrawing(void) {}

void ClearBackground(Color color) {
    (void)color;
}

void DrawCircle(int center_x, int center_y, float radius, Color color) {
    (void)center_x;
    (void)center_y;
    (void)radius;
    (void)color;
}

void DrawRectangle(int x, int y, int width, int height, Color color) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
}

void DrawText(const char *text, int x, int y, int font_size, Color color) {
    (void)text;
    (void)x;
    (void)y;
    (void)font_size;
    (void)color;
}

Texture2D LoadTexture(const char *file_name) {
    if (strcmp(file_name, "missing.png") == 0) return (Texture2D){0};
    return (Texture2D){.id = 77,
                       .width = 32,
                       .height = 16,
                       .mipmaps = 1,
                       .format = 7};
}

void UnloadTexture(Texture2D texture) {
    (void)texture;
}

void DrawTexture(Texture2D texture, int x, int y, Color tint) {
    (void)texture;
    (void)x;
    (void)y;
    (void)tint;
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
