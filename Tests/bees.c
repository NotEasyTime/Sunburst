// stress_bees.c
#include "../src/sunburst.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
  #include <windows.h>
  static double now_seconds(void){
      static LARGE_INTEGER freq;
      static int inited = 0;
      if(!inited){ QueryPerformanceFrequency(&freq); inited = 1; }
      LARGE_INTEGER t; QueryPerformanceCounter(&t);
      return (double)t.QuadPart / (double)freq.QuadPart;
  }
#else
  #include <sys/time.h>
  #include <time.h>
  static double now_seconds(void){
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
  }
#endif

// ---------- Config ----------
static int   g_window_w = 1600;
static int   g_window_h = 900;
static int   g_fbw = 1600, g_fbh = 900;
static int   g_initial_bees = 10000;     // default; override via argv
static int   g_bee_size_px  = 32;        // width/height per sprite
static float g_speed_min    = 40.0f;     // px/s
static float g_speed_max    = 240.0f;    // px/s
static int   g_vsync        = 0;         // start with vsync OFF (toggle with 'V')

// ---------- Data ----------
typedef struct {
    float x, y;     // position
    float vx, vy;   // velocity
    float s;        // uniform scale (applied to bee_size_px)
} Bee;

static Bee*   g_bees       = NULL;
static int    g_bee_count  = 0;      // current live bees (<= g_bee_cap)
static int    g_bee_cap    = 0;      // allocated capacity
static Texture g_bee_tex;            // bee texture

// ---------- Utils ----------
static float frand01(void){
    return (float)rand() / (float)RAND_MAX;
}
static float frand_range(float a, float b){
    return a + (b - a) * frand01();
}

static void ensure_capacity(int new_cap){
    if (new_cap <= g_bee_cap) return;
    Bee* nb = (Bee*)realloc(g_bees, (size_t)new_cap * sizeof(Bee));
    if (!nb){
        fprintf(stderr, "Failed to allocate bees for %d\n", new_cap);
        exit(1);
    }
    g_bees = nb;
    g_bee_cap = new_cap;
}

static void spawn_bees(int target_count){
    ensure_capacity(target_count);
    for (int i = g_bee_count; i < target_count; ++i){
        float w = (float)(g_bee_size_px);
        float h = (float)(g_bee_size_px);
        float s = frand_range(0.75f, 1.5f);
        float px = frand_range(0.0f, (float)g_fbw - w * s);
        float py = frand_range(0.0f, (float)g_fbh - h * s);
        float sp = frand_range(g_speed_min, g_speed_max);
        float ang = frand_range(0.0f, 6.2831853f); // 2π
        g_bees[i].x = px;
        g_bees[i].y = py;
        g_bees[i].vx = cosf(ang) * sp;
        g_bees[i].vy = sinf(ang) * sp;
        g_bees[i].s = s;
    }
    g_bee_count = target_count;
}

static void shrink_bees(int target_count){
    if (target_count < 0) target_count = 0;
    if (target_count > g_bee_cap) target_count = g_bee_cap;
    g_bee_count = target_count;
}

static void toggle_vsync(void){
    g_vsync = !g_vsync;
    GL_SetSwapInterval(g_vsync ? 1 : 0);
    printf("[stress_bees] VSync: %s\n", g_vsync ? "ON" : "OFF");
}

// Fixed-timestep simulation
static void sim_step(float dt){
    // bounce inside framebuffer
    for (int i = 0; i < g_bee_count; ++i){
        Bee* b = &g_bees[i];
        float w = g_bee_size_px * b->s;
        float h = g_bee_size_px * b->s;

        b->x += b->vx * dt;
        b->y += b->vy * dt;

        if (b->x < 0.0f)          { b->x = 0.0f;               b->vx = -b->vx; }
        if (b->y < 0.0f)          { b->y = 0.0f;               b->vy = -b->vy; }
        if (b->x + w > g_fbw)     { b->x = g_fbw - w;          b->vx = -b->vx; }
        if (b->y + h > g_fbh)     { b->y = g_fbh - h;          b->vy = -b->vy; }
    }
}

static void draw_bees(void){
    for (int i = 0; i < g_bee_count; ++i){
        Bee* b = &g_bees[i];
        int w = (int)(g_bee_size_px * b->s);
        int h = (int)(g_bee_size_px * b->s);
        DrawTexture(&g_bee_tex, (int)b->x, (int)b->y, w, h, true);
    }
}

