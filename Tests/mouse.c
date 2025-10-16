#include "../src/sunburst.h"

const int ww = 800;
const int wh = 600; 

int main(){

    InitWindow(ww, wh, "Sunburst Stress Test - Bees");
    CreateGLContext();
    GL_SetSwapInterval(1);
    RendererInit();


    while(!WindowShouldClose()){

        InputBeginFrame();
        PollEvents();

        int mx, my;
        GetMousePosition(&mx, &my);

        Begin2D();
        ClearBackground();

        if(mx > 299){
            DrawRectangle(250,300,50,50,(Color){0,0,1,1});
        } else {
            DrawRectangle(250,300,50,50,(Color){1,0,0,1});
        }

        End2D();

        PrintFrameRate();

    }

    RendererShutdown();
    CloseWindowSB();

    return 0;
}