#include "../src/sunburst.h"

const int ww = 800;
const int wh = 600; 

int main(){

    InitWindow(ww, wh, "Sunburst Stress Test - Bees");
    CreateGLContext();
    GL_SetSwapInterval(1);
    RendererInit();

    Rect r = (Rect){50,50,150,150};


    while(!WindowShouldClose()){

        InputBeginFrame();
        PollEvents();

        if (IsKeyPressed(KEY_ESCAPE)) return 0;

        int mx, my;
        GetMousePosition(&mx, &my);

        Begin2D();
        ClearBackground();

        DrawRectangleRect(&r, (Color){0,1,0,1});

        End2D();

        PrintFrameRate();

    }

    RendererShutdown();
    CloseWindowSB();

    return 0;
}