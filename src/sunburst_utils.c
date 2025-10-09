#include "sunburst.h"
#include <stdio.h>

#if defined(__APPLE__)
  #include <mach/mach_time.h>
#else
  #include <time.h>
#endif

static double now_seconds(void) {
#if defined(__APPLE__)
    static mach_timebase_info_data_t tb;
    if (!tb.denom) mach_timebase_info(&tb);
    uint64_t t = mach_absolute_time();
    double ns = (double)t * (double)tb.numer / (double)tb.denom;
    return ns * 1e-9;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

void PrintFrameRate(void) {
    static double prevTime = 0.0;
    static int frames = 0;

    double current = now_seconds();
    if (prevTime == 0.0)
        prevTime = current;

    frames++;

    double elapsed = current - prevTime;
    if (elapsed >= 1.0) {
        double fps = (double)frames / elapsed;
        printf("FPS: %.1f\n", fps);
        frames = 0;
        prevTime = current;
    }
}
