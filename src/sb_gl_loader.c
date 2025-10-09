#include "sb_gl_loader.h"

#ifdef _WIN32
PFNGLCREATEPROGRAMPROC            pglCreateProgram;
PFNGLCREATESHADERPROC             pglCreateShader;
PFNGLSHADERSOURCEPROC             pglShaderSource;
PFNGLCOMPILESHADERPROC            pglCompileShader;
PFNGLGETSHADERIVPROC              pglGetShaderiv;
PFNGLGETSHADERINFOLOGPROC         pglGetShaderInfoLog;
PFNGLATTACHSHADERPROC             pglAttachShader;
PFNGLLINKPROGRAMPROC              pglLinkProgram;
PFNGLGETPROGRAMIVPROC             pglGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC        pglGetProgramInfoLog;
PFNGLUSEPROGRAMPROC               pglUseProgram;
PFNGLDELETESHADERPROC             pglDeleteShader;

PFNGLGENVERTEXARRAYSPROC          pglGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC          pglBindVertexArray;

PFNGLGENBUFFERSPROC               pglGenBuffers;
PFNGLBINDBUFFERPROC               pglBindBuffer;
PFNGLBUFFERDATAPROC               pglBufferData;
PFNGLENABLEVERTEXATTRIBARRAYPROC  pglEnableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC      pglVertexAttribPointer;

PFNGLGETINTEGERVPROC              pglGetIntegerv;
PFNGLDRAWARRAYSPROC               pglDrawArrays;
PFNGLBINDATTRIBLOCATIONPROC       pglBindAttribLocation;

static void* get_proc_addr(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (!p) {
        static HMODULE ogl = NULL;
        if (!ogl) ogl = LoadLibraryA("opengl32.dll");
        if (ogl) p = (void*)GetProcAddress(ogl, name);
    }
    return p;
}

static bool load_fn(void** dst, const char* name) {
    *dst = get_proc_addr(name);
    return *dst != NULL;
}

bool sbgl_load_win32(void) {
    // Shaders / program
    if (!load_fn((void**)&pglCreateProgram,           "glCreateProgram")) return false;
    if (!load_fn((void**)&pglCreateShader,            "glCreateShader")) return false;
    if (!load_fn((void**)&pglShaderSource,            "glShaderSource")) return false;
    if (!load_fn((void**)&pglCompileShader,           "glCompileShader")) return false;
    if (!load_fn((void**)&pglGetShaderiv,             "glGetShaderiv")) return false;
    if (!load_fn((void**)&pglGetShaderInfoLog,        "glGetShaderInfoLog")) return false;
    if (!load_fn((void**)&pglAttachShader,            "glAttachShader")) return false;
    if (!load_fn((void**)&pglLinkProgram,             "glLinkProgram")) return false;
    if (!load_fn((void**)&pglGetProgramiv,            "glGetProgramiv")) return false;
    if (!load_fn((void**)&pglGetProgramInfoLog,       "glGetProgramInfoLog")) return false;
    if (!load_fn((void**)&pglUseProgram,              "glUseProgram")) return false;
    if (!load_fn((void**)&pglDeleteShader,            "glDeleteShader")) return false;

    // VAO
    if (!load_fn((void**)&pglGenVertexArrays,         "glGenVertexArrays")) return false;
    if (!load_fn((void**)&pglBindVertexArray,         "glBindVertexArray")) return false;

    // VBO
    if (!load_fn((void**)&pglGenBuffers,              "glGenBuffers")) return false;
    if (!load_fn((void**)&pglBindBuffer,              "glBindBuffer")) return false;
    if (!load_fn((void**)&pglBufferData,              "glBufferData")) return false;
    if (!load_fn((void**)&pglEnableVertexAttribArray, "glEnableVertexAttribArray")) return false;
    if (!load_fn((void**)&pglVertexAttribPointer,     "glVertexAttribPointer")) return false;

    // Misc
    if (!load_fn((void**)&pglGetIntegerv,             "glGetIntegerv")) return false;
    if (!load_fn((void**)&pglDrawArrays,              "glDrawArrays")) return false;

    if (!load_fn((void**)&pglBindAttribLocation,       "glBindAttribLocation")) return false;

    return true;
}
#endif
