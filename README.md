# samgame

A minimal C17 raylib starter scaffold with a raylib-free `game_core` library, a `samgame` app target, and a `samgame_tests` CTest target.

## Prerequisites

- CMake 3.24+
- A C17-capable compiler
- Visual Studio with the C++ toolchain on Windows (the `debug` preset uses the Visual Studio generator)

## Configure / Build / Test / Run

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug -C Debug --output-on-failure
.\build\debug\Debug\samgame.exe
```

The `debug` preset always configures exactly to `${sourceDir}/build/debug`. With the Visual Studio generator, the built executable ends up at `build/debug/Debug/samgame.exe`.

## Targets

- `game_core` - static library for headless/gameplay code only; it does **not** link raylib
- `samgame` - executable that links `raylib` and `game_core`
- `samgame_tests` - test executable that links `game_core` only and is registered with CTest

## Dependency Management

raylib is acquired with CMake `FetchContent` only and pinned to release `5.5`. There is no `find_package(raylib)` fallback in this scaffold.

## Project Structure

```text
.
|-- CMakeLists.txt
|-- CMakePresets.json
|-- README.md
|-- assets/
|-- src/
|   |-- main.c
|   `-- game_core/
|       |-- player_motion.h
|       |-- player_motion.c
|       |-- startup_config.h
|       |-- world_config.h
|       |-- world_gen.h
|       `-- world_gen.c
`-- tests/
    |-- test_player_motion.c
    |-- test_startup_config.c
    `-- test_world_gen.c
```

## Starter Scope

This starter keeps the official example's first-person camera feel, static scene room/ground, cursor capture, jumping, and generated columns. The playable area is enclosed by four blocking walls and a roof so the player cannot leave the room through the sides or top.

World dimensions live in `src/game_core/world_config.h`. Use that file for room bounds, wall thickness, roof height, roof thickness, column height/radius, and player collision radius so generation, rendering, collision, tests, and documentation stay aligned.
