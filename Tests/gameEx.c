#include "../src/sunburst.h"
#include <stdlib.h>

int WindowWidth = 1600;
int WindowHeight = 800;
int fbw, fbh;

typedef struct Bouncer{

      int x, y, width, height;
      float dx, dy;
      int jumpRate;
      Color c;

    } Bouncer;

  void DrawBouncer(Bouncer *b){
    DrawRectangle(b->x, b->y, b->width, b->height, b->c);
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

    Bouncer bees[1000];
    for(int i = 0; i < 1000; i++){
      bees[i] = (Bouncer){5 + (12 * i),5 - ( i * 30),10,10,0,0,i,(Color){1, i * .01 ,i * .001,1}};
    }

    int i = 0;
    /* Good API */
    while (!WindowShouldClose()) {
        /* Will abstract */
        PollEvents();

        ++i;
        if(i > 1000) i = 0;

        Begin2D();
        /* Good API */
        ClearBackground();

        
        GetFramebufferSize(&fbw, &fbh);
        for(int i = 0; i < 1000; ++i){
          if((rand() % 1000) == bees[i].jumpRate){
            bees[i].dy += -5;
          }
          UpdateBouncer(&bees[i]);
        }

        End2D();

        /* Good API */
        PrintFrameRate();

    }
    /* Good API */
    RendererShutdown();
    CloseWindowSB();
    return 0;
}
