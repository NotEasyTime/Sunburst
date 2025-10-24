# Sunburst
Game Engine

## Architecture 
- nob.h for the build system
- GLFW for cross platform windowing and inuput
- clay for the layout engine
- stb_image for image loading and textures
- sunburst for the opengl rendering/ rest of the game engine

## To run

- You will need a static libglfw.a in the build/ folder,
  - this can be sourced from GLFW github relseases page.
  - Sunburst currently uses version 3.4
- To compile the engine and tests you'll need clang on macos or msvc on windows
- nob.c will need to be bootstraped once
- nob.c once compiled will check if its source code has been modified and will recompile itself if it has 
- On windows open a *developer powershell* and run `cl nob.c`
- After that step and for all other compiles just run `./nob.exe tests/bees.c`
- On macos `clang nob.c -o nob`
- Macos `./nob path/to/main.c`

- The compiler outputs in `build/game.exe` or `build/game`
