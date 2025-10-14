#include "sunburst.h"
#include <stdlib.h>
#include <string.h> // memset
#include <stdio.h>

#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION 1
  #include <OpenGL/gl3.h>   // Core profile
#elif defined _WIN32
  #include <windows.h>  // must precede gl.h so WINGDIAPI/APIENTRY are defined
  #include <GL/gl.h>
#else
  #include <GL/gl.h>        // Or your loader on other platforms
#endif



#if defined(_WIN32)
  #include <windows.h>
  #include <GL/gl.h>
  #include "sb_gl_loader.h"

  // Map calls to loaded pointers
  #define glCreateProgram           pglCreateProgram
  #define glCreateShader            pglCreateShader
  #define glShaderSource            pglShaderSource
  #define glCompileShader           pglCompileShader
  #define glGetShaderiv             pglGetShaderiv
  #define glGetShaderInfoLog        pglGetShaderInfoLog
  #define glAttachShader            pglAttachShader
  #define glLinkProgram             pglLinkProgram
  #define glGetProgramiv            pglGetProgramiv
  #define glGetProgramInfoLog       pglGetProgramInfoLog
  #define glUseProgram              pglUseProgram
  #define glDeleteShader            pglDeleteShader
  #define glGenVertexArrays         pglGenVertexArrays
  #define glBindVertexArray         pglBindVertexArray
  #define glGenBuffers              pglGenBuffers
  #define glBindBuffer              pglBindBuffer
  #define glBufferData              pglBufferData
  #define glEnableVertexAttribArray pglEnableVertexAttribArray
  #define glVertexAttribPointer     pglVertexAttribPointer
  #define glGetIntegerv             pglGetIntegerv
  #define glDrawArrays              pglDrawArrays
  #define glBindAttribLocation       pglBindAttribLocation

#elif defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION 1
  #include <OpenGL/gl3.h>
#else
  #include <GL/gl.h>
#endif



static GLuint g_prog = 0;
static GLuint g_vao  = 0;
static GLuint g_vbo  = 0;

static const char* kVS =
"#version 150 core\n"
"in vec2 pos;\n"
"in vec4 inColor;\n"
"out vec4 vColor;\n"
"void main(){\n"
"  vColor = inColor;\n"
"  gl_Position = vec4(pos, 0.0, 1.0);\n"
"}\n";

static const char* kFS =
"#version 150 core\n"
"in vec4 vColor;\n"
"out vec4 outColor;\n"
"void main(){ outColor = vColor; }\n";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; GLsizei n=0;
        glGetShaderInfoLog(s, sizeof log, &n, log);
        fprintf(stderr, "GL shader error: %.*s\n", (int)n, log);
    }
    return s;
}

// --- Batching (CPU staging) -------------------------------------------------
// We’ll push 6 vertices per rectangle (two triangles).
// Each vertex: 2 (pos) + 4 (color) = 6 floats.
#define VERT_STRIDE_FLOATS 6
#define VERTS_PER_RECT     6
#define FLOATS_PER_RECT    (VERT_STRIDE_FLOATS*VERTS_PER_RECT)

// Reasonable default batch size: e.g., 8192 vertices (≈1365 rects)
static size_t   g_batch_capacity_vertices = 0;
static size_t   g_batch_count_vertices    = 0;
static float*   g_batch_data              = NULL;

// Cached framebuffer size for pixel->NDC each frame
static int g_fbw = 0, g_fbh = 0;

// --- Public-ish API ---------------------------------------------------------
void RendererInit(void) {
    // Program
    GLuint vs = compile(GL_VERTEX_SHADER,   kVS);
    GLuint fs = compile(GL_FRAGMENT_SHADER, kFS);
    g_prog = glCreateProgram();
    glAttachShader(g_prog, vs);
    glAttachShader(g_prog, fs);
    glBindAttribLocation(g_prog, 0, "pos");
    glBindAttribLocation(g_prog, 1, "inColor");
    glLinkProgram(g_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // VAO/VBO
    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);

    // Vertex layout: [x,y,r,g,b,a]
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          sizeof(float)*VERT_STRIDE_FLOATS, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                          sizeof(float)*VERT_STRIDE_FLOATS, (void*)(sizeof(float)*2));

    // CPU batch buffer
    g_batch_capacity_vertices = 8192; // tweak as you like
    g_batch_data = (float*)malloc(sizeof(float) * g_batch_capacity_vertices * VERT_STRIDE_FLOATS);
    g_batch_count_vertices = 0;

    // Leave our stuff bound; we own the state.
}

