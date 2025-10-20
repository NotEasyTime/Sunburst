#pragma once
#include <stdbool.h>
#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION 1
  #include <OpenGL/gl3.h>
#elif defined _WIN32
  #include <windows.h>  // must precede gl.h so WINGDIAPI/APIENTRY are defined
  #include <GL/gl.h>
#else
  #include <GL/gl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Color { float r, g, b, a; } Color;

typedef struct {
    GLuint id;
    int width;
    int height;
} Texture;

// Diagnostics
void PrintFrameRate(void);

// simple 2D drawing
void DrawRectangle(int, int, int, int, Color);
// static inline void DrawRectangleRect(Rect* r, Color c){ DrawRectangle(r->x, r->y, r->width, r->height, c); }

void ClearBackground();
void Begin2D(int ,int);
void End2D(void);
void RendererInit(void);
void RendererShutdown(void);
Texture LoadTexture(const char*);
void DestroyTexture(Texture*);
void DrawTexture(Texture*, int, int, int, int, bool);

#ifdef __cplusplus
}
#endif
