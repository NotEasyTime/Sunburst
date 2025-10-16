#include "../src/sunburst.h"
#include <stdlib.h>

int WindowWidth = 1600;
int WindowHeight = 800;
int fbw, fbh;
Texture bee;

typedef struct Bouncer{

      int x, y, width, height;
      float dx, dy;
      int jumpRate;
      Color c;

    } Bouncer;

  void DrawBouncer(Bouncer *b){
    DrawTexture(&bee, b->x, b->y, b->width, b->height, true);
  }


  void Physics(Bouncer *b) {

    b->dy += 0.5f;

    b->x += b->dx;
    b->y += b->dy;

    float floorY = fbh - b->height;
    if (b->y > floorY) {
        b->y = floorY;
        b->dy = -b->dy * 0.7f;  
    }
}

  void UpdateBouncer(Bouncer *b){
    Physics(b);
    DrawBouncer(b);
  }



int main(void) {

    /* Good API */
    InitWindow(WindowWidth, WindowHeight, "Bounce");


    /* Will abstract */
    CreateGLContext();
    GL_SetSwapInterval(1);

    RendererInit();

    bee = LoadTexture("bee.png");

    Bouncer beeb = (Bouncer){50 + (120 ),50 - ( 30),100,100,0,0,0,(Color){1, 0 ,0,1}};
    

    int i = 0;
    /* Good API */
    while (!WindowShouldClose()) {
        /* Will abstract */
        InputBeginFrame(); 
        PollEvents();

        if (IsKeyPressed(KEY_ESCAPE)) return 0;
        if (IsKeyPressed(KEY_SPACE)) beeb.dy = -10;

        ++i;
        if(i > 1000) i = 0;

        Begin2D();
        /* Good API */
        ClearBackground();

        
        GetFramebufferSize(&fbw, &fbh);
        
        UpdateBouncer(&beeb);
        

        End2D();

        /* Good API */
        PrintFrameRate();

    }
    /* Good API */
    DestroyTexture(&bee);
    RendererShutdown();
    CloseWindowSB();
    return 0;
}
