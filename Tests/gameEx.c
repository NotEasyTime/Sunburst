#include "../src/sunburst.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

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
  #include <unistd.h>
  #include <sys/time.h>
  #if defined(__APPLE__) || defined(__linux__)
    #include <time.h>
    static double now_seconds(void){
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
    }
  #else
    static double now_seconds(void){
        struct timeval tv; gettimeofday(&tv, NULL);
        return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
    }
  #endif
#endif

/* ---------------- Window / Render ---------------- */
int WindowWidth = 1600;
int WindowHeight = 800;
int fbw, fbh;
Texture bee;

/* ---------------- Player ---------------- */
typedef struct Bouncer{
    float x, y;                 // position (px)
    int   width, height;        // size (px)
    float vx, vy;               // velocity (px/s)
    float flapImpulse;          // vy set/add on flap (px/s)
    float gravity;              // px/s^2
    Color c;
} Bouncer;

static inline void DrawBouncer(const Bouncer *b){
    DrawTexture(&bee, (int)b->x, (int)b->y, b->width, b->height, true);
}

static inline void Physics(Bouncer *b, float dt){
    // integrate
    b->vy += b->gravity * dt;
    b->x  += b->vx * dt;
    b->y  += b->vy * dt;

    // floor/ceiling hard fail (Flappy rules)
    if (b->y < 0) {
        b->y = 0;
    }
    float floorY = (float)fbh - b->height;
    if (b->y > floorY) {
        b->y = floorY;
    }
}

/* --------------- Pipes ------------------ */
typedef struct {
    float x;        // left edge (px)
    int   width;    // px
    int   gapY;     // top of gap (px)
    int   gapH;     // height of gap (px)
    bool  active;
    bool  scored;
} Pipe;

#define MAX_PIPES 64
static Pipe pipes[MAX_PIPES];

static inline void ResetPipes(void){
    for (int i = 0; i < MAX_PIPES; ++i) pipes[i].active = pipes[i].scored = false;
}

static inline int RandRange(int lo, int hi){ return lo + (rand() % (hi - lo + 1)); }

static inline void SpawnPipe(int screenW, int screenH, int pipeW, int gapH){
    int slot = -1;
    for (int i = 0; i < MAX_PIPES; ++i) if (!pipes[i].active){ slot = i; break; }
    if (slot < 0) return;

    int margin = 40;
    int gapY = RandRange(margin, screenH - gapH - margin);

    pipes[slot].x      = (float)screenW + pipeW; // off right
    pipes[slot].width  = pipeW;
    pipes[slot].gapY   = gapY;
    pipes[slot].gapH   = gapH;
    pipes[slot].active = true;
    pipes[slot].scored = false;
}

static inline void UpdatePipes(float speed_px_s, float dt){
    float dx = speed_px_s * dt;
    for (int i = 0; i < MAX_PIPES; ++i){
        if (!pipes[i].active) continue;
        pipes[i].x -= dx;
        if ((pipes[i].x + pipes[i].width) < 0) pipes[i].active = false;
    }
}

static inline void DrawPipe(const Pipe* p){
    // top rect
    DrawRectangle((int)p->x, 0, p->width, p->gapY, (Color){0.1f, 0.8f, 0.3f, 1.0f});
    // bottom rect
    int bottomY = p->gapY + p->gapH;
    int bottomH = fbh - bottomY;
    if (bottomH > 0){
        DrawRectangle((int)p->x, bottomY, p->width, bottomH, (Color){0.1f, 0.7f, 0.25f, 1.0f});
    }
}

/* --------------- Utils ------------------ */
static inline bool AABB(int ax, int ay, int aw, int ah,
                        int bx, int by, int bw, int bh){
    return (ax < bx + bw) && (ax + aw > bx) &&
           (ay < by + bh) && (ay + ah > by);
}

static inline void EndGame(int score){
    printf("\nGame Over! Score = %d\n", score);
    DestroyTexture(&bee);
    RendererShutdown();
    CloseWindowSB();
    exit(0);
}

