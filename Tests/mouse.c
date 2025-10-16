#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/sunburst.h"

typedef struct Container{

    size_t element_size;       // size of one element
    size_t index;              // number of elements in use
    size_t capacity;           // total allocated elements
    void* data;                // pointer to the allocated array


} Container;

static inline void container_init(Container* c, size_t element_size, size_t capacity) {
    assert(c && element_size > 0);
    c->element_size = element_size;
    c->index = 0;
    c->capacity = capacity;
    c->data = malloc(c->element_size * c->capacity);
}

static inline void container_push(Container* c, const void* element) {
    if (c->index >= c->capacity) {
        c->capacity *= 2;
        c->data = realloc(c->data, c->element_size * c->capacity);
    }
    memcpy((char*)c->data + c->index * c->element_size, element, c->element_size);
    c->index++;
}

static inline void*  container_data(Container* c){ return c->data; }
static inline const void* container_cdata(const Container* c){ return c->data; } 

static inline int container_get_copy(const Container* c, size_t index, void* out) {
    assert(c && out);
    if (index >= c->index) return 0;
    memcpy(out,
           (const char*)c->data + index * c->element_size,
           c->element_size);
    return 1;
}

static inline void container_replace(Container* c, size_t index, const void* element) {
    assert(c != NULL && "Container pointer cannot be NULL");
    assert(c->data != NULL && "Container data not initialized");
    assert(element != NULL && "Element pointer cannot be NULL");
    assert(index < c->index && "Index out of range: cannot replace beyond current size");

    void* dest = (char*)c->data + index * c->element_size;
    memcpy(dest, element, c->element_size);
}

static inline int container_pop_copy(Container* c, void* out) {
    assert(c && out);
    if (c->index == 0) return 0;
    size_t top = c->index - 1;
    memcpy(out, (char*)c->data + top * c->element_size, c->element_size);
    c->index = top;
    return 1;
}

static inline size_t container_size(const Container* c){ return c->index; }
static inline size_t container_capacity(const Container* c){ return c->capacity; }

static inline void container_free(Container* c) {
    free(c->data);
    c->data = NULL;
    c->index = c->capacity = 0;
}

// --- add this near your other types ---
typedef struct {
    Rect  rect;
    Color color;
} Stroke;

// simple 1px outline helper using 4 skinny rects
static inline void DrawRectOutline(const Rect* r, float px, Color c) {
    Rect top    = { r->x,           r->y,            r->width, px };
    Rect bottom = { r->x,           r->y+r->height-px, r->width, px };
    Rect left   = { r->x,           r->y,            px,       r->height };
    Rect right  = { r->x+r->width-px, r->y,          px,       r->height };
    DrawRectangleRect(&top, c);
    DrawRectangleRect(&bottom, c);
    DrawRectangleRect(&left, c);
    DrawRectangleRect(&right, c);
}

const int ww = 800;
const int wh = 600;
int fbw, fbh;

