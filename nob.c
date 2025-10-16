#define NOB_IMPLEMENTATION
#include "nob.h"

// Some folder paths that we use throughout the build process.
#define BUILD_FOLDER "build/"
#define SRC_FOLDER   "src/"
#define IMPLNTS      "implements/"
#define TESTS        "tests/"

int main(int argc, char **argv)
{
    // This line enables the self-rebuilding. It detects when nob.c is updated and auto rebuilds it then
    // runs it again.
    NOB_GO_REBUILD_URSELF(argc, argv);

    // It's better to keep all the building artifacts in a separate build folder. Let's create it if it
    // does not exist yet.
    //
    // Majority of the nob command return bool which indicates whether operation has failed or not (true -
    // success, false - failure). If the operation returned false you don't need to log anything, the
    // convention is usually that the function logs what happened to itself. Just do
    // `if (!nob_function()) return;`
    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    // The working horse of nob is the Nob_Cmd structure. It's a Dynamic Array of strings which represent
    // command line that you want to execute.
    Nob_Cmd cmd = {0};

    // Let's append the command line arguments
#if defined(__APPLE__)
    // --- Compile Swift source into object ---
    nob_cmd_append(&cmd,
        "swiftc",
        "-emit-object",
        "-parse-as-library",
        "-Ounchecked",
        "-sdk", "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk",
        "-framework", "AppKit",
        "-framework", "OpenGL",        // needed for NSOpenGLContext
        "-o", BUILD_FOLDER"sunburst.o",
        IMPLNTS"b_AppKit.swift"
    );
    nob_cmd_run(&cmd);

    // --- Archive to static lib ---
    nob_cmd_append(&cmd,
        "libtool",
        "-static",
        "-o", BUILD_FOLDER"sunburst.a",
        BUILD_FOLDER"sunburst.o"
    );
    nob_cmd_run(&cmd);
    nob_cmd_append(&cmd,
        "clang", "-c",
        SRC_FOLDER"sunburst_draw.c",
        "-o", BUILD_FOLDER"sunburst_draw.o",
        "-DGL_SILENCE_DEPRECATION"
    );
    nob_cmd_run(&cmd);
    nob_cmd_append(&cmd,
        "clang", "-c",
        SRC_FOLDER"sunburst_image.c",
        "-o", BUILD_FOLDER"sunburst_image.o",
        "-DGL_SILENCE_DEPRECATION"
    );
    nob_cmd_run(&cmd);
    nob_cmd_append(&cmd,
        "clang", "-c",
        SRC_FOLDER"sunburst_utils.c",
        "-o", BUILD_FOLDER"sunburst_utils.o"
    );
    nob_cmd_run(&cmd);
#elif defined(__linux__)
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", BUILD_FOLDER"sunburst", SRC_FOLDER"gameEx.c");
#elif defined(_MSC_VER)
    // Backend
    nob_cmd_append(&cmd, "cl", "/c", IMPLNTS"b_Win32.c",
        "/Fo:" BUILD_FOLDER "b_Win32.obj", "/std:c11", "/O2", "/EHsc",
        "/DUNICODE", "/D_UNICODE", "/nologo");
    nob_cmd_run(&cmd);

    // GL loader (Windows)
    nob_cmd_append(&cmd, "cl", "/c", SRC_FOLDER"sb_gl_loader.c",
        "/Fo:" BUILD_FOLDER "sb_gl_loader.obj", "/std:c11", "/O2", "/EHsc", "/nologo");
    nob_cmd_run(&cmd);

    // C helpers
    nob_cmd_append(&cmd, "cl", "/c", SRC_FOLDER"sunburst_draw.c",
        "/Fo:" BUILD_FOLDER "sunburst_draw.obj", "/std:c11", "/O2", "/EHsc", "/nologo");
    nob_cmd_run(&cmd);
    nob_cmd_append(&cmd, "cl", "/c", SRC_FOLDER"sunburst_image.c",
        "/Fo:" BUILD_FOLDER "sunburst_image.obj", "/std:c11", "/O2", "/EHsc", "/nologo");
    nob_cmd_run(&cmd);


    nob_cmd_append(&cmd, "cl", "/c", SRC_FOLDER"sunburst_utils.c",
        "/Fo:" BUILD_FOLDER "sunburst_utils.obj", "/std:c11", "/O2", "/EHsc", "/nologo");
    nob_cmd_run(&cmd);


    #else
    #   error "Unsupported platform"
#endif // _MSC_VER

    // Let's execute the command.
    // if (!nob_cmd_run(&cmd)) return 1;
    // nob_cmd_run() automatically resets the cmd array, so you can nob_cmd_append() more strings
    // into it.

    return 0;
}
