#define GLFW_INCLUDE_NONE
#include "../src/glfw3.h"

#include "../src/sunburst.h"

#define CLAY_IMPLEMENTATION
#include "../src/clay.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <OpenGL/gl3.h>

void HandleClayErrors(Clay_ErrorData errorData) {
    printf("%s", errorData.errorText.chars);

    switch (errorData.errorType) {
        default: /* intentionally fallthrough to just logging above */ break;
    }
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}



int main(void)
{
    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(640, 480, "Editor", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, key_callback);

    glfwMakeContextCurrent(window);
    //gladLoadGL(glfwGetProcAddress);
    glfwSwapInterval(1);

    RendererInit();

    uint64_t bytes = Clay_MinMemorySize();
    void* mem = malloc(bytes);
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(bytes, mem);
    Clay_Initialize(arena, (Clay_Dimensions){ 640, 480 }, (Clay_ErrorHandler){ HandleClayErrors });

    int size = 75;
    double xpos, ypos;
   
while (!glfwWindowShouldClose(window)){

    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1)) glfwGetCursorPos(window, &xpos, &ypos);

    int width, height;
    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    glfwGetWindowSize(window, &width, &height); 
    double mouse_x_px = xpos * xscale;
    double mouse_y_px = ypos * yscale;

    Clay_SetLayoutDimensions((Clay_Dimensions){ width * xscale, height * yscale });
    Clay_BeginLayout();

        // Outer container
        CLAY(
            CLAY_ID("OuterContainer"),
            (Clay_ElementDeclaration){
                
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .padding = CLAY_PADDING_ALL(16),
                    .childGap = 16
                },
                .backgroundColor = (Clay_Color){250,250,255,255}
            }
        ) {
            CLAY(
                    CLAY_ID("DragMe"),
                    (Clay_ElementDeclaration){
                        .layout = {
                        .sizing = {.width = mouse_x_px, .height = CLAY_SIZING_GROW(0) },
                        .padding = CLAY_PADDING_ALL(16),
                        .childGap = 16,
                        .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}
                    },
                    .backgroundColor = (Clay_Color){200,200,255,255}
                    }
                ) {

                    CLAY(CLAY_ID("INner"), (Clay_ElementDeclaration){
                        .layout = {
                        .sizing = { .width = 15, .height = CLAY_SIZING_GROW(0) },
                        .padding = CLAY_PADDING_ALL(16),
                        .childGap = 16
                    },
                    .backgroundColor = (Clay_Color){100,200,100,255}
                    }){}
                    CLAY(CLAY_ID("INner2"), (Clay_ElementDeclaration){
                        .layout = {
                        .sizing = { .width = 15, .height = CLAY_SIZING_PERCENT(.9) },
                        .padding = CLAY_PADDING_ALL(16),
                        .childGap = 16
                    },
                    .backgroundColor = (Clay_Color){100,200,100,255}
                    }){}
            } 
            CLAY(
                CLAY_ID("MainContent"),
                (Clay_ElementDeclaration){
                    .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } },
                    .backgroundColor = (Clay_Color){250,230,250,255}
                }
            ) { }
        }

        Clay_RenderCommandArray cmds = Clay_EndLayout();

        ClearBackground();
        Begin2D(width * xscale, height * yscale);

        for (int i = 0; i < cmds.length; i++) {
            Clay_RenderCommand* rc = &cmds.internalArray[i];
            switch (rc->commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                    Color c = {
                        rc->renderData.rectangle.backgroundColor.r / 255.0f,
                        rc->renderData.rectangle.backgroundColor.g / 255.0f,
                        rc->renderData.rectangle.backgroundColor.b / 255.0f,
                        rc->renderData.rectangle.backgroundColor.a / 255.0f
                    };
                    DrawRectangle(rc->boundingBox.x, rc->boundingBox.y,
                                  rc->boundingBox.width, rc->boundingBox.height, c);
                } break;
                default:
                    break;
            }
        }

        End2D();

    glfwSwapBuffers(window);
    glfwPollEvents();
}

    glfwDestroyWindow(window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}