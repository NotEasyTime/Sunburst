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

typedef struct {
    GLuint id;
    int width;
    int height;
} Texture;

typedef struct {
    float x;
    float y;
    float width;
    float height;
} Rectangle;

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

// ===== Input API (cross-platform, polling model) =====
typedef enum Key {
    KEY_UNKNOWN = 0,
    // Letters
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    // Numbers row
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    // Controls / navigation
    KEY_SPACE, KEY_ENTER, KEY_ESCAPE, KEY_BACKSPACE, KEY_TAB,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_SHIFT, KEY_CTRL, KEY_ALT, KEY_SUPER,   // generic modifiers
    KEY_PAGEUP, KEY_PAGEDOWN, KEY_HOME, KEY_END, KEY_INSERT, KEY_DELETE,
    KEY_COUNT
} Key;

typedef enum MouseButton {
    MOUSE_LEFT = 0,
    MOUSE_RIGHT = 1,
    MOUSE_MIDDLE = 2,
    MOUSE_X1 = 3,
    MOUSE_X2 = 4,
    MOUSE_BUTTON_COUNT
} MouseButton;

// Call once at startup/shutdown
void InputInit(void);
void InputShutdown(void);

// Call once per frame (usually at the *start* of your frame, before PollEvents)
void InputBeginFrame(void);

// Queries (edge = true only on the frame the change occurred)
bool IsKeyDown(Key k);
bool IsKeyPressed(Key k);
bool IsKeyReleased(Key k);

bool IsMouseDown(MouseButton b);
bool IsMousePressed(MouseButton b);
bool IsMouseReleased(MouseButton b);

// Mouse position (window/client coordinates) and wheel delta (per frame)
void GetMousePosition(int* x, int* y);
int  GetMouseWheelDelta(void);

// Optional: UTF-32 text input (returns true if a codepoint is available this frame)
bool GetCharPressed(unsigned* outCodepoint);

// Hooks Swift/AppKit will call:
void SB_InputSetKey(int /*Key*/ k, int down);
void SB_InputSetMouse(int /*MouseButton*/ b, int down);
void SB_InputSetMousePos(int x, int y);
void SB_InputAddWheel(int delta);
void SB_InputPushUTF32(unsigned codepoint);


// simple 2D drawing
typedef struct Color { float r, g, b, a; } Color;
void DrawRectangle(int x, int y, int width, int height, Color color);

void ClearBackground();
void Begin2D(void);
void End2D(void);
void RendererInit(void);
void RendererShutdown(void);
Texture LoadTexture(const char*);
void DestroyTexture(Texture*);
void DrawTexture(Texture*, int, int, int, int, bool);

int GetRectOverlap(const Rectangle* a, const Rectangle* b, Rectangle*);
int CheckRectPoint(const Rectangle*, float, float);
int CheckRectsOverlap(const Rectangle*, const Rectangle*);


#ifdef __cplusplus
}
#endif
