// sunburst_x11.c — Linux X11 implementation for sunburst.h C API
// Build:
//   cc demo.c sunburst_x11.c -o demo -lX11
// Notes:
//   • Requires an X11 server (Xorg or XWayland).
//   • We call XInitThreads() so macwin_app_quit can be called from another thread.

#define _POSIX_C_SOURCE 200809L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

#include "sunburst.h"

struct macwin_App {
    Display* dpy;
    int      screen;
    Atom     atom_wm_delete;
    Atom     atom_wm_protocols;
    Atom     atom_utf8_string;
    Atom     atom_net_wm_name;

    // Window tracking
    volatile long window_count;
    volatile int  running;

    // Wake mechanism for macwin_app_quit (safe from any thread)
    int wake_pipe[2]; // [0] = read end, [1] = write end

    // For mapping Window -> wrapper pointer
    XContext win_ctx;
};

struct macwin_Window {
    Window            win;
    struct macwin_App* app;
    int               resizable;
    int               width;
    int               height;
};

// Singleton-ish current app (mirrors Cocoa's NSApp pattern)
static struct macwin_App* g_current_app = NULL;

// ------------------------ helpers ------------------------

static void set_utf8_title(struct macwin_App* app, Window w, const char* title) {
    if (!title) title = "";
    XChangeProperty(app->dpy, w,
                    app->atom_net_wm_name,
                    app->atom_utf8_string, 8,
                    PropModeReplace,
                    (const unsigned char*)title,
                    (int)strlen(title));
    // Also set legacy name for older WMs
    XStoreName(app->dpy, w, title);
}

static void apply_size_hints(struct macwin_App* app, Window w, int w_px, int h_px, int resizable) {
    XSizeHints hints;
    memset(&hints, 0, sizeof(hints));
    hints.flags = PSize | PMinSize;
    hints.width  = w_px;
    hints.height = h_px;
    hints.min_width  = 64;  // sensible floor
    hints.min_height = 64;

    if (!resizable) {
        hints.flags |= PMaxSize;
        hints.min_width  = hints.max_width  = w_px;
        hints.min_height = hints.max_height = h_px;
    }
    XSetWMNormalHints(app->dpy, w, &hints);
}

static void register_protocols(struct macwin_App* app, Window w) {
    XSetWMProtocols(app->dpy, w, &app->atom_wm_delete, 1);
}

// Centralized event dispatch. Returns 0 if the app should stop, 1 to continue.
static int dispatch_event(struct macwin_App* app, XEvent* ev) {
    switch (ev->type) {
    case ClientMessage: {
        // WM_DELETE_WINDOW from a top-level window?
        if ((Atom)ev->xclient.message_type == app->atom_wm_protocols &&
            (Atom)ev->xclient.data.l[0] == app->atom_wm_delete) {
            // Find our wrapper and destroy the window
            XPointer ptr = NULL;
            if (XFindContext(app->dpy, ev->xclient.window, app->win_ctx, &ptr) == 0 && ptr) {
                struct macwin_Window* wrap = (struct macwin_Window*)ptr;
                if (wrap->win) {
                    XDestroyWindow(app->dpy, wrap->win);
                    // do not free here; free on DestroyNotify
                    wrap->win = 0;
                }
            } else {
                // If we don't know it, still destroy the X window
                XDestroyWindow(app->dpy, ev->xclient.window);
            }
        }
        return 1;
    }
    case DestroyNotify: {
        // A top-level window got destroyed; decrement and free wrapper (if we own it).
        XPointer ptr = NULL;
        if (XFindContext(app->dpy, ev->xdestroywindow.window, app->win_ctx, &ptr) == 0 && ptr) {
            struct macwin_Window* wrap = (struct macwin_Window*)ptr;
            XDeleteContext(app->dpy, ev->xdestroywindow.window, app->win_ctx);
            // Decrement window count exactly once here
            if (app->window_count > 0) app->window_count--;
            free(wrap);
        }
        // If last window closed, we can stop the app automatically.
        if (app->window_count <= 0) app->running = 0;
        return 1;
    }
    default:
        return 1;
    }
}

// ------------------------ API implementation ------------------------

macwin_App* macwin_app_create(void) {
    // Enable Xlib thread safety (allows wake/quit from other threads)
    XInitThreads();

    macwin_App* app = (macwin_App*)calloc(1, sizeof(*app));
    if (!app) return NULL;

    app->dpy = XOpenDisplay(NULL);
    if (!app->dpy) {
        free(app);
        return NULL;
    }

    app->screen = DefaultScreen(app->dpy);
    app->atom_wm_delete    = XInternAtom(app->dpy, "WM_DELETE_WINDOW", False);
    app->atom_wm_protocols = XInternAtom(app->dpy, "WM_PROTOCOLS", False);
    app->atom_utf8_string  = XInternAtom(app->dpy, "UTF8_STRING", False);
    app->atom_net_wm_name  = XInternAtom(app->dpy, "_NET_WM_NAME", False);

    app->window_count = 0;
    app->running = 0;

    // Create a wake pipe so macwin_app_quit can unblock macwin_app_run
    if (pipe(app->wake_pipe) != 0) {
        XCloseDisplay(app->dpy);
        free(app);
        return NULL;
    }

    app->win_ctx = XUniqueContext();
    g_current_app = app;
    return app;
}

