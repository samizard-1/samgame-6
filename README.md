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
|   |-- app_config.h
|   |-- main.c
|   `-- game_core/
|       |-- player_motion.h
|       |-- player_motion.c
|       |-- world_block.h
|       |-- world_block.c
|       |-- world_collision.h
|       |-- world_collision.c
|       |-- world_config.h
|       |-- world_gen.h
|       |-- world_gen.c
|       |-- world_layout.h
|       |-- world_layout.c
|       |-- world_support.h
|       |-- world_support.c
|       |-- world_surface.h
|       `-- world_surface.c
`-- tests/
    |-- test_player_motion.c
    |-- test_startup_config.c
    `-- test_world_gen.c
```

## World Geometry

`game_core` generates finite block volumes with rectangular X/Z footprints and
bottom/top Y bounds. Blocks are converted into generic surfaces before runtime
support and collision checks, so future climbable objects can join the same path
without making player movement depend on a specific geometry type.

Blocks use discrete top-height levels instead of arbitrary continuous heights. A
level is `WORLD_BLOCK_LEVEL_HEIGHT` units tall, with level 1 reachable from the
floor by the current jump and level 2 intentionally unreachable from the floor.
The default generator creates one upward jumpable block chain, then fills the
remaining blocks using difficulty parameters that tune nearby placement,
upward-step likelihood, and max extra level delta. Runtime collision treats
blocks as volumes: sides block the player when vertical spans overlap, tops can
support the player, and undersides act as ceilings during jumps.

## Starter Scope

This starter keeps the official example's first-person camera feel, static scene room/ground, cursor capture, jumping, and generated blocks. The playable area is enclosed by four blocking side walls, while vertical climb is intentionally unbounded.

World dimensions live in `src/game_core/world_config.h`. Use that file for room
bounds, wall thickness, block level defaults, block extents, and player
collision radius so generation, rendering, collision, tests, and documentation
stay aligned.

App startup constants such as screen size, target FPS, and startup block count
live in `src/app_config.h` so executable policy stays out of the raylib-free
core.
