#define GLFW_INCLUDE_NONE
#include "../src/glfw3.h"

#include "../src/sunburst.h"

#define CLAY_IMPLEMENTATION
#include "../src/clay.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#if defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION 1
  #include <OpenGL/gl3.h>
#elif defined(__linux__)
  #include <GL/gl.h>
#endif

const Clay_Color COLOR_LIGHT  = (Clay_Color){224, 215, 210, 255};
const Clay_Color COLOR_RED    = (Clay_Color){168,  66,  28, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color){225, 138,  50, 255};

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

static Clay_ElementDeclaration sidebarItemConfig = (Clay_ElementDeclaration){
    .layout = {
        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(50) },
    },
    .backgroundColor = COLOR_ORANGE
};

typedef struct { int placeholder; } MyImageHandle;
static MyImageHandle profilePicture = {0};

static void SidebarItemComponent(void) {
    // Use a real Clay element id:
    CLAY(CLAY_ID("SidebarItem"), sidebarItemConfig) {
        // children go here if you want
    }
}

int main(void)
{
    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(640, 480, "OpenGL Triangle", NULL, NULL);
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

   
while (!glfwWindowShouldClose(window))
{
    int fbW = 0, fbH = 0;
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    fbW = viewport[2];
    fbH = viewport[3];

    Clay_SetLayoutDimensions((Clay_Dimensions){ fbW, fbH });
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
            // Sidebar
            CLAY(
                CLAY_ID("SideBar"),
                (Clay_ElementDeclaration){
                    .layout = {
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .sizing = { .width = CLAY_SIZING_FIXED(300), .height = CLAY_SIZING_GROW(0) },
                        .padding = CLAY_PADDING_ALL(16),
                        .childGap = 16
                    },
                    .backgroundColor = COLOR_LIGHT
                }
            ) {
                // Profile area
                CLAY(
                    CLAY_ID("ProfilePictureOuter"),
                    (Clay_ElementDeclaration){
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(0) },
                            .padding = CLAY_PADDING_ALL(16),
                            .childGap = 16,
                            .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
                        },
                        .backgroundColor = COLOR_RED
                    }
                ) {
                    CLAY(
                        CLAY_ID("ProfilePicture"),
                        (Clay_ElementDeclaration){
                            .layout = { .sizing = { .width = CLAY_SIZING_FIXED(60), .height = CLAY_SIZING_FIXED(60) } },
                            .image  = { .imageData = &profilePicture }
                        }
                    ) { }
                }

                for (int i = 0; i < 1; i++) {
                    SidebarItemComponent();
                }
            }
            CLAY(
                CLAY_ID("MainContent"),
                (Clay_ElementDeclaration){
                    .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) } },
                    .backgroundColor = COLOR_LIGHT
                }
            ) { }
        }

        Clay_RenderCommandArray cmds = Clay_EndLayout();

        ClearBackground();
        Begin2D(fbW, fbH);

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