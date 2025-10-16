// rect_rush_demo.c
// A small arcade game using the provided SB API (rectangles only).
// - Move: WASD
// - Shoot: Left mouse button (toward mouse)
// - Resize player: Mouse wheel (affects hitbox)
// - Press R to restart after death
// - Press ESC to quit

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "../src/sunburst.h"   // your header from the message



// ------------------------- Config -------------------------
#define MAX_ENEMIES     64
#define MAX_BULLETS     128
#define PLAYER_BASE_W   28
#define PLAYER_BASE_H   28
#define PLAYER_SPEED    6.0f       // pixels per frame
#define BULLET_SPEED    12.0f
#define BULLET_W        6
#define BULLET_H        6
#define ENEMY_MIN_SIZE  18
#define ENEMY_MAX_SIZE  48
#define ENEMY_MIN_SPD   1.5f
#define ENEMY_MAX_SPD   3.5f
#define WAVE_ENEMIES(n) (6 + (n) * 3)   // enemies per wave

// ------------------------- Types --------------------------
typedef struct {
    Rect rect;
    float vx, vy;
    int alive;
} Enemy;

typedef struct {
    Rect rect;
    float vx, vy;
    int alive;
} Bullet;

typedef enum { STATE_PLAYING, STATE_DEAD } GameState;

// ------------------------- Globals ------------------------
static Enemy  g_enemies[MAX_ENEMIES];
static Bullet g_bullets[MAX_BULLETS];
static Rect g_player;
static int g_wave = 1;
static int g_score = 0;
static int g_lives = 1;
static GameState g_state = STATE_PLAYING;

// Colors
static const Color COL_BG      = {0.07f, 0.07f, 0.10f, 1.0f};
static const Color COL_TEXT    = {0.95f, 0.95f, 0.98f, 1.0f};
static const Color COL_PLAYER  = {0.20f, 0.75f, 0.30f, 1.0f};
static const Color COL_ENEMY   = {0.85f, 0.20f, 0.20f, 1.0f};
static const Color COL_BULLET  = {0.95f, 0.80f, 0.10f, 1.0f};
static const Color COL_HIT     = {1.00f, 0.45f, 0.15f, 1.0f};

// ------------------------- Util ---------------------------
static float frand(float a, float b) {
    return a + (float)rand() / (float)RAND_MAX * (b - a);
}

static void clamp_rect_to_screen(Rect* r, int w, int h) {
    if (r->x < 0) r->x = 0;
    if (r->y < 0) r->y = 0;
    if (r->x + r->width > w)  r->x = (float)w  - r->width;
    if (r->y + r->height > h) r->y = (float)h  - r->height;
}

static void spawn_enemy_outside_player(Rect player, int fbw, int fbh) {
    // Find a free enemy slot
    int idx = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g_enemies[i].alive) { idx = i; break; }
    }
    if (idx < 0) return;

    Enemy e = {0};
    e.rect.width  = frand((float)ENEMY_MIN_SIZE, (float)ENEMY_MAX_SIZE);
    e.rect.height = frand((float)ENEMY_MIN_SIZE, (float)ENEMY_MAX_SIZE);

    // Spawn near edges
    int edge = rand() % 4;
    if (edge == 0) { // top
        e.rect.x = frand(0, (float)(fbw - (int)e.rect.width));
        e.rect.y = -e.rect.height - 4;
    } else if (edge == 1) { // bottom
        e.rect.x = frand(0, (float)(fbw - (int)e.rect.width));
        e.rect.y = fbh + 4;
    } else if (edge == 2) { // left
        e.rect.x = -e.rect.width - 4;
        e.rect.y = frand(0, (float)(fbh - (int)e.rect.height));
    } else { // right
        e.rect.x = fbw + 4;
        e.rect.y = frand(0, (float)(fbh - (int)e.rect.height));
    }

    // Velocity points roughly toward the player
    float cx = e.rect.x + e.rect.width * 0.5f;
    float cy = e.rect.y + e.rect.height * 0.5f;
    float px = player.x + player.width * 0.5f;
    float py = player.y + player.height * 0.5f;
    float dx = px - cx;
    float dy = py - cy;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.001f) { dx = 1; dy = 0; len = 1; }
    dx /= len; dy /= len;

    float spd = frand(ENEMY_MIN_SPD, ENEMY_MAX_SPD);
    e.vx = dx * spd;
    e.vy = dy * spd;

    e.alive = 1;
    g_enemies[idx] = e;
}