static void usage(const char* exe){
    fprintf(stderr,
        "Usage: %s [count] [size_px] [vsync]\n"
        "  count    : number of bees (default %d)\n"
        "  size_px  : sprite size in pixels (default %d)\n"
        "  vsync    : 0 or 1 (default %d)\n"
        "\nRuntime controls:\n"
        "  V  : toggle VSync\n"
        "  B  : double bee count (up to cap)\n"
        "  N  : halve bee count\n"
        "  ESC: quit\n",
        exe, g_initial_bees, g_bee_size_px, g_vsync);
}

// ---------- Main ----------
int main(int argc, char** argv){
    if (argc >= 2) g_initial_bees = atoi(argv[1] ? argv[1] : "0");
    if (argc >= 3) g_bee_size_px  = atoi(argv[2] ? argv[2] : "0");
    if (argc >= 4) g_vsync        = atoi(argv[3] ? argv[3] : "0");
    if (g_initial_bees <= 0) g_initial_bees = 10000;
    if (g_bee_size_px  <= 0) g_bee_size_px  = 32;
    if (g_vsync != 0 && g_vsync != 1) g_vsync = 0;

    srand((unsigned)time(NULL));

    InitWindow(g_window_w, g_window_h, "Sunburst Stress Test - Bees");
    CreateGLContext();
    GL_SetSwapInterval(g_vsync ? 1 : 0);
    RendererInit();

    g_bee_tex = LoadTexture("bee.png");
    GetFramebufferSize(&g_fbw, &g_fbh);

    // Pre-allocate a big pool so you can crank it up live without realloc cost.
    // Feel free to raise this if you want to go wild.
    int initial_cap = g_initial_bees;
    int max_cap = initial_cap * 4; // up to 4x live growth with 'B'
    ensure_capacity(max_cap);
    spawn_bees(g_initial_bees);

    // Fixed timestep sim at 120 Hz
    const double DT = 1.0/120.0;
    double t_cur = now_seconds();
    double acc = 0.0;
    double hud_acc = 0.0;
    int frames = 0;

    printf("[stress_bees] Starting with %d bees @ %d px, vsync=%d\n",
           g_bee_count, g_bee_size_px, g_vsync);
    printf("[stress_bees] Controls: V=toggle vsync, B=double, N=halve, ESC=quit\n");

    while (!WindowShouldClose()){
        // Timing
        double t_new = now_seconds();
        double frame = t_new - t_cur;
        if (frame > 0.25) frame = 0.25; // avoid spiral of death
        t_cur = t_new;
        acc += frame;
        hud_acc += frame;

        // Input
        InputBeginFrame();
        PollEvents();

        if (IsKeyPressed(KEY_ESCAPE)) break;
        if (IsKeyPressed(KEY_V)) toggle_vsync();
        if (IsKeyPressed(KEY_B)){
            int next = g_bee_count * 2;
            if (next > g_bee_cap) next = g_bee_cap;
            spawn_bees(next);
            printf("[stress_bees] Bees: %d (cap %d)\n", g_bee_count, g_bee_cap);
        }
        if (IsKeyPressed(KEY_N)){
            int next = g_bee_count / 2;
            shrink_bees(next);
            printf("[stress_bees] Bees: %d (cap %d)\n", g_bee_count, g_bee_cap);
        }

        // Simulate
        while (acc >= DT){
            GetFramebufferSize(&g_fbw, &g_fbh);
            sim_step((float)DT);
            acc -= DT;
        }

        // Render
        Begin2D();
        ClearBackground();
        draw_bees();
        End2D();

        PrintFrameRate();

        // Console HUD once a second
        frames++;
        if (hud_acc >= 1.0){
            printf("[stress_bees] bees=%d  tex=%dx%d  fb=%dx%d  vsync=%d\n",
                   g_bee_count, g_bee_tex.width, g_bee_tex.height, g_fbw, g_fbh, g_vsync);
            hud_acc -= 1.0;
            frames = 0;
        }
    }

    DestroyTexture(&g_bee_tex);
    free(g_bees);
    RendererShutdown();
    CloseWindowSB();
    return 0;
}
