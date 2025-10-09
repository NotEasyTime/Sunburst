// sunburst.h
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool InitWindow(int width, int height, const char* title);
void PollEvents(void);
bool WindowShouldClose(void);
void DestroyWindow(void);

// Optional helpers
void SetWindowTitle(const char* title);
void GetFramebufferSize(int* outWidth, int* outHeight);

// GL lifecycle/helpers 
bool CreateGLContext(void);          // create & attach a GL context to the window's content
void GL_SwapBuffers(void);           // present the back buffer
void GL_SetSwapInterval(int interval); // 0 = immediate, 1 = vsync

// Diagnostics
void PrintFrameRate(void);


// simple 2D drawing
typedef struct Color { float r, g, b, a; } Color;
// Draw a solid rectangle in pixel coords (origin: top-left of framebuffer)
void DrawRectangle(int x, int y, int width, int height, Color color);

#ifdef __cplusplus
}
#endif