void RendererShutdown(void) {
    free(g_batch_data); g_batch_data = NULL;
    g_batch_capacity_vertices = 0; g_batch_count_vertices = 0;

    if (g_vbo)  glDeleteBuffers(1, &g_vbo),  g_vbo  = 0;
    if (g_vao)  glDeleteVertexArrays(1, &g_vao), g_vao = 0;
    if (g_prog) glDeleteProgram(g_prog), g_prog = 0;
}

// Call once per frame before drawing rectangles
void Begin2D(void) {
    glUseProgram(g_prog);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);

    // Cache framebuffer for pixel->NDC conversion
    GetFramebufferSize(&g_fbw, &g_fbh);

    // Reset batch
    g_batch_count_vertices = 0;
}

// Upload & draw whatever is queued
static void FlushBatch(void) {
    if (g_batch_count_vertices == 0) return;
    const GLsizeiptr byteCount = (GLsizeiptr)(g_batch_count_vertices * VERT_STRIDE_FLOATS * sizeof(float));

    // Orphan, then upload
    glBufferData(GL_ARRAY_BUFFER, byteCount, NULL, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, byteCount, g_batch_data);

    glDrawArrays(GL_TRIANGLES, 0, (GLint)g_batch_count_vertices);

    g_batch_count_vertices = 0;
}

void End2D(void) {
    
    FlushBatch();
    GL_SwapBuffers();
    // You may leave program/vao/vbo bound; you own the state.
}

// --- Helpers ----------------------------------------------------------------
static inline void push_rect_pixels_to_batch(
    int x, int y, int w, int h, float r, float g, float b, float a)
{
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    if (g_fbw <= 0 || g_fbh <= 0 || w == 0 || h == 0) return;

    // Pixel -> NDC (top-left origin)
    const float L =  2.0f * ((float)x        / (float)g_fbw) - 1.0f;
    const float R =  2.0f * ((float)(x + w)  / (float)g_fbw) - 1.0f;
    const float T =  1.0f - 2.0f * ((float)y        / (float)g_fbh);
    const float B =  1.0f - 2.0f * ((float)(y + h)  / (float)g_fbh);

    // Ensure capacity (6 vertices per rect)
    const size_t need = g_batch_count_vertices + VERTS_PER_RECT;
    if (need > g_batch_capacity_vertices) {
        // Flush current, then grow if still not enough
        FlushBatch();
        if (VERTS_PER_RECT > g_batch_capacity_vertices) {
            size_t newCap = g_batch_capacity_vertices ? g_batch_capacity_vertices : 8192;
            while (newCap < VERTS_PER_RECT) newCap *= 2;
            g_batch_data = (float*)realloc(g_batch_data,
                                sizeof(float) * newCap * VERT_STRIDE_FLOATS);
            g_batch_capacity_vertices = newCap;
        }
    }

    float* v = g_batch_data + g_batch_count_vertices * VERT_STRIDE_FLOATS;

    // Two triangles: (L,T)-(L,B)-(R,T) and (R,T)-(L,B)-(R,B)
    // V0
    v[0]=L; v[1]=T; v[2]=r; v[3]=g; v[4]=b; v[5]=a;
    // V1
    v[6]=L; v[7]=B; v[8]=r; v[9]=g; v[10]=b; v[11]=a;
    // V2
    v[12]=R; v[13]=T; v[14]=r; v[15]=g; v[16]=b; v[17]=a;
    // V3
    v[18]=R; v[19]=T; v[20]=r; v[21]=g; v[22]=b; v[23]=a;
    // V4
    v[24]=L; v[25]=B; v[26]=r; v[27]=g; v[28]=b; v[29]=a;
    // V5
    v[30]=R; v[31]=B; v[32]=r; v[33]=g; v[34]=b; v[35]=a;

    g_batch_count_vertices += VERTS_PER_RECT;
}

// --- Your public draw -------------------------------------------------------
void DrawRectangle(int x, int y, int width, int height, Color color) {
    // Just append to the batch; no GL calls here.
    push_rect_pixels_to_batch(x, y, width, height,
                              color.r, color.g, color.b, color.a);
}

// Optional helper to force a flush mid-frame (e.g., before switching shaders)
void Flush2D(void) {
    FlushBatch();
}