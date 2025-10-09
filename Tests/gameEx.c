#include "../src/sunburst.h"
#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION 1
  #include <OpenGL/gl3.h>
#else
  #include <GL/gl.h>
#endif

int main(void) {
    InitWindow(800, 600, "Rects");
    CreateGLContext();
    GL_SetSwapInterval(0);

    int i = 0;
    while (!WindowShouldClose()) {
        PollEvents();

        ++i;

        int w=0,h=0;
        GetFramebufferSize(&w,&h);
        glViewport(0,0,w,h);
        glClearColor(0.08f,0.09f,0.11f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        for(int j = 0; j < 100; ++j){
            DrawRectangle(20 + i, 20 + j, 160, 100, (Color){1,0,0,1});
            DrawRectangle(200 + i, 60 + j, 120, 80, (Color){0,1,0,0.7f});
            DrawRectangle(360 + j, 40 + i, 200, 200, (Color){0.2f,0.6f,1.0f,1});
        }

        GL_SwapBuffers();

        if(i > 800) i = 0;
        PrintFrameRate();

    }
    DestroyWindow();
    return 0;
}
