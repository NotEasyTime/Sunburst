#define NOB_IMPLEMENTATION
#include "nob.h"

const char *srcs[] = {"src/sunburst_draw.c", "src/sunburst.c", "src/sunburst_ui.c"};
const char *objs[] = {"build/sunburst_draw.o", "build/sunburst.o", "build/sunburst_ui.o"};

int unix_sb_lib(){
    Nob_Cmd cmd = {0};
    for (int i = 0; i < (int)NOB_ARRAY_LEN(srcs); ++i) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "cc", "-c", srcs[i], "-o", objs[i], "-DGL_SILENCE_DEPRECATION");
        if (!nob_cmd_run(&cmd)) return 1;
    }
    cmd.count = 0;
    nob_cmd_append(&cmd, "libtool", "-static", "-o", "build/sunburst.a", objs[0], objs[1], objs[2]);
    if (!nob_cmd_run(&cmd)) return 1;
    return 0;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    if (!nob_mkdir_if_not_exists("build")) return 1;

    Nob_Cmd cmd = {0};

#if defined(__APPLE__)

    unix_sb_lib();
    
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
    
    for (int i = 0; i < (int)NOB_ARRAY_LEN(srcs); ++i) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "cl", "/c", srcs[i],
            "/Fo:", objs[i], "/std:c11", "/O2", "/EHsc", "/nologo");
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
            objs[0], objs[1], objs[2],
            "build/gameEx.obj",
            "opengl32.lib", "gdi32.lib", "user32.lib",
            "/OUT:build/game.exe", "/SUBSYSTEM:CONSOLE", "/nologo"
        );
        if (!nob_cmd_run(&cmd)) return 1;
    } else {
        nob_log(NOB_INFO, "No target provided — built engine objects.");
    }

#elif defined(__linux__)
    
    unix_sb_lib();

     if (argc > 1) {
        cmd.count = 0;
        nob_cmd_append(&cmd, "cc", "-c", argv[1], "-o", "build/gameEx.o", "-DGL_SILENCE_DEPRECATION", "-Wno-undefined-inline");
        if (!nob_cmd_run(&cmd)) return 1;

        cmd.count = 0;
        nob_cmd_append(&cmd, "cc",
            "-o", "build/game",
            "build/gameEx.o",
            "build/sunburst.a", "build/libglfw3.a", 
            "-lGL", "-lm", "-ldl", "-lpthread", 
            "-lX11", "-lXrandr", "-lXi", "-lXxf86vm", 
            "-lXinerama", "-lXcursor"
        );
        if (!nob_cmd_run(&cmd)) return 1;
    } else {
        nob_log(NOB_INFO, "No target provided — built engine static lib.");
    }

#else
#   error "Unsupported platform"
#endif
    return 0;
}
