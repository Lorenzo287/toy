#ifndef RAYLIB_H
#define RAYLIB_H

#include <stdbool.h>

typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

typedef struct Texture {
    unsigned int id;
    int width;
    int height;
    int mipmaps;
    int format;
} Texture2D;

void InitWindow(int width, int height, const char *title);
void CloseWindow(void);
bool WindowShouldClose(void);
bool IsWindowReady(void);
void SetTargetFPS(int fps);
void BeginDrawing(void);
void EndDrawing(void);
void ClearBackground(Color color);
void DrawCircle(int center_x, int center_y, float radius, Color color);
void DrawRectangle(int x, int y, int width, int height, Color color);
void DrawText(const char *text, int x, int y, int font_size, Color color);
Texture2D LoadTexture(const char *file_name);
void UnloadTexture(Texture2D texture);
void DrawTexture(Texture2D texture, int x, int y, Color tint);
int GetMouseX(void);
int GetMouseY(void);
float GetFrameTime(void);

#endif  // RAYLIB_H
