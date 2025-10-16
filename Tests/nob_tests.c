#define NOB_IMPLEMENTATION
#include "../nob.h"

// Some folder paths that we use throughout the build process.
#define BUILD_FOLDER "build/"
#define SRC_FOLDER   "src/"
#define TESTS        "tests/"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;
    Nob_Cmd cmd = {0};



#if defined(__APPLE__)
    // 1) Compile the C example
    nob_cmd_append(&cmd,
        "clang",
        "-c",
        argv[1],
        "-o", BUILD_FOLDER"gameEx.o",
        "-DGL_SILENCE_DEPRECATION"       // optional: quiets macOS GL deprecation warnings
    );
    nob_cmd_run(&cmd);

    // 2) Link with swiftc so Swift stdlib is handled + frameworks are easy
    nob_cmd_append(&cmd,
        "swiftc",
        BUILD_FOLDER"gameEx.o",
        BUILD_FOLDER"sunburst_draw.o",
        BUILD_FOLDER"sunburst_image.o",
        BUILD_FOLDER"sunburst_input.o",
        BUILD_FOLDER"sunburst_utils.o",
        BUILD_FOLDER"sunburst.a",
        "-framework", "AppKit",
        "-framework", "OpenGL",
        "-o", BUILD_FOLDER"game",
        // Make sure Swift runtime can be found at runtime (usually /usr/lib/swift on macOS)
        "-Xlinker", "-rpath", "-Xlinker", "@executable_path",
        "-Xlinker", "-rpath", "-Xlinker", "/usr/lib/swift"
    );
    nob_cmd_run(&cmd);
#elif defined(__linux__)
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", BUILD_FOLDER"sunburst", SRC_FOLDER"gameEx.c");
#elif defined(_MSC_VER)
     // Example
    nob_cmd_append(&cmd, "cl", "/c", TESTS"gameEx.c",
        "/Fo:" BUILD_FOLDER "gameEx.obj", "/std:c11", "/O2", "/EHsc", "/nologo");
    nob_cmd_run(&cmd);

    // Link
    nob_cmd_append(&cmd,
        "link",
        BUILD_FOLDER"b_Win32.obj",
        BUILD_FOLDER"sb_gl_loader.obj",
        BUILD_FOLDER"sunburst_draw.obj",
        BUILD_FOLDER"sunburst_image.obj",
        BUILD_FOLDER"sunburst_utils.obj",
        BUILD_FOLDER"gameEx.obj",
        "opengl32.lib", "gdi32.lib", "user32.lib",
        "/OUT:" BUILD_FOLDER "sunburst.exe",
        "/SUBSYSTEM:CONSOLE",
        "/nologo"
    );

    nob_cmd_run(&cmd);

    #else
    #   error "Unsupported platform"
#endif 

    return 0;
}
