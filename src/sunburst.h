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

// Diagnostics
void PrintFrameRate(void);

// simple 2D drawing
void DrawRectangle(int, int, int, int, Color);

void ClearBackground();
void Begin2D(int ,int);
void End2D(void);
void RendererInit(void);
void RendererShutdown(void);

// Clay 
void HandleClayErrors(Clay_ErrorData);

// GLFW
void error_callback(int, const char*);
void SunburstInit();