macwin_Window* macwin_window_create(int width, int height, const char* title, int resizable) {
    if (!g_current_app) return NULL;
    macwin_App* app = g_current_app;

    macwin_Window* wrap = (macwin_Window*)calloc(1, sizeof(*wrap));
    if (!wrap) return NULL;

    Display* dpy = app->dpy;
    int scr = app->screen;
    Window root = RootWindow(dpy, scr);

    // Create top-level window
    unsigned long black = BlackPixel(dpy, scr);
    unsigned long white = WhitePixel(dpy, scr);

    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof(swa));
    swa.background_pixel = white;
    swa.event_mask = StructureNotifyMask; // we care about DestroyNotify

    Window win = XCreateWindow(
        dpy, root,
        0, 0, (unsigned)width, (unsigned)height, /*border*/0,
        CopyFromParent /*depth*/,
        InputOutput,
        CopyFromParent /*visual*/,
        CWBackPixel | CWEventMask,
        &swa
    );

    if (!win) {
        free(wrap);
        return NULL;
    }

    set_utf8_title(app, win, title ? title : "");
    apply_size_hints(app, win, width, height, resizable);
    register_protocols(app, win);

    // Remember wrapper for this Window
    if (XSaveContext(dpy, win, app->win_ctx, (XPointer)wrap) != 0) {
        XDestroyWindow(dpy, win);
        free(wrap);
        return NULL;
    }

    wrap->win = win;
    wrap->app = app;
    wrap->resizable = resizable;
    wrap->width = width;
    wrap->height = height;

    return wrap;
}

void macwin_window_show(macwin_Window* win) {
    if (!win || !win->app || !win->app->dpy || !win->win) return;
    XMapRaised(win->app->dpy, win->win);
    XFlush(win->app->dpy);
    // Count this as a live window once mapped
    win->app->window_count++;
}

void macwin_window_set_title(macwin_Window* win, const char* title) {
    if (!win || !win->app || !win->app->dpy || !win->win) return;
    set_utf8_title(win->app, win->win, title ? title : "");
    XFlush(win->app->dpy);
}

void macwin_window_destroy(macwin_Window* win) {
    if (!win || !win->app || !win->app->dpy) return;
    if (win->win) {
        // Destroying triggers DestroyNotify where we free the wrapper & decrement count.
        XDestroyWindow(win->app->dpy, win->win);
        XFlush(win->app->dpy);
        win->win = 0;
    }
    // Do NOT free(win) here; we free in DestroyNotify to avoid double-free if user also closes the window.
}

void macwin_app_quit(macwin_App* app) {
    if (!app) return;
    app->running = 0;
    // Wake up the blocking select() in macwin_app_run
    const uint8_t byte = 0xFF;
    (void)write(app->wake_pipe[1], &byte, 1);
    // No need to XLockDisplay just to flip a flag/send pipe
}

void macwin_app_run(macwin_App* app) {
    if (!app || !app->dpy) return;
    g_current_app = app;
    app->running = 1;

    const int xfd = ConnectionNumber(app->dpy);
    const int pfd = app->wake_pipe[0];
    int maxfd = (xfd > pfd ? xfd : pfd) + 1;

    while (app->running) {
        // First drain any pending X events
        while (XPending(app->dpy) > 0) {
            XEvent ev;
            XNextEvent(app->dpy, &ev);
            if (!dispatch_event(app, &ev)) {
                app->running = 0;
                break;
            }
        }
        if (!app->running) break;
        if (app->window_count <= 0) { app->running = 0; break; }

        // Block until either X has something or we get a byte on the wake pipe
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(xfd, &rfds);
        FD_SET(pfd, &rfds);
        int rc = select(maxfd, &rfds, NULL, NULL, NULL);
        if (rc < 0) {
            if (errno == EINTR) continue;
            // On irrecoverable error, bail
            app->running = 0;
            break;
        }
        if (FD_ISSET(pfd, &rfds)) {
            // Drain wake byte(s)
            uint8_t buf[64];
            (void)read(pfd, buf, sizeof(buf));
            // running may have been set to 0, loop will exit
        }
        // X events (if any) will be handled on the next iteration's XPending loop
    }

    g_current_app = NULL;
}

int macwin_app_poll(macwin_App* app) {
    if (!app || !app->dpy) return 0;
    g_current_app = app;
    app->running = 1;

    // Process all queued events quickly
    while (XPending(app->dpy) > 0) {
        XEvent ev;
        XNextEvent(app->dpy, &ev);
        if (!dispatch_event(app, &ev)) {
            app->running = 0;
            break;
        }
    }
    if (app->window_count <= 0) app->running = 0;

    return app->running ? 1 : 0;
}
