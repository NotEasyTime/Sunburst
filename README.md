# Sunburst
Game Engine

## To run

To compile the engine and tests you'll need clang on macos or msvc on windows
nob.c will need to be bootstraped once
nob.c once compiled will check if its source code has been modified and will recompile itself if it has 
On windows open a *developer powershell* and run `cl nob.c`
After that step and for all other compiles just run `./nob.exe tests/bees.c`
On macos `clang nob.c -o nob`
Macos `./nob path/to/main.c`

The compiler outputs in `build/game.exe` or `build/game`

You will need to provide your own bee.png at the product root
