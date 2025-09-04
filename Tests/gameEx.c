#include "../sunburst.h"
#include <stdio.h>

int main(void) {
    macwin_App* app = macwin_app_create();
    if (!app) return 1;

    macwin_Window* w = macwin_window_create(640, 480, "Hello from C", 1);
    macwin_window_show(w);

    // Option A: blocking Cocoa run loop
    macwin_app_run(app);

    // Option B: custom loop (uncomment to use instead of app_run)
    // while (macwin_app_poll(app)) {
    //     // do your work here...
    // }

    return 0;
}
