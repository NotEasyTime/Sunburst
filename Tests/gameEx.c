#include "../src/sunburst.h"
#include <stdlib.h>

int WindowWidth = 1600;
int WindowHeight = 800;

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

    float floorY = WindowHeight - b->height;
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

    Bouncer bees[10];
    for(int i = 0; i < 10; i++){
      bees[i] = (Bouncer){50 + (125 * i),50 - ( i * 300),100,100,0,0,i,(Color){1, i * .1 ,0,1}};
    }

    int i = 0;
    /* Good API */
    while (!WindowShouldClose()) {
        /* Will abstract */
        PollEvents();

        ++i;
        if(i > 1000) i = 0;

        /* Good API */
        ClearBackground();

        for(int i = 0; i < 10; ++i){
          if((rand() % 100) == bees[i].jumpRate){
            bees[i].dy += -5;
          }
          UpdateBouncer(&bees[i]);
        }
        
        /* Will abstract */
        GL_SwapBuffers();

        if(i > 800) i = 0;

        /* Good API */
        PrintFrameRate();

    }
    /* Good API */
    CloseWindowSB();
    return 0;
}
