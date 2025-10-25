#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "sunburst.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// Attribute locations
#define ATTR_POS   0
#define ATTR_COLOR 1
#define ATTR_UV    1 // reuse location 1 for uv in the textured pipeline

// Shaders
static const char* s_rectVS =
"#version 330 core\n"
"in vec2 pos;\n"
"in vec4 inColor;\n"
"out vec4 vColor;\n"
"void main(){ vColor = inColor; gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char* s_rectFS =
"#version 330 core\n"
"in vec4 vColor;\n"
"out vec4 outColor;\n"
"void main(){ outColor = vColor; }\n";


// Common utilities
static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; GLsizei n = 0;
        glGetShaderInfoLog(s, (GLsizei)sizeof log, &n, log);
        fprintf(stderr, "GL shader compile error:\n%.*s\n", (int)n, log);
    }
    return s;
}

typedef void (*BindAttribsFn)(GLuint prog);

static GLuint link_program(GLuint vs, GLuint fs, BindAttribsFn bind_attribs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    if (bind_attribs) bind_attribs(p);
    glLinkProgram(p);
    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]; GLsizei n = 0;
        glGetProgramInfoLog(p, (GLsizei)sizeof log, &n, log);
        fprintf(stderr, "GL program link error:\n%.*s\n", (int)n, log);
    }
    return p;
}

static void build_quad_indices(GLuint* dst, size_t quadCount) {
    for (size_t i = 0; i < quadCount; ++i) {
        const GLuint base = (GLuint)(i * 4);
        const size_t o = i * 6;
        dst[o + 0] = base + 0;
        dst[o + 1] = base + 1;
        dst[o + 2] = base + 2;
        dst[o + 3] = base + 2;
        dst[o + 4] = base + 1;
        dst[o + 5] = base + 3;
    }
}

// Framebuffer cache
static int s_fbW = 0, s_fbH = 0;

// Rect batch (indexed 4-vertex quads)
// Vertex layout: [x, y, r, g, b, a]
#define RECT_VTX_STRIDE_FLOATS 6

typedef struct RectBatch {
    GLuint vbo;
    GLuint ebo;
    GLuint vao;
    GLuint prog;

    size_t capQuads;     // capacity in quads
    size_t countQuads;   // queued quads
    float* vtxData;      // CPU staging: 4 verts/quad
} RectBatch;

static RectBatch s_rectBatch = {0};

static void bind_rect_attribs(GLuint prog) {
    glBindAttribLocation(prog, ATTR_POS,   "pos");
    glBindAttribLocation(prog, ATTR_COLOR, "inColor");
}

static void rectbatch_init(size_t capQuads) {
    s_rectBatch.capQuads   = capQuads ? capQuads : 2048;
    s_rectBatch.countQuads = 0;
    s_rectBatch.vtxData = (float*)malloc(
        s_rectBatch.capQuads * 4 * RECT_VTX_STRIDE_FLOATS * sizeof(float));

    // GL objects
    glGenVertexArrays(1, &s_rectBatch.vao);
    glBindVertexArray(s_rectBatch.vao);

    glGenBuffers(1, &s_rectBatch.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_rectBatch.vbo);

    glGenBuffers(1, &s_rectBatch.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_rectBatch.ebo);

    // Static EBO up to capacity
    const size_t indexCount = s_rectBatch.capQuads * 6;
    GLuint* indices = (GLuint*)malloc(indexCount * sizeof(GLuint));
    build_quad_indices(indices, s_rectBatch.capQuads);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(indexCount * sizeof(GLuint)),
                 indices, GL_STATIC_DRAW);
    free(indices);

    // Program + vertex layout
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   s_rectVS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, s_rectFS);
    s_rectBatch.prog = link_program(vs, fs, bind_rect_attribs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    const GLsizei stride = (GLsizei)(sizeof(float) * RECT_VTX_STRIDE_FLOATS);
    glEnableVertexAttribArray(ATTR_POS);
    glVertexAttribPointer(ATTR_POS, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(ATTR_COLOR);
    glVertexAttribPointer(ATTR_COLOR, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*2));
}

static void rectbatch_shutdown(void) {
    free(s_rectBatch.vtxData); s_rectBatch.vtxData = NULL;
    s_rectBatch.capQuads = s_rectBatch.countQuads = 0;

    if (s_rectBatch.vbo)  { glDeleteBuffers(1, &s_rectBatch.vbo);  s_rectBatch.vbo = 0; }
    if (s_rectBatch.ebo)  { glDeleteBuffers(1, &s_rectBatch.ebo);  s_rectBatch.ebo = 0; }
    if (s_rectBatch.vao)  { glDeleteVertexArrays(1, &s_rectBatch.vao); s_rectBatch.vao = 0; }
    #if defined(_MSC_VER) 
    if (s_rectBatch.prog) { glDeleteProgram(s_rectBatch.prog); s_rectBatch.prog = 0; }
    #elif defined(__APPLE__)
    if (s_rectBatch.prog) { glDeleteProgram(s_rectBatch.prog); s_rectBatch.prog = 0; }
    #endif

}