/* --------------- Main ------------------- */
int main(void) {
    srand((unsigned)time(NULL));

    InitWindow(WindowWidth, WindowHeight, "Flappy Bee (Fixed Timestep)");
    CreateGLContext();

    // Toggle freely; physics won’t change:
    // 1 = vsync on, 0 = off
    GL_SetSwapInterval(0);

    RendererInit();
    bee = LoadTexture("bee.png");

    Bouncer beeb = {
        .x = 200.0f, .y = 300.0f,
        .width = 100, .height = 100,
        .vx = 0.0f, .vy = 0.0f,
        .flapImpulse = -1100.0f,    // px/s (tune)
        .gravity = 2400.0f,         // px/s^2 (tune)
        .c = {1,0,0,1}
    };

    // Pipe settings (units per second)
    const float PIPE_SPEED = 360.0f;  // px/s
    const int   PIPE_WIDTH = 120;
    int         GAP_H      = 520;     // px

    // Spawn timing (seconds)
    const float SPAWN_INTERVAL = 1.35f;
    float spawn_t = 0.0f;

    int score = 0;
    ResetPipes();

    // -------- Fixed timestep loop --------
    const double DT = 1.0 / 120.0; // 120 Hz simulation step
    double curTime = now_seconds();
    double acc = 0.0;

    while (!WindowShouldClose()) {
        // --- Time accumulation ---
        double newTime = now_seconds();
        double frame = newTime - curTime;
        if (frame > 0.25) frame = 0.25; // avoid spiral of death
        curTime = newTime;
        acc += frame;

        // --- Input sampling (event-driven) ---
        InputBeginFrame();
        PollEvents();

        if (IsKeyPressed(KEY_ESCAPE)) EndGame(score);
        if (IsKeyPressed(KEY_SPACE))  beeb.vy = beeb.flapImpulse;

        // --- Simulate in fixed steps ---
        while (acc >= DT) {
            float dt = (float)DT;

            GetFramebufferSize(&fbw, &fbh);

            // Player physics
            Physics(&beeb, dt);

            // Out-of-bounds = end
            if (beeb.y <= 0 || (beeb.y + beeb.height) >= fbh) {
                EndGame(score);
            }

            // Spawn logic
            spawn_t -= dt;
            if (spawn_t <= 0.0f){
                SpawnPipe(fbw, fbh, PIPE_WIDTH, GAP_H);
                spawn_t += SPAWN_INTERVAL;
            }

            // Pipes movement
            UpdatePipes(PIPE_SPEED, dt);

            // Collisions + scoring
            for (int i = 0; i < MAX_PIPES; ++i){
                if (!pipes[i].active) continue;

                // Collision (top)
                if (AABB((int)beeb.x, (int)beeb.y, beeb.width, beeb.height,
                         (int)pipes[i].x, 0, pipes[i].width, pipes[i].gapY)){
                    EndGame(score);
                }
                // Collision (bottom)
                int bottomY = pipes[i].gapY + pipes[i].gapH;
                if (AABB((int)beeb.x, (int)beeb.y, beeb.width, beeb.height,
                         (int)pipes[i].x, bottomY, pipes[i].width, fbh - bottomY)){
                    EndGame(score);
                }

                // Scoring (when pipe right edge passes player x)
                if (!pipes[i].scored && (pipes[i].x + pipes[i].width) < beeb.x){
                    pipes[i].scored = true;
                    score++;
                }
            }

            acc -= DT;
        }

        // --- Render once per frame (we skip interpolation for simplicity) ---
        Begin2D();
        ClearBackground();

        // Draw pipes
        for (int i = 0; i < MAX_PIPES; ++i){
            if (pipes[i].active) DrawPipe(&pipes[i]);
        }

        // Draw player
        DrawBouncer(&beeb);

        End2D();

        PrintFrameRate();
    }

    DestroyTexture(&bee);
    RendererShutdown();
    CloseWindowSB();
    return 0;
}
