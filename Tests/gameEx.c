#include "../src/sunburst.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

int WindowWidth = 1600;
int WindowHeight = 800;
int fbw, fbh;
Texture bee;

typedef struct Bouncer{
    int x, y, width, height;
    float dx, dy;
    int jumpRate;
    Color c;
} Bouncer;

static inline void DrawBouncer(Bouncer *b){
    DrawTexture(&bee, b->x, b->y, b->width, b->height, true);
}

static inline void Physics(Bouncer *b) {
    // gravity
    b->dy += 0.6f;
    b->x  += (int)b->dx;
    b->y  += (int)b->dy;
}

static inline bool AABB(int ax, int ay, int aw, int ah,
                        int bx, int by, int bw, int bh){
    return (ax < bx + bw) && (ax + aw > bx) &&
           (ay < by + bh) && (ay + ah > by);
}

/* ---------------- Pipes ---------------- */

typedef struct {
    int x;          // left edge
    int width;      // pipe width
    int gapY;       // top of the gap
    int gapH;       // gap height
    bool active;    // on-screen / in-use
} Pipe;

#define MAX_PIPES 64
static Pipe pipes[MAX_PIPES];

static inline void ResetPipes(void){
    for (int i = 0; i < MAX_PIPES; ++i) pipes[i].active = false;
}

static inline int RandRange(int lo, int hi){
    return lo + (rand() % (hi - lo + 1));
}

static inline void SpawnPipe(int screenW, int screenH){
    // find free slot
    int slot = -1;
    for (int i = 0; i < MAX_PIPES; ++i){
        if (!pipes[i].active){ slot = i; break; }
    }
    if (slot < 0) return;

    const int pipeW = 120;
    const int gapH  = 420;           // tune for difficulty
    const int margin = 40;           // keep gap fully on-screen

    int gapY = RandRange(margin, screenH - gapH - margin);

    pipes[slot].x      = screenW + pipeW; // spawn just off right edge
    pipes[slot].width  = pipeW;
    pipes[slot].gapY   = gapY;
    pipes[slot].gapH   = gapH;
    pipes[slot].active = true;
}

static inline void UpdatePipes(int speedPx, int screenW){
    for (int i = 0; i < MAX_PIPES; ++i){
        if (!pipes[i].active) continue;
        pipes[i].x -= speedPx;
        if (pipes[i].x + pipes[i].width < 0){
            pipes[i].active = false; // off-screen to the left
        }
    }
}

static inline void DrawPipe(const Pipe* p){
    // top rect: (x, 0, width, gapY)
    DrawRectangle(p->x, 0, p->width, p->gapY, (Color){0.1f, 0.8f, 0.3f, 1.0f});
    // bottom rect: (x, gapY+gapH, width, screenH - (gapY+gapH))
    int bottomY = p->gapY + p->gapH;
    int bottomH = fbh - bottomY;
    if (bottomH > 0){
        DrawRectangle(p->x, bottomY, p->width, bottomH, (Color){0.1f, 0.7f, 0.25f, 1.0f});
    }
}

/* --------------- Game Over -------------- */

static inline void EndGame(int score){
    // Print to stdout, then exit immediately (per your ask)
    // If you want an on-screen message instead, replace this with a small “game over” overlay.
    printf("\nGame Over! Score = %d\n", score);
    DestroyTexture(&bee);
    RendererShutdown();
    CloseWindowSB();
    exit(0);
}

/* --------------- Main ------------------- */

int main(void) {
    srand((unsigned)time(NULL));

    InitWindow(WindowWidth, WindowHeight, "Bounce (Flappy Bee)");
    CreateGLContext();
    GL_SetSwapInterval(1);
    RendererInit();

    bee = LoadTexture("bee.png");

    Bouncer beeb = (Bouncer){ 200, 300, 100, 100, 0, 0, 0, (Color){1,0,0,1} };

    // Pipe timing + speed
    int framesSinceSpawn = 0;
    const int SPAWN_FRAMES = 90;  // spawn cadence (lower = more frequent)
    const int PIPE_SPEED   = 6;   // px/frame (increase to make harder)

    int score = 0;          // increment when you pass a pipe
    bool* counted = (bool*)malloc(sizeof(bool)*MAX_PIPES); // track scoring per pipe
    for (int i=0;i<MAX_PIPES;++i) counted[i]=false;

    ResetPipes();

    while (!WindowShouldClose()) {
        InputBeginFrame();
        PollEvents();

        if (IsKeyPressed(KEY_ESCAPE)) EndGame(score);
        if (IsKeyPressed(KEY_SPACE))  beeb.dy = -10.5f;

        Begin2D();
        ClearBackground();

        GetFramebufferSize(&fbw, &fbh);

        // Physics for bee
        Physics(&beeb);

        // Keep inside vertical bounds; if out, end
        if (beeb.y < 0) beeb.y = 0, beeb.dy = 0, EndGame(score);
        if (beeb.y + beeb.height > fbh){
            beeb.y = fbh - beeb.height;
            beeb.dy = 0;
            EndGame(score);
        }

        // Spawn pipes
        framesSinceSpawn++;
        if (framesSinceSpawn >= SPAWN_FRAMES){
            SpawnPipe(fbw, fbh);
            framesSinceSpawn = 0;
        }

        // Update & draw pipes + collisions + scoring
        UpdatePipes(PIPE_SPEED, fbw);

        for (int i = 0; i < MAX_PIPES; ++i){
            if (!pipes[i].active) { counted[i] = false; continue; }

            DrawPipe(&pipes[i]);

            // Collision against top pipe
            if (AABB(beeb.x, beeb.y, beeb.width, beeb.height,
                     pipes[i].x, 0, pipes[i].width, pipes[i].gapY)){
                EndGame(score);
            }
            // Collision against bottom pipe
            int bottomY = pipes[i].gapY + pipes[i].gapH;
            if (AABB(beeb.x, beeb.y, beeb.width, beeb.height,
                     pipes[i].x, bottomY, pipes[i].width, fbh - bottomY)){
                EndGame(score);
            }

            // Scoring: count when bee passes the pipe’s right edge
            if (!counted[i] && (pipes[i].x + pipes[i].width) < beeb.x){
                counted[i] = true;
                score++;
                // Optional: small upward nudge on point
                // beeb.dy -= 0.3f;
            }
        }

        // Draw bee last so it’s on top
        DrawBouncer(&beeb);

        End2D();

        // Optional console HUD
        // (You also have PrintFrameRate(); leaving it as-is.)
        // printf("\rScore: %d ", score); fflush(stdout);

        PrintFrameRate();
    }

    DestroyTexture(&bee);
    RendererShutdown();
    CloseWindowSB();
    free(counted);
    return 0;
}
