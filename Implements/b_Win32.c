// b_Win32.c — Windows (Win32) implementation for sunburst.h C API
// Build (MSVC):
//   cl /O2 /std:c11 sunburst_win.c /link user32.lib gdi32.lib
// Build (MinGW):
//   x86_64-w64-mingw32-gcc -O2 -std=c11 sunburst_win.c -o demo.exe -luser32 -lgdi32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sunburst.h"

// -------------------- Internal types --------------------

struct macwin_App {
    HINSTANCE   hinst;
    ATOM        wndclass_atom;
    DWORD       main_thread_id;
    volatile LONG window_count;
    volatile LONG running; // 1 while app loop active (or until WM_QUIT)
};

struct macwin_Window {
    HWND                 hwnd;
    struct macwin_App*   app;
};

// -------------------- UTF-8 -> UTF-16 helpers --------------------

static wchar_t* utf8_to_wide(const char* s) {
    if (!s) {
        wchar_t* empty = (wchar_t*)calloc(1, sizeof(wchar_t));
        if (empty) empty[0] = L'\0';
        return empty;
    }
    int needed = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (needed <= 0) return NULL;
    wchar_t* w = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)needed);
    if (!w) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, w, needed)) {
        free(w);
        return NULL;
    }
    return w;
}

// -------------------- Window proc --------------------

static LRESULT CALLBACK sunburst_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    struct macwin_Window* win = (struct macwin_Window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
        // Stash pointer early if provided via CreateWindowEx lpParam
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        if (cs && cs->lpCreateParams) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (win && win->app) {
            LONG remaining = InterlockedDecrement(&(win->app->window_count));
            (void)remaining;
            // If you want “quit when last window closes”, uncomment:
            // if (remaining == 0) PostThreadMessageW(win->app->main_thread_id, WM_QUIT, 0, 0);
        }
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// -------------------- Class registration --------------------

static ATOM register_window_class(HINSTANCE hinst) {
    const wchar_t* clsname = L"SunburstWindowClass";
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = sunburst_wndproc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hinst;
    wc.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = clsname;
    wc.hIconSm       = wc.hIcon;
    return RegisterClassExW(&wc);
}

// -------------------- API implementation --------------------

macwin_App* macwin_app_create(void) {
    macwin_App* app = (macwin_App*)calloc(1, sizeof(*app));
    if (!app) return NULL;

    app->hinst = GetModuleHandleW(NULL);
    app->main_thread_id = GetCurrentThreadId();
    app->window_count = 0;
    app->running = 0;

    ATOM atom = register_window_class(app->hinst);
    if (!atom) {
        free(app);
        return NULL;
    }
    app->wndclass_atom = atom;
    return app;
}

static DWORD window_style_from_resizable(int resizable) {
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!resizable) {
        // Remove maximize + thick frame to prevent resizing
        style &= ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
    }
    return style;
}

macwin_Window* macwin_window_create(int width, int height, const char* title, int resizable) {
    macwin_Window* w = (macwin_Window*)calloc(1, sizeof(*w));
    if (!w) return NULL;

    // We assume the app has been created; grab the one from thread-local context? User passes nothing.
    // The header implies the app must be created first and we operate under that assumption.
    // We’ll look up the most recent app by a static (not thread-safe if multiple apps).
    // Simpler: require user to call this only after macwin_app_create and before run/poll.
    // We will store the app pointer later via SetWindowLongPtr in WM_NCCREATE.
    // But we still need the HINSTANCE and class; get from module handle.
    HINSTANCE hinst = GetModuleHandleW(NULL);

    wchar_t* wtitle = utf8_to_wide(title);
    if (!wtitle) { free(w); return NULL; }

    // Desired client size => adjust window rect to include nonclient area
    RECT r = {0, 0, width, height};
    DWORD style = window_style_from_resizable(resizable);
    DWORD exstyle = WS_EX_APPWINDOW;
    AdjustWindowRectEx(&r, style, FALSE, exstyle);
    int win_w = r.right - r.left;
    int win_h = r.bottom - r.top;

    // NOTE: We will patch in the app pointer after creation—client will call show/run later.
    // To make this robust, we’ll first try to find an existing app by scanning the window class.
    // Simpler: we delay incrementing window_count until macwin_window_show where we must have an app.
    // Instead, we accept passing the app as global—keep it simple: store a static last created app.
    // To avoid globals, we will add a tiny helper that retrieves the last app via a static variable.

    // Minimal approach: set lpParam to 'w' so WM_NCCREATE can stash it in GWLP_USERDATA.
    HWND hwnd = CreateWindowExW(
        exstyle,
        L"SunburstWindowClass",
        wtitle,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        win_w, win_h,
        NULL, NULL, hinst, (LPVOID)w);
    free(wtitle);

    if (!hwnd) {
        free(w);
        return NULL;
    }

    w->hwnd = hwnd;
    // app will be set when shown (we need it to increment counts); users typically create app first.
    // We can try to look up the app pointer from a module static:
    return w;
}

// We’ll keep a single “current app” to associate windows with.
// This mirrors the Cocoa singleton model and keeps the API simple.
static macwin_App* g_current_app = NULL;

void macwin_window_show(macwin_Window* win) {
    if (!win) return;
    // Associate with the current app if not already
    if (!g_current_app) {
        // Likely user forgot to call macwin_app_create; be forgiving but do nothing fatal.
        // You could fprintf here in your own codebase.
    } else if (!win->app) {
        win->app = g_current_app;
        InterlockedIncrement(&(win->app->window_count));
    }

    ShowWindow(win->hwnd, SW_SHOW);
    UpdateWindow(win->hwnd);
}

void macwin_window_set_title(macwin_Window* win, const char* title) {
    if (!win) return;
    wchar_t* wtitle = utf8_to_wide(title);
    if (!wtitle) return;
    SetWindowTextW(win->hwnd, wtitle);
    free(wtitle);
}

void macwin_window_destroy(macwin_Window* win) {
    if (!win) return;
    if (win->hwnd) {
        DestroyWindow(win->hwnd);
        win->hwnd = NULL;
    }
    free(win);
}

void macwin_app_quit(macwin_App* app) {
    if (!app) return;
    // Post quit to the main thread so this can be called from any thread
    PostThreadMessageW(app->main_thread_id, WM_QUIT, 0, 0);
}

void macwin_app_run(macwin_App* app) {
    if (!app) return;
    g_current_app = app; // set singleton for subsequent window creations/shows
    app->running = 1;

    MSG msg;
    // Standard blocking message loop
    while (1) {
        BOOL ok = GetMessageW(&msg, NULL, 0, 0);
        if (ok <= 0) {
            // WM_QUIT (0) or error (-1)
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    app->running = 0;
    g_current_app = NULL;
}

int macwin_app_poll(macwin_App* app) {
    if (!app) return 0;
    g_current_app = app; // ensure windows associate to this app
    app->running = 1;

    MSG msg;
    // Pump all pending messages quickly (one “iteration” of the loop).
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            app->running = 0;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // If no windows remain, you may choose to stop automatically:
    // if (app->window_count <= 0) app->running = 0;

    return app->running ? 1 : 0;
}
