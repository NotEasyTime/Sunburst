#pragma once
#include <stdbool.h>
#include "clay.h"
#include "glfw3.h"
#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION 1
  #include <OpenGL/gl3.h>
#elif defined(__linux__)
  #include <GL/gl.h>
#elif defined(_MSC_VER)
  #include <windows.h>
  #include "glad/glad.h"
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
void HandleClayErrors(Clay_ErrorData);

// GLFW
void error_callback(int, const char*);
void SunburstInit();