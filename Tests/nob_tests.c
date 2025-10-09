#define NOB_IMPLEMENTATION
#include "../nob.h"

// Some folder paths that we use throughout the build process.
#define BUILD_FOLDER "build/"
#define SRC_FOLDER   "src/"
#define TESTS        "tests/"

int main(int argc, char **argv)
{
    // This line enables the self-rebuilding. It detects when nob.c is updated and auto rebuilds it then
    // runs it again.
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    // The working horse of nob is the Nob_Cmd structure. It's a Dynamic Array of strings which represent
    // command line that you want to execute.
    Nob_Cmd cmd = {0};

    // Append the command line arguments
#if defined(__APPLE__)
    // 1) Compile the C example
    nob_cmd_append(&cmd,
        "clang",
        "-c",
        TESTS"gameEx.c",
        "-o", BUILD_FOLDER"gameEx.o",
        "-DGL_SILENCE_DEPRECATION"       // optional: quiets macOS GL deprecation warnings
    );
    nob_cmd_run(&cmd);

    // 2) Link with swiftc so Swift stdlib is handled + frameworks are easy
    nob_cmd_append(&cmd,
        "swiftc",
        BUILD_FOLDER"gameEx.o",
        BUILD_FOLDER"sunburst_draw.o",
        BUILD_FOLDER"sunburst_utils.o",
        BUILD_FOLDER"sunburst.a",
        "-framework", "AppKit",
        "-framework", "OpenGL",
        "-o", BUILD_FOLDER"gameEx",
        // Make sure Swift runtime can be found at runtime (usually /usr/lib/swift on macOS)
        "-Xlinker", "-rpath", "-Xlinker", "@executable_path",
        "-Xlinker", "-rpath", "-Xlinker", "/usr/lib/swift"
    );
    nob_cmd_run(&cmd);
#elif defined(__linux__)
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", BUILD_FOLDER"sunburst", SRC_FOLDER"gameEx.c");
#elif defined(_MSC_VER)
    nob_cmd_append(&cmd, "cl", "/Fe:" BUILD_FOLDER "sunburst.exe", SRC_FOLDER "gameEx.c", "/link", "user32.lib", "gdi32.lib");
    // cl /O2 /std:c11 sunburst_win.c /link user32.lib gdi32.lib

    #else
    #   error "Unsupported platform"
#endif // _MSC_VER

    // Let's execute the command.
    // if (!nob_cmd_run(&cmd)) return 1;
    // nob_cmd_run() automatically resets the cmd array, so you can nob_cmd_append() more strings
    // into it.

    return 0;
}
