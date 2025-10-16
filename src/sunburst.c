#include "sunburst.h"

int CheckRectsOverlap(const Rect* a, const Rect* b) {
    if (!a || !b) return 0;
    return !(a->x + a->width  <= b->x ||
             b->x + b->width  <= a->x ||
             a->y + a->height <= b->y ||
             b->y + b->height <= a->y);
}

int CheckRectPoint(const Rect* r, float px, float py) {
    if (!r) return 0;
    return (px >= r->x && px <= r->x + r->width &&
            py >= r->y && py <= r->y + r->height);
}

int GetRectOverlap(const Rect* a, const Rect* b, Rect* result) {
    if (!a || !b || !result) return 0;

    if (!CheckRectsOverlap(a, b)) return 0;

    float x1 = (a->x > b->x) ? a->x : b->x;
    float y1 = (a->y > b->y) ? a->y : b->y;
    float x2 = ((a->x + a->width)  < (b->x + b->width))  ? (a->x + a->width)  : (b->x + b->width);
    float y2 = ((a->y + a->height) < (b->y + b->height)) ? (a->y + a->height) : (b->y + b->height);

    result->x = x1;
    result->y = y1;
    result->width  = x2 - x1;
    result->height = y2 - y1;
    return 1;
}

#if defined(_WIN32)
  #include <windows.h>
  static double now_seconds(void) {
      static LARGE_INTEGER freq = {0};
      if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
      LARGE_INTEGER t; QueryPerformanceCounter(&t);
      return (double)t.QuadPart / (double)freq.QuadPart;
  }
#elif defined(__APPLE__)
  #include <mach/mach_time.h>
  #include <stdio.h>
  static double now_seconds(void) {
      static mach_timebase_info_data_t tb; if (!tb.denom) mach_timebase_info(&tb);
      uint64_t t = mach_absolute_time();
      double ns = (double)t * (double)tb.numer / (double)tb.denom;
      return ns * 1e-9;
  }
#else
  #include <time.h>
  static double now_seconds(void) {
      struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
      return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
  }
#endif


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

void ClearBackground(){
    int w=0,h=0;
    GetFramebufferSize(&w,&h);
    glViewport(0,0,w,h);
    glClearColor(0.08f,0.09f,0.11f,1.0f);
    glClear(0x00004000);
}
