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
ctest --test-dir build/debug --output-on-failure
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
|       |-- startup_config.h
|       |-- world_gen.h
|       `-- world_gen.c
`-- tests/
    |-- test_startup_config.c
    `-- test_world_gen.c
```

## Starter Scope

This starter keeps the official example's first-person camera feel, static scene walls/ground, cursor capture, and generated columns. It intentionally omits the original example's extra camera-mode hotkeys and projection-toggle flow so the initial project stays small and easy to extend.

## VS Code

This repo includes a ready-to-use `.vscode/` setup that calls `cmake`/`ctest` directly.

Keyboard-friendly workflow:

- `Ctrl+Shift+B` → configure + build
- `F5` → build + run with debugger
- `Ctrl+F5` → build + run without debugger

There is also a default test task:

- `Terminal: Run Test Task` → configure + build + run `ctest`

Clean rebuild:

- `Tasks: Run Task` → `Clean Rebuild (debug)`

These tasks rely on `cmake` and `ctest` being on `PATH`. On Windows, the preset uses the Visual Studio generator so normal VS Code launches and `F5` work without needing a separate Developer shell.
