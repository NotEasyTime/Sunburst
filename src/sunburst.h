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

bool InitWindow(int width, int height, const char* title);
void PollEvents(void);
bool WindowShouldClose(void);
void CloseWindowSB(void);  

// Optional helpers
void SetWindowTitle(const char* title);
void GetFramebufferSize(int* outWidth, int* outHeight);

// GL lifecycle/helpers 
bool CreateGLContext(void);
void GL_SwapBuffers(void);
void GL_SetSwapInterval(int interval);

// Diagnostics
void PrintFrameRate(void);

// simple 2D drawing
typedef struct Color { float r, g, b, a; } Color;
void DrawRectangle(int x, int y, int width, int height, Color color);
void DrawTriangle(int ax, int ay, int bx, int by, int cx, int cy, Color color);

void ClearBackground();
void Begin2D(void);
void End2D(void);
void RendererInit(void);
void RendererShutdown(void);

#ifdef __cplusplus
}
#endif
