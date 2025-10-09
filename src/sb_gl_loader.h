#pragma once
#include <stdbool.h>

#ifdef _WIN32
  #include <windows.h>   // must precede gl.h
  #include <GL/gl.h>

  // ---- Win32 GL 1.1 headers are ancient: define what we need ----
  #ifndef GL_ARRAY_BUFFER
    #define GL_ARRAY_BUFFER            0x8892
    #define GL_STATIC_DRAW             0x88E4
    #define GL_STREAM_DRAW             0x88E0
    #define GL_VERTEX_SHADER           0x8B31
    #define GL_FRAGMENT_SHADER         0x8B30
    #define GL_COMPILE_STATUS          0x8B81
    #define GL_LINK_STATUS             0x8B82
    #define GL_INFO_LOG_LENGTH         0x8B84
    #define GL_COLOR_BUFFER_BIT        0x00004000
    #define GL_CURRENT_PROGRAM         0x8B8D
    #define GL_ARRAY_BUFFER_BINDING    0x8894
  #endif

  // ---- Typedefs for the functions we’ll load ----
  typedef char GLchar;
  typedef ptrdiff_t GLsizeiptr;

  typedef GLuint (APIENTRY * PFNGLCREATEPROGRAMPROC)(void);
  typedef GLuint (APIENTRY * PFNGLCREATESHADERPROC)(GLenum);
  typedef void   (APIENTRY * PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar* const*, const GLint*);
  typedef void   (APIENTRY * PFNGLCOMPILESHADERPROC)(GLuint);
  typedef void   (APIENTRY * PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
  typedef void   (APIENTRY * PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
  typedef void   (APIENTRY * PFNGLATTACHSHADERPROC)(GLuint, GLuint);
  typedef void   (APIENTRY * PFNGLLINKPROGRAMPROC)(GLuint);
  typedef void   (APIENTRY * PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
  typedef void   (APIENTRY * PFNGLGETPROGRAMINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
  typedef void   (APIENTRY * PFNGLUSEPROGRAMPROC)(GLuint);
  typedef void   (APIENTRY * PFNGLDELETESHADERPROC)(GLuint);

  typedef void   (APIENTRY * PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
  typedef void   (APIENTRY * PFNGLBINDVERTEXARRAYPROC)(GLuint);

  typedef void   (APIENTRY * PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
  typedef void   (APIENTRY * PFNGLBINDBUFFERPROC)(GLenum, GLuint);
  typedef void   (APIENTRY * PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void*, GLenum);
  typedef void   (APIENTRY * PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
  typedef void   (APIENTRY * PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

  typedef void   (APIENTRY * PFNGLGETINTEGERVPROC)(GLenum, GLint*);
  typedef void   (APIENTRY * PFNGLDRAWARRAYSPROC)(GLenum, GLint, GLsizei);
  // after other typedefs
  typedef void (APIENTRY * PFNGLBINDATTRIBLOCATIONPROC)(GLuint program, GLuint index, const GLchar* name);

  // extern pointer
  extern PFNGLBINDATTRIBLOCATIONPROC       pglBindAttribLocation;


  // ---- Extern pointers (defined in .c) ----
  extern PFNGLCREATEPROGRAMPROC            pglCreateProgram;
  extern PFNGLCREATESHADERPROC             pglCreateShader;
  extern PFNGLSHADERSOURCEPROC             pglShaderSource;
  extern PFNGLCOMPILESHADERPROC            pglCompileShader;
  extern PFNGLGETSHADERIVPROC              pglGetShaderiv;
  extern PFNGLGETSHADERINFOLOGPROC         pglGetShaderInfoLog;
  extern PFNGLATTACHSHADERPROC             pglAttachShader;
  extern PFNGLLINKPROGRAMPROC              pglLinkProgram;
  extern PFNGLGETPROGRAMIVPROC             pglGetProgramiv;
  extern PFNGLGETPROGRAMINFOLOGPROC        pglGetProgramInfoLog;
  extern PFNGLUSEPROGRAMPROC               pglUseProgram;
  extern PFNGLDELETESHADERPROC             pglDeleteShader;

  extern PFNGLGENVERTEXARRAYSPROC          pglGenVertexArrays;
  extern PFNGLBINDVERTEXARRAYPROC          pglBindVertexArray;

  extern PFNGLGENBUFFERSPROC               pglGenBuffers;
  extern PFNGLBINDBUFFERPROC               pglBindBuffer;
  extern PFNGLBUFFERDATAPROC               pglBufferData;
  extern PFNGLENABLEVERTEXATTRIBARRAYPROC  pglEnableVertexAttribArray;
  extern PFNGLVERTEXATTRIBPOINTERPROC      pglVertexAttribPointer;

  extern PFNGLGETINTEGERVPROC              pglGetIntegerv;
  extern PFNGLDRAWARRAYSPROC               pglDrawArrays;

  // Call after a GL context is current
  bool sbgl_load_win32(void);

#else
  // Non-Windows: nothing to load
  static inline bool sbgl_load_win32(void) { return true; }
#endif
