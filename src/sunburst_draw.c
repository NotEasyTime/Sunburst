#include "sunburst.h"
#include "stb_image.h"
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



// -----------------------------------------------------------------------------
// Attribute bindings for both pipelines
#define ATTR_POS   0
#define ATTR_COLOR 1
#define ATTR_UV    1  // reuses location 1 in the texture program

// Rect batch layout: [x, y, r, g, b, a]
#define VERT_STRIDE_FLOATS 6
#define VERTS_PER_RECT     6

// -----------------------------------------------------------------------------
// Rectangle pipeline (shaders)
static const char* s_rectVS =
"#version 150 core\n"
"in vec2 pos;\n"
"in vec4 inColor;\n"
"out vec4 vColor;\n"
"void main(){\n"
"  vColor = inColor;\n"
"  gl_Position = vec4(pos, 0.0, 1.0);\n"
"}\n";

static const char* s_rectFS =
"#version 150 core\n"
"in vec4 vColor;\n"
"out vec4 outColor;\n"
"void main(){ outColor = vColor; }\n";

// Texture pipeline (shaders)
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
// GL objects and CPU batch
static GLuint s_rectProg = 0;
static GLuint s_rectVAO  = 0;
static GLuint s_rectVBO  = 0;

static GLuint s_texProg      = 0;
static GLuint s_texVAO       = 0;
static GLuint s_texVBO       = 0;
static GLint  s_texSamplerLo = -1;

static size_t s_batchCapVerts = 0;      // capacity in vertices
static size_t s_batchCount    = 0;      // queued vertices
static float* s_batchData     = NULL;   // CPU-side staging

static int s_fbW = 0, s_fbH = 0;        // cached each frame

// -----------------------------------------------------------------------------
// External platform hooks
extern void GetFramebufferSize(int* outW, int* outH);
extern void GL_SwapBuffers(void);

// -----------------------------------------------------------------------------
// Utilities
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

static GLuint link_program(GLuint vs, GLuint fs,
                           void (*bind_attribs)(GLuint prog)) {
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

// -----------------------------------------------------------------------------
// Pipeline setup
static void bind_rect_attribs(GLuint prog) {
    glBindAttribLocation(prog, ATTR_POS,   "pos");
    glBindAttribLocation(prog, ATTR_COLOR, "inColor");
}

static void bind_tex_attribs(GLuint prog) {
    glBindAttribLocation(prog, ATTR_POS, "pos");
    glBindAttribLocation(prog, ATTR_UV,  "uv");
}

static void init_rect_pipeline(void) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   s_rectVS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, s_rectFS);
    s_rectProg = link_program(vs, fs, bind_rect_attribs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &s_rectVAO);
    glBindVertexArray(s_rectVAO);

    glGenBuffers(1, &s_rectVBO);
    glBindBuffer(GL_ARRAY_BUFFER, s_rectVBO);

    const GLsizei stride = (GLsizei)(sizeof(float) * VERT_STRIDE_FLOATS);
    glEnableVertexAttribArray(ATTR_POS);
    glVertexAttribPointer(ATTR_POS, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(ATTR_COLOR);
    glVertexAttribPointer(ATTR_COLOR, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*2));
}

static void init_texture_pipeline(void) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   s_texVS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, s_texFS);
    s_texProg = link_program(vs, fs, bind_tex_attribs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    s_texSamplerLo = glGetUniformLocation(s_texProg, "tex");

    glGenVertexArrays(1, &s_texVAO);
    glBindVertexArray(s_texVAO);

    glGenBuffers(1, &s_texVBO);
    glBindBuffer(GL_ARRAY_BUFFER, s_texVBO);

    const GLsizei stride = (GLsizei)(sizeof(float) * 4); // [x,y,u,v]
    glEnableVertexAttribArray(ATTR_POS);
    glVertexAttribPointer(ATTR_POS, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(ATTR_UV);
    glVertexAttribPointer(ATTR_UV,  2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)*2));
}

// -----------------------------------------------------------------------------
// Batch handling
static void flush_batch(void) {
    if (s_batchCount == 0) return;

    const GLsizeiptr byteCount =
        (GLsizeiptr)(s_batchCount * VERT_STRIDE_FLOATS * sizeof(float));

    // Stream path: orphan the buffer, then upload
    glBufferData(GL_ARRAY_BUFFER, byteCount, NULL, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, byteCount, s_batchData);
    glDrawArrays(GL_TRIANGLES, 0, (GLint)s_batchCount);

    s_batchCount = 0;
}