static void start_wave(int wave, int fbw, int fbh) {
    int needed = WAVE_ENEMIES(wave);
    int count = 0;
    for (int i = 0; i < MAX_ENEMIES && count < needed; i++) {
        if (!g_enemies[i].alive) {
            spawn_enemy_outside_player(g_player, fbw, fbh);
            count++;
        }
    }
}

static void reset_game(int fbw, int fbh) {
    g_wave = 1;
    g_score = 0;
    g_lives = 1;
    g_state = STATE_PLAYING;

    for (int i = 0; i < MAX_ENEMIES; i++) g_enemies[i].alive = 0;
    for (int i = 0; i < MAX_BULLETS; i++) g_bullets[i].alive = 0;

    g_player.width = PLAYER_BASE_W;
    g_player.height = PLAYER_BASE_H;
    g_player.x = fbw * 0.5f - g_player.width * 0.5f;
    g_player.y = fbh * 0.5f - g_player.height * 0.5f;

    start_wave(g_wave, fbw, fbh);
}

static void shoot_toward(int mx, int my) {
    // Find a free bullet
    int idx = -1;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g_bullets[i].alive) { idx = i; break; }
    }
    if (idx < 0) return;

    Bullet b = {0};
    float cx = g_player.x + g_player.width  * 0.5f;
    float cy = g_player.y + g_player.height * 0.5f;

    float dx = (float)mx - cx;
    float dy = (float)my - cy;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.001f) { dx = 1; dy = 0; len = 1; }
    dx /= len; dy /= len;

    b.rect.x = cx - BULLET_W * 0.5f;
    b.rect.y = cy - BULLET_H * 0.5f;
    b.rect.width  = BULLET_W;
    b.rect.height = BULLET_H;
    b.vx = dx * BULLET_SPEED;
    b.vy = dy * BULLET_SPEED;
    b.alive = 1;

    g_bullets[idx] = b;
}

