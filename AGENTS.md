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
- Climbable geometry in `game_core` is currently generated as finite block
  volumes with X/Z footprints and bottom/top Y bounds. Blocks are converted into
  generic surfaces/volumes for support and collision checks.
- `tests/` owns headless regression tests and should link only against
  `game_core`.
- `assets/` is reserved for runtime assets.
- `build/`, `__tmp_probe_build/`, and CMake `_deps/` content are generated
  artifacts. Do not hand-edit them.

## Hard Constraints

- Use C17 only. C extensions are disabled by CMake.
- Do not introduce new dependencies unless the user explicitly asks.
- No backwards compatibility is required for internal interfaces. When changing
  behavior, re-engineer APIs around the new model instead of preserving old
  wrappers, legacy call patterns, or compatibility shims just to keep diffs
  smaller.
- Keep `game_core` free of raylib includes and raylib types so tests remain
  headless.
- raylib is fetched only through CMake `FetchContent`, pinned to release `5.5`.
  Do not add a `find_package(raylib)` fallback without a clear user request.
- Preserve deterministic world generation for a fixed seed.
- Preserve the default map/collision behavior unless the task explicitly changes
  it:
  - world definition constants live in `src/game_core/world_config.h`,
  - default bounds are `[-15, 15]` on X/Z,
  - block top heights are discrete levels using `WORLD_BLOCK_LEVEL_HEIGHT`,
  - default generation guarantees one upward jumpable block chain,
  - block half extents are `WORLD_BLOCK_HALF_X` and `WORLD_BLOCK_HALF_Z`,
  - block vertical thickness is `WORLD_BLOCK_HEIGHT_Y`,
  - player collision radius is `0.5`,
  - all four side walls are blocking,
  - roof underside is high enough above the default max block height for future
    block-top jumping,
  - block collision uses rectangular X/Z footprints plus bottom/top Y bounds.

## Build And Test

Preferred Windows workflow:

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug -C Debug --output-on-failure
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
- When adding new climbable objects, add or convert them into generic climbable
  surfaces instead of teaching movement/support code about each geometry type.
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
