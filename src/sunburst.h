#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool InitWindow(int width, int height, const char* title);
void PollEvents(void);
bool WindowShouldClose(void);
void CloseWindowSB(void);  // <-- renamed (was DestroyWindow)

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

#ifdef __cplusplus
}
#endif
