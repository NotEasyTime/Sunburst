// Implements/b_Win32.c
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/sunburst.h"

// --- Globals ---
static HINSTANCE g_hinst;
static HWND      g_hwnd;
static HDC       g_hdc;
static HGLRC     g_glrc;
static bool      g_should_close = false;

// --- WGL extension glue ---
typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC,HGLRC,const int*);
typedef BOOL  (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);
typedef int   (WINAPI *PFNWGLGETSWAPINTERVALEXTPROC)(void);

static PFNWGLCREATECONTEXTATTRIBSARBPROC  p_wglCreateContextAttribsARB = NULL;
static PFNWGLSWAPINTERVALEXTPROC          p_wglSwapIntervalEXT        = NULL;
static PFNWGLGETSWAPINTERVALEXTPROC       p_wglGetSwapIntervalEXT     = NULL;

// Some WGL enums if headers are old
#ifndef WGL_DRAW_TO_WINDOW_ARB
#define WGL_DRAW_TO_WINDOW_ARB   0x2001
#define WGL_SUPPORT_OPENGL_ARB   0x2010
#define WGL_DOUBLE_BUFFER_ARB    0x2011
#define WGL_PIXEL_TYPE_ARB       0x2013
#define WGL_COLOR_BITS_ARB       0x2014
#define WGL_DEPTH_BITS_ARB       0x2022
#define WGL_STENCIL_BITS_ARB     0x2023
#define WGL_TYPE_RGBA_ARB        0x202B
#endif

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
#define WGL_CONTEXT_FLAGS_ARB         0x2094
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_CONTEXT_DEBUG_BIT_ARB     0x00000001
#endif

// --- Win32 window proc ---
static LRESULT CALLBACK Sunburst_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CLOSE:
        g_should_close = true;
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}

// --- UTF-8 -> UTF-16 helper ---
static void utf8_to_wide(const char* s, wchar_t* out, int outCount) {
    if (!s || !out || outCount <= 0) return;
    MultiByteToWideChar(CP_UTF8, 0, s, -1, out, outCount);
}

// --- Public API impl ---

bool InitWindow(int width, int height, const char* title) {
    if (g_hwnd) {
        // retitle/resize existing window
        if (title) {
            wchar_t wtitle[512];
            utf8_to_wide(title, wtitle, 512);
            SetWindowTextW(g_hwnd, wtitle);
        }
        RECT r = {0,0,width,height};
        AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
        SetWindowPos(g_hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(g_hwnd, SW_SHOW);
        return true;
    }

    g_hinst = GetModuleHandleW(NULL);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = Sunburst_WndProc;
    wc.hInstance     = g_hinst;
    wc.lpszClassName = L"SunburstWin32WndClass";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r = {0,0,width,height};
    AdjustWindowRectEx(&r, style, FALSE, 0);

    wchar_t wtitle[512] = L"Window";
    if (title) utf8_to_wide(title, wtitle, 512);

    g_hwnd = CreateWindowExW(
        0, wc.lpszClassName, wtitle, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        NULL, NULL, g_hinst, NULL);
    if (!g_hwnd) return false;

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    g_should_close = false;
    return true;
}

void PollEvents(void) {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_should_close = true;
        } else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}

bool WindowShouldClose(void) {
    return g_should_close;
}

void CloseWindowSB(void) {     // <-- renamed
    if (g_glrc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_glrc); g_glrc = NULL; }
    if (g_hdc)  { ReleaseDC(g_hwnd, g_hdc); g_hdc = NULL; }
    if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = NULL; }  // this is WinAPI DestroyWindow(HWND)
    g_should_close = false;
}

void SetWindowTitle(const char* title) {
    if (!g_hwnd || !title) return;
    wchar_t wtitle[512];
    utf8_to_wide(title, wtitle, 512);
    SetWindowTextW(g_hwnd, wtitle);
}

void GetFramebufferSize(int* outWidth, int* outHeight) {
    if (!g_hwnd) return;
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    if (outWidth)  *outWidth  = rc.right - rc.left;
    if (outHeight) *outHeight = rc.bottom - rc.top;
}

// Create modern OpenGL context (3.3 core if available)
bool CreateGLContext(void) {
    if (!g_hwnd) return false;
    if (g_glrc)  return true;

    g_hdc = GetDC(g_hwnd);

    // Choose and set pixel format
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits= 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(g_hdc, &pfd);
    if (pf == 0 || !SetPixelFormat(g_hdc, pf, &pfd)) {
        return false;
    }

    // Create temp context to load extensions
    HGLRC temp = wglCreateContext(g_hdc);
    if (!temp) return false;
    if (!wglMakeCurrent(g_hdc, temp)) {
        wglDeleteContext(temp);
        return false;
    }

    p_wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    p_wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    p_wglGetSwapIntervalEXT =
        (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");

    // Try to create a 3.3 core context
    HGLRC modern = NULL;
    if (p_wglCreateContextAttribsARB) {
        const int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            // WGL_CONTEXT_FLAGS_ARB,       WGL_CONTEXT_DEBUG_BIT_ARB, // optional
            0
        };
        modern = p_wglCreateContextAttribsARB(g_hdc, 0, attribs);
    }

    if (modern) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(temp);
        g_glrc = modern;
    } else {
        // Fall back to legacy context
        g_glrc = temp;
    }

    #include "../src/sb_gl_loader.h"

    if (!wglMakeCurrent(g_hdc, g_glrc)) { /* handle error */ }

    #ifdef _WIN32
    if (!sbgl_load_win32()) {
        // Couldn’t load required GL functions
        return false;
    }
    #endif


    // Default vsync to ON (if supported)
    if (p_wglSwapIntervalEXT) {
        p_wglSwapIntervalEXT(1);
    }
    return true;
}

void GL_SwapBuffers(void) {
    if (g_hdc) SwapBuffers(g_hdc);
}

void GL_SetSwapInterval(int interval) {
    if (p_wglSwapIntervalEXT) p_wglSwapIntervalEXT(interval);
}
