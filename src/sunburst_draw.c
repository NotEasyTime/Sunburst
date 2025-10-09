#include "sunburst.h"

#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION 1
  #include <OpenGL/gl3.h>   // Core profile
#elif defined _WIN32
  #include <windows.h>  // must precede gl.h so WINGDIAPI/APIENTRY are defined
  #include <GL/gl.h>
#else
  #include <GL/gl.h>        // Or your loader on other platforms
#endif

#include <string.h> // memset
#include <stdio.h>

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

static void ensure_pipeline(void) {
    if (g_prog) return;

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

    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);

    // pos (location 0): 2 floats, color (location 1): 4 floats
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*6, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float)*6, (void*)(sizeof(float)*2));
}

// Convert pixel coords (origin top-left) to NDC
static void rect_to_vertices(int x, int y, int w, int h, int fbw, int fbh,
                             float r, float g, float b, float a, float* out /* 4*6 floats */)
{
    // Clamp
    if (w < 0) { x += w; w = -w; }
    if (h < 0) { y += h; h = -h; }
    if (fbw <= 0 || fbh <= 0) return;

    // Pixel -> NDC
    float L =  2.0f*( (float)x        / (float)fbw ) - 1.0f;
    float R =  2.0f*( (float)(x + w)  / (float)fbw ) - 1.0f;
    float T =  1.0f - 2.0f*( (float)y        / (float)fbh );
    float B =  1.0f - 2.0f*( (float)(y + h)  / (float)fbh );

    // Triangle strip: LT, LB, RT, RB — each vertex: [x,y,r,g,b,a]
    float v[24] = {
        L, T,  r,g,b,a,
        L, B,  r,g,b,a,
        R, T,  r,g,b,a,
        R, B,  r,g,b,a
    };
    memcpy(out, v, sizeof v);
}

void DrawRectangle(int x, int y, int width, int height, Color color) {
    ensure_pipeline();

    int fbw=0, fbh=0;
    GetFramebufferSize(&fbw, &fbh);
    if (fbw <= 0 || fbh <= 0) return;

    // Save minimal state we touch
    GLint prevProg=0; glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    GLint prevArrayBuffer=0; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);
    GLint prevVAO=0;
#if defined(__APPLE__)
    // GL 3.2 core on mac has VAOs; query binding
    glGetIntegerv(0x85B5/*GL_VERTEX_ARRAY_BINDING*/, &prevVAO);
#endif

    glUseProgram(g_prog);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);

    float verts[24];
    rect_to_vertices(x, y, width, height, fbw, fbh,
                     color.r, color.g, color.b, color.a, verts);

    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Restore state
#if defined(__APPLE__)
    glBindVertexArray((GLuint)prevVAO);
#endif
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)prevArrayBuffer);
    glUseProgram((GLuint)prevProg);
}
