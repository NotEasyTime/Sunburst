#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "sunburst.h" 

// ---------- Internal state ----------
static uint8_t  gKeyNow[KEY_COUNT], gKeyPrev[KEY_COUNT];
static uint8_t  gMouseNow[MOUSE_BUTTON_COUNT], gMousePrev[MOUSE_BUTTON_COUNT];
static int      gMouseX, gMouseY;
static int      gWheelDelta;           // accumulates per-frame
static unsigned gTextQueue[8];         // tiny ring buffer for text input
static int      gTextHead, gTextTail;

// ----- Public glue -----
void InputInit(void) {
    memset(gKeyNow, 0, sizeof gKeyNow);
    memset(gKeyPrev, 0, sizeof gKeyPrev);
    memset(gMouseNow, 0, sizeof gMouseNow);
    memset(gMousePrev, 0, sizeof gMousePrev);
    gMouseX = gMouseY = 0;
    gWheelDelta = 0;
    gTextHead = gTextTail = 0;
}

void InputShutdown(void) { /* nothing yet */ }

void InputBeginFrame(void) {
    memcpy(gKeyPrev,   gKeyNow,   sizeof gKeyNow);
    memcpy(gMousePrev, gMouseNow, sizeof gMouseNow);
    gWheelDelta = 0;
    gTextHead = gTextTail = 0;   // clear text queue each frame
}

bool IsKeyDown(Key k)         { return (k >= 0 && k < KEY_COUNT) ? gKeyNow[k]  : false; }
bool IsKeyPressed(Key k)      { return (k >= 0 && k < KEY_COUNT) ? (gKeyNow[k] && !gKeyPrev[k]) : false; }
bool IsKeyReleased(Key k)     { return (k >= 0 && k < KEY_COUNT) ? (!gKeyNow[k] && gKeyPrev[k]) : false; }

bool IsMouseDown(MouseButton b)    { return (b >= 0 && b < MOUSE_BUTTON_COUNT) ? gMouseNow[b] : false; }
bool IsMousePressed(MouseButton b) { return (b >= 0 && b < MOUSE_BUTTON_COUNT) ? (gMouseNow[b] && !gMousePrev[b]) : false; }
bool IsMouseReleased(MouseButton b){ return (b >= 0 && b < MOUSE_BUTTON_COUNT) ? (!gMouseNow[b] && gMousePrev[b]) : false; }

void GetMousePosition(int* x, int* y) { if (x) *x = gMouseX; if (y) *y = gMouseY; }
int  GetMouseWheelDelta(void)         { return gWheelDelta; }

bool GetCharPressed(unsigned* out) {
    if (!out) return false;
    if (gTextHead == gTextTail) return false;
    *out = gTextQueue[gTextTail & 7];
    gTextTail++;
    return true;
}

// Small helper to push UTF-32 codepoints (platform backends call this)
static void push_text(unsigned cp) {
    gTextQueue[gTextHead & 7] = cp;
    gTextHead++;
}

// ---------- Platform backends hook into these ----------
// These are called from your PollEvents() internal platform code.
static void set_key(Key k, int down)       { if (k > KEY_UNKNOWN && k < KEY_COUNT) gKeyNow[k] = (uint8_t)(down != 0); }
static void set_mouse(MouseButton b, int down) { if (b >= 0 && b < MOUSE_BUTTON_COUNT) gMouseNow[b] = (uint8_t)(down != 0); }
static void set_mouse_pos(int x, int y)    { gMouseX = x; gMouseY = y; }
static void add_wheel(int delta)           { gWheelDelta += delta; }

void SB_InputSetKey(int k, int down)            { set_key((Key)k, down); }
void SB_InputSetMouse(int b, int down)          { set_mouse((MouseButton)b, down); }
void SB_InputSetMousePos(int x, int y)          { set_mouse_pos(x, y); }
void SB_InputAddWheel(int delta)                { add_wheel(delta); }
void SB_InputPushUTF32(unsigned cp)             { push_text(cp); }

// ---------- Key mapping utilities per platform ----------
static Key map_alpha_numeric_upper(int ch) {
    // ch should be ASCII uppercase 'A'..'Z' or '0'..'9'
    if (ch >= 'A' && ch <= 'Z') return (Key)(KEY_A + (ch - 'A'));
    if (ch >= '0' && ch <= '9') return (Key)(KEY_0 + (ch - '0'));
    return KEY_UNKNOWN;
}

static Key map_common_special(int code) {
    // Use platform-specific callers to translate to these cases
    switch (code) {
        case 1:  return KEY_ESCAPE;
        case 2:  return KEY_ENTER;
        case 3:  return KEY_BACKSPACE;
        case 4:  return KEY_TAB;
        case 5:  return KEY_SPACE;
        case 6:  return KEY_LEFT;
        case 7:  return KEY_RIGHT;
        case 8:  return KEY_UP;
        case 9:  return KEY_DOWN;
        case 10: return KEY_SHIFT;
        case 11: return KEY_CTRL;
        case 12: return KEY_ALT;
        case 13: return KEY_SUPER;
        case 14: return KEY_HOME;
        case 15: return KEY_END;
        case 16: return KEY_PAGEUP;
        case 17: return KEY_PAGEDOWN;
        case 18: return KEY_INSERT;
        case 19: return KEY_DELETE;
        default: return KEY_UNKNOWN;
    }
}