int main(void) {
    InitWindow(ww, wh, "Sunburst Paint");
    CreateGLContext();
    GL_SetSwapInterval(1);
    RendererInit();

    // --- UI constants ---
    const int PALETTE_W = 72;          // width of the left palette strip
    const int SWATCH_H  = 36;          // height of each color swatch
    const int GRID      = 16;          // snap grid size (looks crisp)
    const float OUTLINE = 1.0f;

    // palette (extend as you like)
    const Color PALETTE[] = {
        (Color){1,0,0,1},   (Color){0,1,0,1},   (Color){0,0,1,1},
        (Color){1,1,0,1},   (Color){1,0,1,1},   (Color){0,1,1,1},
        (Color){0.2f,0.2f,0.2f,1}, (Color){0.5f,0.5f,0.5f,1}, (Color){1,1,1,1},
        (Color){0.93f,0.49f,0.19f,1}, (Color){0.55f,0.27f,0.07f,1}
    };
    const int PALETTE_COUNT = (int)(sizeof(PALETTE)/sizeof(PALETTE[0]));
    int current_color_idx = 2; // start blue-ish

    // brush state
    int brush_w = 20;
    int brush_h = 20;

    // strokes buffer
    Container strokes;
    container_init(&strokes, sizeof(Stroke), 1024);

    // work rects
    Rect cursor = (Rect){0,0,(float)brush_w,(float)brush_h};

    while (!WindowShouldClose()) {
        InputBeginFrame();
        PollEvents();
        if (IsKeyPressed(KEY_ESCAPE)) break;

        GetFramebufferSize(&fbw, &fbh);

        // brush sizing
        if (IsKeyPressed(KEY_1))  { brush_w = brush_w>2? brush_w-2 : 2; brush_h = brush_h>2? brush_h-2 : 2; }
        if (IsKeyPressed(KEY_2)) { brush_w += 2; brush_h += 2; }
        if (IsKeyPressed(KEY_C)) {            // clear canvas
            strokes.index = 0;                // fast clear (keeps capacity)
        }

        // mouse & cursor
        int mx, my;
        GetMousePosition(&mx, &my);

        // snap to grid for crisp edges (optional – comment out if you want freeform)
        int sx = mx;
        int sy = my;

        cursor.x = (float)sx;
        cursor.y = (float)sy;
        cursor.width  = (float)brush_w;
        cursor.height = (float)brush_h;

        // --- Palette click detection ---
        // Only handle if mouse is in the palette column
        if (IsMouseDown(MOUSE_LEFT) && mx < PALETTE_W) {
            int idx = my / SWATCH_H;
            if (idx >= 0 && idx < PALETTE_COUNT) {
                current_color_idx = idx;
            }
        }

        // --- Painting (left click in canvas area) ---
        if (IsMouseDown(MOUSE_LEFT) && mx >= PALETTE_W) {
            Stroke s;
            s.rect  = cursor;
            s.color = PALETTE[current_color_idx];
            container_push(&strokes, &s);
        }

        // --- Erase (right click) — removes most recent overlapping stroke ---
        if (IsMouseDown(MOUSE_RIGHT) && strokes.index > 0) {
            // simple "eraser": pop last stroke that intersects the cursor
            for (size_t i = (size_t)strokes.index - 1; i >= 0; --i) {
                Stroke tmp;
                if (container_get_copy(&strokes, (size_t)i, &tmp)) {
                    bool overlap =
                        !(tmp.rect.x + tmp.rect.width  <= cursor.x ||
                          cursor.x   + cursor.width     <= tmp.rect.x ||
                          tmp.rect.y + tmp.rect.height <= cursor.y ||
                          cursor.y   + cursor.height    <= tmp.rect.y);
                    if (overlap) {
                        // move the last stroke into i, then shrink size (swap-remove)
                        Stroke last;
                        container_get_copy(&strokes, strokes.index - 1, &last);
                        // replace i with last
                        container_replace(&strokes, (size_t)i, &last);
                        strokes.index--; // popped
                        break;
                    }
                }
            }
        }

        // ------------ DRAW ------------
        Begin2D();
        // canvas background
        ClearBackground();

        // palette panel
        DrawRectangleRect(&(Rect){0,0, (float)PALETTE_W, (float)fbh}, (Color){0.12f,0.12f,0.12f,1});
        for (int i = 0; i < PALETTE_COUNT; ++i) {
            Rect sw = { 6, (float)(i*SWATCH_H + 6), (float)PALETTE_W - 12.0f, (float)SWATCH_H - 12.0f };
            DrawRectangleRect(&sw, PALETTE[i]);
            // selection outline
            if (i == current_color_idx) {
                DrawRectOutline(&sw, OUTLINE, (Color){1,1,1,1});
                Rect inner = { sw.x+2, sw.y+2, sw.width-4, sw.height-4 };
                DrawRectOutline(&inner, OUTLINE, (Color){0,0,0,1});
            }
        }

        // strokes
        for (size_t i = 0; i < strokes.index; ++i) {
            Stroke s;
            container_get_copy(&strokes, i, &s);
            DrawRectangleRect(&s.rect, s.color);
        }

        // cursor brush preview: semi-transparent fill + high-contrast outline
        Color cc = PALETTE[current_color_idx];
        Color fill = { cc.r, cc.g, cc.b, 0.35f };
        DrawRectangleRect(&cursor, fill);
        DrawRectOutline(&cursor, OUTLINE, (Color){1,1,1,1});
        DrawRectOutline(&(Rect){cursor.x+1, cursor.y+1, cursor.width-2, cursor.height-2}, OUTLINE, (Color){0,0,0,1});

        End2D();

        PrintFrameRate();
    }

    container_free(&strokes);
    RendererShutdown();
    CloseWindowSB();
    return 0;
}
