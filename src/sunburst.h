#pragma once
#include <stdbool.h>
#include "clay.h"
#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION 1
  #include <OpenGL/gl3.h>
#elif defined _WIN32
  #include <windows.h>  // must precede gl.h so WINGDIAPI/APIENTRY are defined
  #include <GL/gl.h>
#else
  #include <GL/gl.h>
#endif

typedef struct Color { float r, g, b, a; } Color;

typedef struct {
    GLuint id;
    int width;
    int height;
} Texture;

typedef struct {
    // TODO: add clay id
    bool *value;
    float size_px;
    float gap_px;
} Checkbox;

// Diagnostics
void PrintFrameRate(void);

// simple 2D drawing
void DrawRectangle(int, int, int, int, Color);

void ClearBackground();
void Begin2D(int ,int);
void End2D(void);
void RendererInit(void);
void RendererShutdown(void);
Texture LoadTexture(const char*);
void DestroyTexture(Texture*);
void DrawTexture(Texture*, int, int, int, int, bool);

// Clay 
//void UI_Checkbox(RGFW_window*, Checkbox, bool);

