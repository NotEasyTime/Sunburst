#include "sunburst.h"
#include "stb_image.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION 1
  #include <OpenGL/gl3.h>   // Core profile
#elif defined(_WIN32)
  #include <windows.h>      // must precede gl.h so WINGDIAPI/APIENTRY are defined
  #include <GL/gl.h>
  #include "sb_gl_loader.h" // your function loader providing pgl* pointers

  // Map modern GL calls to loaded pointers. Ensure your loader defines these.
  #define glCreateProgram            pglCreateProgram
  #define glCreateShader             pglCreateShader
  #define glShaderSource             pglShaderSource
  #define glCompileShader            pglCompileShader
  #define glGetShaderiv              pglGetShaderiv
  #define glGetShaderInfoLog         pglGetShaderInfoLog
  #define glAttachShader             pglAttachShader
  #define glLinkProgram              pglLinkProgram
  #define glGetProgramiv             pglGetProgramiv
  #define glGetProgramInfoLog        pglGetProgramInfoLog
  #define glUseProgram               pglUseProgram
  #define glDeleteShader             pglDeleteShader
  #define glBindAttribLocation       pglBindAttribLocation
  #define glGetUniformLocation       pglGetUniformLocation
  #define glUniform1i                pglUniform1i

  #define glGenVertexArrays          pglGenVertexArrays
  #define glBindVertexArray          pglBindVertexArray
  #define glGenBuffers               pglGenBuffers
  #define glDeleteVertexArrays       pglDeleteVertexArrays
  #define glDeleteBuffers            pglDeleteBuffers
  #define glBindBuffer               pglBindBuffer
  #define glBufferData               pglBufferData
  #define glBufferSubData            pglBufferSubData
  #define glEnableVertexAttribArray  pglEnableVertexAttribArray
  #define glVertexAttribPointer      pglVertexAttribPointer
  #define glDrawElements             pglDrawElements
  #define glActiveTexture            pglActiveTexture

#else
  // On Linux/others, include your GL loader *before* this file in the build,
  // or ensure <GL/gl.h> + loader provides core profile functions.
  #include <GL/gl.h>
#endif

// -----------------------------------------------------------------------------
// External platform hooks
extern void GetFramebufferSize(int* outW, int* outH);
extern void GL_SwapBuffers(void);

// -----------------------------------------------------------------------------
// Attribute locations
#define ATTR_POS   0
#define ATTR_COLOR 1
#define ATTR_UV    1 // reuse location 1 for uv in the textured pipeline

// -----------------------------------------------------------------------------
// Shaders
static const char* s_rectVS =
"#version 150 core\n"
"in vec2 pos;\n"
"in vec4 inColor;\n"
"out vec4 vColor;\n"
"void main(){ vColor = inColor; gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char* s_rectFS =
"#version 150 core\n"
"in vec4 vColor;\n"
"out vec4 outColor;\n"
"void main(){ outColor = vColor; }\n";

static const char* s_texVS =
"#version 150 core\n"
"in vec2 pos;\n"
"in vec2 uv;\n"
"out vec2 vUV;\n"
"void main(){ vUV = uv; gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char* s_texFS =
"#version 150 core\n"
"in vec2 vUV;\n"
"uniform sampler2D tex;\n"
"out vec4 outColor;\n"
"void main(){ outColor = texture(tex, vUV); }\n";

// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Framebuffer cache
static int s_fbW = 0, s_fbH = 0;

// -----------------------------------------------------------------------------
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
    if (s_rectBatch.prog) { pglDeleteProgram(s_rectBatch.prog); s_rectBatch.prog = 0; }
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

// -----------------------------------------------------------------------------
// Textured sprite batch (indexed 4-vertex quads)
// Vertex layout: [x, y, u, v]
#define TEX_VTX_STRIDE_FLOATS 4

typedef struct TexBatch {
    GLuint vbo;
    GLuint ebo;
    GLuint vao;
    GLuint prog;
    GLint  samplerLoc;

    GLuint currentTex;
    size_t capQuads;
    size_t countQuads;
    float* vtxData; // 4 verts/quad
} TexBatch;

static TexBatch s_texBatch = {0};

