#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    if (!nob_mkdir_if_not_exists("build")) return 1;

    Nob_Cmd cmd = {0};

#if defined(__APPLE__)
    const char *srcs[] = {"src/sunburst_draw.c", "src/sunburst.c", "src/sunburst_ui.c"};
    const char *objs[] = {"build/sunburst_draw.o", "build/sunburst.o", "build/sunburst_ui.o"};
    for (int i = 0; i < (int)NOB_ARRAY_LEN(srcs); ++i) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "clang", "-c", srcs[i], "-o", objs[i], "-DGL_SILENCE_DEPRECATION");
        if (!nob_cmd_run(&cmd)) return 1;
    }
    cmd.count = 0;
    nob_cmd_append(&cmd, "libtool", "-static", "-o", "build/sunburst.a", objs[0], objs[1], objs[2]);
    if (!nob_cmd_run(&cmd)) return 1;

    if (argc > 1) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "clang", "-c", argv[1], "-o", "build/gameEx.o", "-DGL_SILENCE_DEPRECATION", "-Wno-undefined-inline");
        if (!nob_cmd_run(&cmd)) return 1;

        cmd.count = 0;
        nob_cmd_append(&cmd,
            "clang",
            "build/gameEx.o", "build/libglfw3.a", "build/sunburst.a",
            "-framework", "Cocoa", "-framework", "OpenGL", "-framework", "IOKit",
            "-framework", "CoreVideo",
            "-framework", "QuartzCore",
            "-o", "build/game"
        );
        if (!nob_cmd_run(&cmd)) return 1;
    } else {
        nob_log(NOB_INFO, "No target provided — built engine static lib.");
    }

#elif defined(_MSC_VER)
    const char *wsrcs[] = {
        "implements/b_Win32.c",
        "src/sb_gl_loader.c",
        "src/sunburst_draw.c",
        "src/sunburst_input.c",
        "src/sunburst_image.c",
        "src/sunburst.c",
    };
    const char *wobjs[] = {
        "build/b_Win32.obj",
        "build/sb_gl_loader.obj",
        "build/sunburst_draw.obj",
        "build/sunburst_input.obj",
        "build/sunburst_image.obj",
        "build/sunburst.obj",
    };
    for (int i = 0; i < (int)NOB_ARRAY_LEN(wsrcs); ++i) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "cl", "/c", wsrcs[i],
            "/Fo:", wobjs[i], "/std:c11", "/O2", "/EHsc", "/nologo");
        if (!nob_cmd_run(&cmd)) return 1;
    }

    if (argc > 1) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "cl", "/c", argv[1],
            "/Fo:", "build/gameEx.obj", "/std:c11", "/O2", "/EHsc", "/nologo");
        if (!nob_cmd_run(&cmd)) return 1;

        cmd.count = 0;
        nob_cmd_append(&cmd,
            "link",
            wobjs[0], wobjs[1], wobjs[2], wobjs[3], wobjs[4], wobjs[5],
            "build/gameEx.obj",
            "opengl32.lib", "gdi32.lib", "user32.lib",
            "/OUT:build/game.exe", "/SUBSYSTEM:CONSOLE", "/nologo"
        );
        if (!nob_cmd_run(&cmd)) return 1;
    } else {
        nob_log(NOB_INFO, "No target provided — built engine objects.");
    }

#elif defined(__linux__)
    cmd.count = 0;
    // Linux path here is your call; this mirrors your original single-link example.
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", "build/sunburst", "src/gameEx.c");
    if (!nob_cmd_run(&cmd)) return 1;

#else
#   error "Unsupported platform"
#endif

    return 0;
}