static void rectbatch_maybe_grow(size_t requiredQuads) {
    if (requiredQuads <= s_rectBatch.capQuads) return;

    size_t newCap = s_rectBatch.capQuads;
    while (newCap < requiredQuads) newCap <<= 1;

    float* newV = (float*)realloc(
        s_rectBatch.vtxData, newCap * 4 * RECT_VTX_STRIDE_FLOATS * sizeof(float));
    if (!newV) {
        fprintf(stderr, "Out of memory growing rect batch.\n");
        return;
    }
    s_rectBatch.vtxData = newV;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_rectBatch.ebo);
    const size_t indexCount = newCap * 6;
    GLuint* indices = (GLuint*)malloc(indexCount * sizeof(GLuint));
    build_quad_indices(indices, newCap);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(indexCount * sizeof(GLuint)),
                 indices, GL_STATIC_DRAW);
    free(indices);

    s_rectBatch.capQuads = newCap;
}

static void rectbatch_flush(void) {
    if (s_rectBatch.countQuads == 0) return;

    const size_t vCount = s_rectBatch.countQuads * 4;
    const size_t vBytes = vCount * RECT_VTX_STRIDE_FLOATS * sizeof(float);
    const size_t iCount = s_rectBatch.countQuads * 6;

    glUseProgram(s_rectBatch.prog);
    glBindVertexArray(s_rectBatch.vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_rectBatch.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_rectBatch.ebo);

    // Stream vertices for the quads enqueued
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vBytes, NULL, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)vBytes, s_rectBatch.vtxData);

    glDrawElements(GL_TRIANGLES, (GLsizei)iCount, GL_UNSIGNED_INT, (void*)0);

    s_rectBatch.countQuads = 0;
}

static inline void rectbatch_push(int x, int y, int w, int h,
                                  float r, float g, float b, float a) {
    if (w == 0 || h == 0 || s_fbW <= 0 || s_fbH <= 0) return;
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }

    const size_t need = s_rectBatch.countQuads + 1;
    if (need > s_rectBatch.capQuads) {
        rectbatch_maybe_grow(need);
        if (need > s_rectBatch.capQuads) return; // OOM guard
    }

    // Pixel -> NDC (origin: top-left)
    const float L =  2.0f * ((float)x        / (float)s_fbW) - 1.0f;
    const float R =  2.0f * ((float)(x + w)  / (float)s_fbW) - 1.0f;
    const float T =  1.0f - 2.0f * ((float)y        / (float)s_fbH);
    const float B =  1.0f - 2.0f * ((float)(y + h)  / (float)s_fbH);

    float* v = s_rectBatch.vtxData + (s_rectBatch.countQuads * 4 * RECT_VTX_STRIDE_FLOATS);

    // V0 (L,T)
    v[0]=L; v[1]=T; v[2]=r; v[3]=g; v[4]=b; v[5]=a;
    // V1 (L,B)
    v[6]=L; v[7]=B; v[8]=r; v[9]=g; v[10]=b; v[11]=a;
    // V2 (R,T)
    v[12]=R; v[13]=T; v[14]=r; v[15]=g; v[16]=b; v[17]=a;
    // V3 (R,B)
    v[18]=R; v[19]=B; v[20]=r; v[21]=g; v[22]=b; v[23]=a;

    s_rectBatch.countQuads += 1;
}

// Public API
void RendererInit(void) {
    rectbatch_init(2048);
    texbatch_init(2048);
    s_fbW = s_fbH = 0;

    
}

void RendererShutdown(void) {
    texbatch_shutdown();
    rectbatch_shutdown();
}

void Begin2D(int fbWidth, int fbHeight) {
    s_fbW = fbWidth;
    s_fbH = fbHeight;
    glViewport(0, 0, fbWidth, fbHeight);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


void End2D(void) {
    // If you tend to draw rects then sprites, this order preserves that pattern
    rectbatch_flush();
    texbatch_flush();
}

void Flush2D(void) {
    rectbatch_flush();
    texbatch_flush();
}

void DrawRectangle(int x, int y, int w, int h, Color c) {
    rectbatch_push(x, y, w, h, c.r, c.g, c.b, c.a);
}