static void bind_tex_attribs(GLuint prog) {
    glBindAttribLocation(prog, ATTR_POS, "pos");
    glBindAttribLocation(prog, ATTR_UV,  "uv");
}

static void texbatch_init(size_t capQuads) {
    s_texBatch.capQuads   = capQuads ? capQuads : 2048;
    s_texBatch.countQuads = 0;
    s_texBatch.currentTex = 0;
    s_texBatch.vtxData = (float*)malloc(
        s_texBatch.capQuads * 4 * TEX_VTX_STRIDE_FLOATS * sizeof(float));

    glGenVertexArrays(1, &s_texBatch.vao);
    glBindVertexArray(s_texBatch.vao);

    glGenBuffers(1, &s_texBatch.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_texBatch.vbo);

    glGenBuffers(1, &s_texBatch.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_texBatch.ebo);

    // Static index buffer
    const size_t indexCount = s_texBatch.capQuads * 6;
    GLuint* indices = (GLuint*)malloc(indexCount * sizeof(GLuint));
    build_quad_indices(indices, s_texBatch.capQuads);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(indexCount * sizeof(GLuint)),
                 indices, GL_STATIC_DRAW);
    free(indices);

    // Program + layout
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   s_texVS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, s_texFS);
    s_texBatch.prog = link_program(vs, fs, bind_tex_attribs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    s_texBatch.samplerLoc = glGetUniformLocation(s_texBatch.prog, "tex");

    const GLsizei stride = (GLsizei)(sizeof(float) * TEX_VTX_STRIDE_FLOATS);
    glEnableVertexAttribArray(ATTR_POS);
    glVertexAttribPointer(ATTR_POS, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(ATTR_UV);
    glVertexAttribPointer(ATTR_UV,  2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*2));
}

static void texbatch_shutdown(void) {
    free(s_texBatch.vtxData); s_texBatch.vtxData = NULL;
    s_texBatch.capQuads = s_texBatch.countQuads = 0;
    s_texBatch.currentTex = 0;

    if (s_texBatch.vbo) { glDeleteBuffers(1, &s_texBatch.vbo); s_texBatch.vbo = 0; }
    if (s_texBatch.ebo) { glDeleteBuffers(1, &s_texBatch.ebo); s_texBatch.ebo = 0; }
    if (s_texBatch.vao) { glDeleteVertexArrays(1, &s_texBatch.vao); s_texBatch.vao = 0; }
    #if defined(_MSC_VER)
    if (s_texBatch.prog){ pglDeleteProgram(s_texBatch.prog); s_texBatch.prog = 0; }
    #elif defined(__APPLE__)
    if (s_texBatch.prog){ glDeleteProgram(s_texBatch.prog); s_texBatch.prog = 0; }
    #endif
    s_texBatch.samplerLoc = -1;
}

static void texbatch_maybe_grow(size_t requiredQuads) {
    if (requiredQuads <= s_texBatch.capQuads) return;

    size_t newCap = s_texBatch.capQuads;
    while (newCap < requiredQuads) newCap <<= 1;

    float* newV = (float*)realloc(
        s_texBatch.vtxData, newCap * 4 * TEX_VTX_STRIDE_FLOATS * sizeof(float));
    if (!newV) {
        fprintf(stderr, "Out of memory growing textured batch.\n");
        return;
    }
    s_texBatch.vtxData = newV;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_texBatch.ebo);
    const size_t indexCount = newCap * 6;
    GLuint* indices = (GLuint*)malloc(indexCount * sizeof(GLuint));
    build_quad_indices(indices, newCap);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(indexCount * sizeof(GLuint)),
                 indices, GL_STATIC_DRAW);
    free(indices);

    s_texBatch.capQuads = newCap;
}

static void texbatch_flush(void) {
    if (s_texBatch.countQuads == 0 || s_texBatch.currentTex == 0) return;

    const size_t vCount = s_texBatch.countQuads * 4;
    const size_t vBytes = vCount * TEX_VTX_STRIDE_FLOATS * sizeof(float);
    const size_t iCount = s_texBatch.countQuads * 6;

    glUseProgram(s_texBatch.prog);
    glBindVertexArray(s_texBatch.vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_texBatch.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_texBatch.ebo);

    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vBytes, NULL, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)vBytes, s_texBatch.vtxData);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_texBatch.currentTex);
    if (s_texBatch.samplerLoc >= 0) glUniform1i(s_texBatch.samplerLoc, 0);

    glDrawElements(GL_TRIANGLES, (GLsizei)iCount, GL_UNSIGNED_INT, (void*)0);

    s_texBatch.countQuads = 0;
}

