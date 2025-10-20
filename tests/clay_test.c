#define CLAY_IMPLEMENTATION
#define RGFW_IMPLEMENTATION
#define RGFW_OPENGL
#define RGFW_DEBUG
#include "../src/RGFW.h"
#include "../src/clay.h"
#include "../src/sunburst.h"

#include <stdio.h>
#include <stdlib.h>

const Clay_Color COLOR_LIGHT  = (Clay_Color){224, 215, 210, 255};
const Clay_Color COLOR_RED    = (Clay_Color){168,  66,  28, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color){225, 138,  50, 255};

void HandleClayErrors(Clay_ErrorData errorData) {
    printf("%s", errorData.errorText.chars);

    switch (errorData.errorType) {
        default: /* intentionally fallthrough to just logging above */ break;
    }
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

int main(void) {
    RGFW_glHints* hints = RGFW_getGlobalHints_OpenGL();
    hints->major = 4;
    hints->minor = 1;
    RGFW_setGlobalHints_OpenGL(hints);

    i32 w = 800, h = 600;
    RGFW_window* win = RGFW_createWindow("a window", 0, 0, w, h, RGFW_windowCenter);
    RGFW_window_createContext_OpenGL(win, hints);

    RendererInit();

    uint64_t bytes = Clay_MinMemorySize();
    void* mem = malloc(bytes);
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(bytes, mem);
    Clay_Initialize(arena, (Clay_Dimensions){ w, h }, (Clay_ErrorHandler){ HandleClayErrors });

    while (RGFW_window_shouldClose(win) == RGFW_FALSE) {
        RGFW_event ev;
        while (RGFW_window_checkEvent(win, &ev)) { /* no-op */ }

        // Retina-correct drawable (framebuffer) size 
        // Use the framebuffer pixel size on macOS/Retina. This is the size GL actually renders to.
        int fbW = 0, fbH = 0;
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        fbW = viewport[2];
        fbH = viewport[3];

        Clay_SetLayoutDimensions((Clay_Dimensions){ fbW, fbH });

        i32 mouseX = 0, mouseY = 0;
        RGFW_window_getMouse(win, &mouseX, &mouseY);
        Clay_SetPointerState((Clay_Vector2){ (float)mouseX, (float)mouseY }, 0);

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

                    CLAY_TEXT(
                        CLAY_STRING("Clay - UI Library"),
                        CLAY_TEXT_CONFIG({ .fontSize = 24, .textColor = (Clay_Color){255,255,255,255} })
                    );
                }

                for (int i = 0; i < 5; i++) {
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
        Begin2D((int)fbW, (int)fbH);

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
        RGFW_window_swapBuffers_OpenGL(win);
    }
    RendererShutdown();
    RGFW_window_close(win);
    free(mem);
    return 0;
}