static inline void push_rect_pixels_to_batch(
    int x, int y, int w, int h, float r, float g, float b, float a)
{
    if (w == 0 || h == 0) return;
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    if (s_fbW <= 0 || s_fbH <= 0) return;

    // Pixel -> NDC (origin top-left)
    const float L =  2.0f * ((float)x        / (float)s_fbW) - 1.0f;
    const float R =  2.0f * ((float)(x + w)  / (float)s_fbW) - 1.0f;
    const float T =  1.0f - 2.0f * ((float)y        / (float)s_fbH);
    const float B =  1.0f - 2.0f * ((float)(y + h)  / (float)s_fbH);

    // Ensure capacity (6 verts per rect)
    const size_t need = s_batchCount + VERTS_PER_RECT;
    if (need > s_batchCapVerts) {
        // Flush what we have first
        flush_batch();
        if (VERTS_PER_RECT > s_batchCapVerts) {
            size_t newCap = s_batchCapVerts ? s_batchCapVerts : 8192;
            while (newCap < VERTS_PER_RECT) newCap <<= 1;
            float* newData = (float*)realloc(
                s_batchData, sizeof(float) * newCap * VERT_STRIDE_FLOATS);
            if (!newData) {
                fprintf(stderr, "Out of memory growing 2D batch.\n");
                return;
            }
            s_batchData = newData;
            s_batchCapVerts = newCap;
        }
    }

    float* v = s_batchData + s_batchCount * VERT_STRIDE_FLOATS;

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

    s_batchCount += VERTS_PER_RECT;
}

// -----------------------------------------------------------------------------
// Public API
void RendererInit(void) {
    // Pipelines
    init_rect_pipeline();
    init_texture_pipeline();

    // Allocate CPU batch (verts)
    s_batchCapVerts = 8192; // ~1365 rects
    s_batchData = (float*)malloc(sizeof(float) * s_batchCapVerts * VERT_STRIDE_FLOATS);
    s_batchCount = 0;
}

void RendererShutdown(void) {
    // CPU
    free(s_batchData); s_batchData = NULL;
    s_batchCapVerts = 0;
    s_batchCount = 0;

    // Rect pipeline GL
    if (s_rectVBO)  { glDeleteBuffers(1, &s_rectVBO);  s_rectVBO  = 0; }
    if (s_rectVAO)  { glDeleteVertexArrays(1, &s_rectVAO); s_rectVAO = 0; }
    if (s_rectProg) { glDeleteProgram(s_rectProg); s_rectProg = 0; }

    // Texture pipeline GL
    if (s_texVBO)   { glDeleteBuffers(1, &s_texVBO);   s_texVBO   = 0; }
    if (s_texVAO)   { glDeleteVertexArrays(1, &s_texVAO); s_texVAO = 0; }
    if (s_texProg)  { glDeleteProgram(s_texProg); s_texProg = 0; }

    s_texSamplerLo = -1;
}

void Begin2D(void) {
    glUseProgram(s_rectProg);
    glBindVertexArray(s_rectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, s_rectVBO);

    GetFramebufferSize(&s_fbW, &s_fbH);
    s_batchCount = 0;
}

static inline void restore_rect_pipeline(void) {
    glUseProgram(s_rectProg);
    glBindVertexArray(s_rectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, s_rectVBO);
}

void End2D(void) {
    flush_batch();
    GL_SwapBuffers();
}

void Flush2D(void) {
    flush_batch();
}

void DrawRectangle(int x, int y, int w, int h, Color color) {
    push_rect_pixels_to_batch(x, y, w, h, color.r, color.g, color.b, color.a);
}

// -----------------------------------------------------------------------------
// Textures
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
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

void DrawTexture(Texture* tex, int x, int y, int w, int h, bool flipY) {
    if (!tex || !tex->id) return;

    if (w <= 0) w = tex->width;
    if (h <= 0) h = tex->height;

    // Ensure we're in a known state and flush rects first
    Flush2D();

    // Convert pixel quad to NDC
    const float L =  2.0f * ((float)x        / (float)s_fbW) - 1.0f;
    const float R =  2.0f * ((float)(x + w)  / (float)s_fbW) - 1.0f;
    const float T =  1.0f - 2.0f * ((float)y        / (float)s_fbH);
    const float B =  1.0f - 2.0f * ((float)(y + h)  / (float)s_fbH);

    const float u0 = 0.0f, v0 = flipY ? 1.0f : 0.0f;
    const float u1 = 1.0f, v1 = flipY ? 0.0f : 1.0f;

    // 2 triangles, [x,y,u,v]
    const float quad[6*4] = {
        L,T,u0,v0,  L,B,u0,v1,  R,T,u1,v0,
        R,T,u1,v0,  L,B,u0,v1,  R,B,u1,v1
    };

    glUseProgram(s_texProg);
    glBindVertexArray(s_texVAO);
    glBindBuffer(GL_ARRAY_BUFFER, s_texVBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(quad), quad, GL_STREAM_DRAW);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex->id);
    if (s_texSamplerLo >= 0) glUniform1i(s_texSamplerLo, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Restore rectangle pipeline so subsequent DrawRectangle calls just work
    restore_rect_pipeline();
}