static void texbatch_push_quad(GLuint tex, int x, int y, int w, int h, bool flipY) {
    if (w <= 0 || h <= 0 || s_fbW <= 0 || s_fbH <= 0) return;

    // Texture change → flush current batch
    if (s_texBatch.countQuads > 0 && tex != s_texBatch.currentTex) {
        texbatch_flush();
    }
    s_texBatch.currentTex = tex;

    const size_t need = s_texBatch.countQuads + 1;
    if (need > s_texBatch.capQuads) {
        texbatch_maybe_grow(need);
        if (need > s_texBatch.capQuads) return; // OOM guard
    }

    // Pixel -> NDC (origin: top-left)
    const float L =  2.0f * ((float)x        / (float)s_fbW) - 1.0f;
    const float R =  2.0f * ((float)(x + w)  / (float)s_fbW) - 1.0f;
    const float T =  1.0f - 2.0f * ((float)y        / (float)s_fbH);
    const float B =  1.0f - 2.0f * ((float)(y + h)  / (float)s_fbH);

    const float u0 = 0.0f, v0 = flipY ? 1.0f : 0.0f;
    const float u1 = 1.0f, v1 = flipY ? 0.0f : 1.0f;

    float* v = s_texBatch.vtxData + (s_texBatch.countQuads * 4 * TEX_VTX_STRIDE_FLOATS);

    // V0 (L,T)
    v[0]=L; v[1]=T; v[2]=u0; v[3]=v0;
    // V1 (L,B)
    v[4]=L; v[5]=B; v[6]=u0; v[7]=v1;
    // V2 (R,T)
    v[8]=R; v[9]=T; v[10]=u1; v[11]=v0;
    // V3 (R,B)
    v[12]=R; v[13]=B; v[14]=u1; v[15]=v1;

    s_texBatch.countQuads += 1;
}

// -----------------------------------------------------------------------------
// Public API
void RendererInit(void) {
    rectbatch_init(2048);
    texbatch_init(2048);
    s_fbW = s_fbH = 0;

    // Optional: enable blending once (premultiplied recommended if assets are PMA)
    // glEnable(GL_BLEND);
    // glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void RendererShutdown(void) {
    texbatch_shutdown();
    rectbatch_shutdown();
}

void Begin2D(void) {
    GetFramebufferSize(&s_fbW, &s_fbH);
    s_rectBatch.countQuads = 0;
    s_texBatch.countQuads  = 0;
    // No GL binds here; flush does correct binds per batch.
}

void End2D(void) {
    // If you tend to draw rects then sprites, this order preserves that pattern
    rectbatch_flush();
    texbatch_flush();
    GL_SwapBuffers();
}

void Flush2D(void) {
    rectbatch_flush();
    texbatch_flush();
}

void DrawRectangle(int x, int y, int w, int h, Color c) {
    rectbatch_push(x, y, w, h, c.r, c.g, c.b, c.a);
}

void DrawTexture(Texture* tex, int x, int y, int w, int h, bool flipY) {
    if (!tex || !tex->id) return;
    if (w <= 0) w = tex->width;
    if (h <= 0) h = tex->height;
    texbatch_push_quad(tex->id, x, y, w, h, flipY);
}

// -----------------------------------------------------------------------------
// Texture I/O (stb_image)
Texture LoadTexture(const char* path) {
    Texture t = {0};
    int comp = 0;
    unsigned char* data = stbi_load(path, &t.width, &t.height, &comp, 4);
    if (!data) {
        fprintf(stderr, "stb_image: failed to load '%s'\n", path);
        return t;
    }

    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // or LINEAR / mipmapped
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t.width, t.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    return t;
}

void DestroyTexture(Texture* t) {
    if (t && t->id) {
        GLuint id = t->id;
        glDeleteTextures(1, &id);
        t->id = 0;
        t->width = t->height = 0;
    }
}