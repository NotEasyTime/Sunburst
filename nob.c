#define NOB_IMPLEMENTATION
#include "nob.h"

// Some folder paths that we use throughout the build process.
#define BUILD_FOLDER "build/"
#define SRC_FOLDER   "./"
#define IMPLNTS      "Implements/"
#define TESTS        "Tests/"

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
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", BUILD_FOLDER"sunburst", TESTS"gameEx.c", IMPLNTS"b_Cocoa.m", "-framework", "Cocoa");
#elif defined(__linux__)
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", BUILD_FOLDER"sunburst", SRC_FOLDER"gameEx.c");
#elif defined(_MSC_VER)
    //nob_cmd_append(&cmd, "cl", "/Fe:" BUILD_FOLDER "sunburst.exe", SRC_FOLDER "gameEx.c");
    #else
    #   error "Unsupported platform"
#endif // _MSC_VER

    // Let's execute the command.
    if (!nob_cmd_run(&cmd)) return 1;
    // nob_cmd_run() automatically resets the cmd array, so you can nob_cmd_append() more strings
    // into it.

    return 0;
}
