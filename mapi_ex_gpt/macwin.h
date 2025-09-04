// macwin.h — C-callable wrapper around Cocoa windows
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle types
typedef struct macwin_App macwin_App;
typedef struct macwin_Window macwin_Window;

// Must be called on the main thread before any other call.
macwin_App* macwin_app_create(void);

// Create a window (returns NULL on failure)
macwin_Window* macwin_window_create(int width, int height, const char* title,
                                    int resizable /*0/1*/);

// Show the window
void macwin_window_show(macwin_Window* win);

// Set window title
void macwin_window_set_title(macwin_Window* win, const char* title);

// Close/destroy the window
void macwin_window_destroy(macwin_Window* win);

// Run the Cocoa event loop (blocking). Must be on the main thread.
void macwin_app_run(macwin_App* app);

// Optional: pump one iteration of the event loop (non-blocking).
// Returns 0 if the app should quit, 1 otherwise.
int macwin_app_poll(macwin_App* app);

// Request app termination (safe from any thread)
void macwin_app_quit(macwin_App* app);

#ifdef __cplusplus
}
#endif