// ------------------------- Main ---------------------------
int main(void) {
    srand((unsigned)time(NULL));

    if (!InitWindow(960, 540, "Rect Rush (SB API demo)")) return 1;
    if (!CreateGLContext()) return 1;
    RendererInit();
    InputInit();
    GL_SetSwapInterval(1);

    int fbw = 0, fbh = 0;
    GetFramebufferSize(&fbw, &fbh);
    reset_game(fbw, fbh);

    // Basic game loop
    while (!WindowShouldClose()) {
        // --- Per-frame input begin & event pump ---
        InputBeginFrame();
        PollEvents();

        // Resize awareness
        int nfbw = fbw, nfbh = fbh;
        GetFramebufferSize(&nfbw, &nfbh);
        if (nfbw != fbw || nfbh != fbh) {
            // Keep player inside if window changed
            fbw = nfbw; fbh = nfbh;
            clamp_rect_to_screen(&g_player, fbw, fbh);
        }

        // --- Gameplay input ---
        if (IsKeyDown(KEY_ESCAPE)) break;

        if (g_state == STATE_PLAYING) {
            // Move player with WASD
            float dx = 0, dy = 0;
            if (IsKeyDown(KEY_W)) dy -= 1;
            if (IsKeyDown(KEY_S)) dy += 1;
            if (IsKeyDown(KEY_A)) dx -= 1;
            if (IsKeyDown(KEY_D)) dx += 1;
            if (dx != 0 || dy != 0) {
                float len = sqrtf(dx*dx + dy*dy);
                dx /= len; dy /= len;
                g_player.x += dx * PLAYER_SPEED;
                g_player.y += dy * PLAYER_SPEED;
            }

            // Mouse wheel adjusts player size (and hitbox)
            int wheel = GetMouseWheelDelta();
            if (wheel != 0) {
                float before_cx = g_player.x + g_player.width  * 0.5f;
                float before_cy = g_player.y + g_player.height * 0.5f;
                float scale = 1.0f + 0.1f * (float)wheel;
                g_player.width  = fmaxf(12.0f, g_player.width  * scale);
                g_player.height = fmaxf(12.0f, g_player.height * scale);
                // keep centered
                g_player.x = before_cx - g_player.width  * 0.5f;
                g_player.y = before_cy - g_player.height * 0.5f;
            }

            // Shooting
            int mx = 0, my = 0;
            GetMousePosition(&mx, &my);
            if (IsMousePressed(MOUSE_LEFT)) {
                shoot_toward(mx, my);
            }

            // Keep player inside screen
            clamp_rect_to_screen(&g_player, fbw, fbh);

            // --- Update bullets ---
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (!g_bullets[i].alive) continue;
                g_bullets[i].rect.x += g_bullets[i].vx;
                g_bullets[i].rect.y += g_bullets[i].vy;

                // Despawn offscreen
                if (g_bullets[i].rect.x + g_bullets[i].rect.width  < -8 ||
                    g_bullets[i].rect.y + g_bullets[i].rect.height < -8 ||
                    g_bullets[i].rect.x > fbw + 8 ||
                    g_bullets[i].rect.y > fbh + 8) {
                    g_bullets[i].alive = 0;
                }
            }

            // --- Update enemies + handle collisions ---
            int alive_count = 0;
            for (int e = 0; e < MAX_ENEMIES; e++) {
                if (!g_enemies[e].alive) continue;

                // Move and bounce on walls
                g_enemies[e].rect.x += g_enemies[e].vx;
                g_enemies[e].rect.y += g_enemies[e].vy;

                if (g_enemies[e].rect.x < 0) { g_enemies[e].rect.x = 0; g_enemies[e].vx = fabsf(g_enemies[e].vx); }
                if (g_enemies[e].rect.y < 0) { g_enemies[e].rect.y = 0; g_enemies[e].vy = fabsf(g_enemies[e].vy); }
                if (g_enemies[e].rect.x + g_enemies[e].rect.width > fbw) {
                    g_enemies[e].rect.x = fbw - g_enemies[e].rect.width;
                    g_enemies[e].vx = -fabsf(g_enemies[e].vx);
                }
                if (g_enemies[e].rect.y + g_enemies[e].rect.height > fbh) {
                    g_enemies[e].rect.y = fbh - g_enemies[e].rect.height;
                    g_enemies[e].vy = -fabsf(g_enemies[e].vy);
                }

                // Player collision
                if (CheckRectsOverlap(&g_player, &g_enemies[e].rect)) {
                    g_state = STATE_DEAD;
                }

                // Bullet collisions
                for (int b = 0; b < MAX_BULLETS; b++) {
                    if (!g_bullets[b].alive) continue;
                    if (CheckRectsOverlap(&g_bullets[b].rect, &g_enemies[e].rect)) {
                        // For fun: shrink enemy on hit using overlap
                        Rect overlap;
                        GetRectOverlap(&g_bullets[b].rect, &g_enemies[e].rect, &overlap);

                        // Shrink enemy by overlap area heuristic; if too small, kill
                        float shrink = fminf(overlap.width, overlap.height);
                        g_enemies[e].rect.width  -= shrink * 0.8f;
                        g_enemies[e].rect.height -= shrink * 0.8f;
                        if (g_enemies[e].rect.width < 12 || g_enemies[e].rect.height < 12) {
                            g_enemies[e].alive = 0;
                            g_score += 10;
                        } else {
                            // Nudge enemy away from impact
                            if (overlap.width >= overlap.height) {
                                if (g_bullets[b].vy > 0) g_enemies[e].rect.y += overlap.height;
                                else                     g_enemies[e].rect.y -= overlap.height;
                            } else {
                                if (g_bullets[b].vx > 0) g_enemies[e].rect.x += overlap.width;
                                else                     g_enemies[e].rect.x -= overlap.width;
                            }
                        }

                        g_bullets[b].alive = 0;
                    }
                }

                if (g_enemies[e].alive) alive_count++;
            }

            // Wave cleared?
            if (alive_count == 0) {
                g_wave++;
                start_wave(g_wave, fbw, fbh);
            }
        } else {
            // Dead state: press R to restart
            if (IsKeyPressed(KEY_R)) {
                reset_game(fbw, fbh);
            }
        }

        // ----------------- Draw -----------------
        ClearBackground();
        Begin2D();

        // Player
        DrawRectangle((int)g_player.x, (int)g_player.y, (int)g_player.width, (int)g_player.height, COL_PLAYER);

        // Bullets
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (!g_bullets[i].alive) continue;
            DrawRectangle((int)g_bullets[i].rect.x, (int)g_bullets[i].rect.y,
                          (int)g_bullets[i].rect.width, (int)g_bullets[i].rect.height, COL_BULLET);
        }

        // Enemies
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!g_enemies[i].alive) continue;
            // Slight color shift if large
            float t = fminf(1.0f, (g_enemies[i].rect.width + g_enemies[i].rect.height) / (ENEMY_MAX_SIZE*2.0f));
            Color c = { COL_ENEMY.r, COL_ENEMY.g * (0.8f + 0.2f * t), COL_ENEMY.b * (0.8f + 0.2f * (1.0f - t)), 1.0f };
            DrawRectangle((int)g_enemies[i].rect.x, (int)g_enemies[i].rect.y,
                          (int)g_enemies[i].rect.width, (int)g_enemies[i].rect.height, c);
        }

        // HUD (simple rectangles as fake text bars)
        // Score bar
        int barH = 6;
        int scoreBarW = (int)fminf((float)fbw, (float)(g_score * 2 + 40));
        DrawRectangle(0, 0, scoreBarW, barH, COL_TEXT);

        // Wave indicator (little blocks at top-right)
        int bx = fbw - 12, by = 2;
        for (int i = 0; i < g_wave && i < 12; i++) {
            DrawRectangle(bx - i*10, by, 8, 4, (Color){0.6f, 0.8f, 1.0f, 1.0f});
        }

        if (g_state == STATE_DEAD) {
            // Big center panelw
            int w = 100;
            int h = 100;
            int x = fbw/2 - w/2, y = fbh/2 - h/2;
            DrawRectangle(x, y, w, h, (Color){0.08f, 0.08f, 0.12f, 0.95f});
            DrawRectangle(x+12, y+12, w-24, h-24, COL_HIT);
            // “Press R to restart” banner (as thin rectangles)
            DrawRectangle(x+24, y+24, w-48, 8, COL_TEXT);
            DrawRectangle(x+24, y+48, w-48, 8, COL_TEXT);
            DrawRectangle(x+24, y+72, w-48, 8, COL_TEXT);
        }

        End2D();

        // Optional: OS-level title updates
        char title[128];
        snprintf(title, sizeof(title), "Rect Rush — Score: %d  Wave: %d  [%s]",
                 g_score, g_wave, g_state == STATE_PLAYING ? "PLAY" : "DEAD");
        SetWindowTitle(title);

        PrintFrameRate();      // your diagnostic
    }

    // Shutdown
    InputShutdown();
    RendererShutdown();
    CloseWindowSB();
    return 0;
}
