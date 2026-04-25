# AGENTS.md

Guidance for future agentic work in this repository. This file applies to the
entire project unless a deeper `AGENTS.md` overrides it.

## Project Snapshot

- `samgame` is a minimal C17 raylib starter.
- The repo is intentionally small: keep changes narrow, readable, and easy to
  reverse.
- The project has three main CMake targets:
  - `game_core`: headless gameplay/static library code.
  - `samgame`: raylib executable and rendering/input integration.
  - `samgame_tests`: assert-based CTest executable for headless tests.

## Architecture

- `src/main.c` owns raylib setup, rendering, input/camera integration, and HUD
  drawing.
- `src/game_core/` owns raylib-free gameplay logic and configuration.
- `tests/` owns headless regression tests and should link only against
  `game_core`.
- `assets/` is reserved for runtime assets.
- `build/`, `__tmp_probe_build/`, and CMake `_deps/` content are generated
  artifacts. Do not hand-edit them.

## Hard Constraints

- Use C17 only. C extensions are disabled by CMake.
- Do not introduce new dependencies unless the user explicitly asks.
- Keep `game_core` free of raylib includes and raylib types so tests remain
  headless.
- raylib is fetched only through CMake `FetchContent`, pinned to release `5.5`.
  Do not add a `find_package(raylib)` fallback without a clear user request.
- Preserve deterministic world generation for a fixed seed.
- Preserve the default map/collision behavior unless the task explicitly changes
  it:
  - world definition constants live in `src/game_core/world_config.h`,
  - default bounds are `[-15, 15]` on X/Z,
  - column radius is `1.0`,
  - player collision radius is `0.5`,
  - all four side walls are blocking,
  - roof underside is high enough above the default max column height for future
    pillar-top jumping,
  - column collision uses square footprints, matching rendered cubes.

## Build And Test

Preferred Windows workflow:

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

Run the app after building:

```powershell
.\build\debug\Debug\samgame.exe
```

The `debug` preset writes to `build/debug` and uses the Visual Studio generator.
If a local machine lacks that generator, do not rewrite the project just to make
configuration pass; report the environment mismatch.

## Coding Guidelines

- Prefer simple C functions, local structs, and explicit data flow over broad
  abstractions.
- Keep gameplay logic in `game_core` when it can be tested without a window.
- Add or update tests when changing deterministic generation, startup constants,
  collision rules, or other gameplay behavior.
- Use `assert`-style tests consistent with the existing test files.
- Keep public headers small and stable; avoid exposing implementation helpers.
- Use `size_t` for counts and indices.
- Use `float` math for gameplay coordinates to match raylib integration.
- Keep comments scarce and useful; name things clearly instead.

## Agent Workflow

- Inspect the existing code before editing. The repo is small enough that broad
  guesses are unnecessary.
- Keep diffs focused. Avoid unrelated formatting churn.
- Do not modify generated build outputs or vendored/fetched raylib sources.
- Before claiming completion for code changes, run the CMake build and CTest
  command above when the local toolchain permits it.
- If tests cannot run because Visual Studio/CMake/toolchain pieces are missing,
  say exactly which command failed and why.
