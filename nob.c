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

    
    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    // A Dynamic Array of strings which represent the command line that you want to execute.
    Nob_Cmd cmd = {0};

    // Append the command line arguments per platform 
#if defined(__APPLE__)
    nob_cmd_append(&cmd,
        "clang", "-c",
        SRC_FOLDER"sunburst_input.c",
        "-o", BUILD_FOLDER"sunburst_input.o",
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
    // --- Compile Swift source into object ---
    nob_cmd_append(&cmd,
        "swiftc",
        "-emit-object",
        "-parse-as-library",
        "-Ounchecked",
        "-sdk", "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk",
        "-framework", "AppKit",
        "-framework", "OpenGL",
        "-import-objc-header", "src/bridge.h",
        "-Xcc", "-I", "-Xcc", "src/",   // <<< FIXED: was 'scr/'
        "-o", "build/sunburst_swift.o",
        "implements/b_AppKit.swift"
    );
    nob_cmd_run(&cmd);

    // --- Archive to static lib ---
    nob_cmd_append(&cmd,
        "libtool",
        "-static",
        "-o", BUILD_FOLDER"sunburst.a",
        BUILD_FOLDER"sunburst_swift.o",   // <- corrected name
        BUILD_FOLDER"sunburst_input.o",
        BUILD_FOLDER"sunburst_draw.o",
        BUILD_FOLDER"sunburst_image.o",
        BUILD_FOLDER"sunburst_utils.o"
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


    if(argc > 1){
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
    } else {
        printf("No target provided\nOnly compiled engine\n");
    }

    return 0;
}
