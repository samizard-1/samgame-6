# Project State Review

Date: 2026-04-25

## Architecture Map

This is a small C17 raylib project with a clean top-level split:

- `src/main.c` owns the window, raylib camera integration, menus, runtime game session, drawing, and the per-frame gameplay loop.
- `src/game_core/` owns raylib-free gameplay logic: player vertical motion, world bounds, block construction, procedural generation, collision, support detection, and block-to-surface conversion.
- `tests/` builds one assert-based CTest executable, `samgame_tests`, that links only against `game_core`.
- CMake defines three primary targets: `game_core`, `samgame`, and `samgame_tests`.

The strongest boundary is the raylib-free `game_core` target. Collision, support, and generation are testable without opening a window. The main risk is not boundary leakage today; it is concentration of orchestration in `src/main.c` and concentration of generation policy/mechanics in `src/game_core/world_gen.c`.

## Findings

### 1. Stream-generation failure state is recorded but never acted on

- Severity: `should fix`
- Location: `src/main.c:42`, `src/main.c:43`, `src/main.c:285`, `src/main.c:287`, `src/main.c:400`
- Evidence: `app_state` stores `last_generation_target_level` and `last_generation_success`, and the gameplay loop assigns `state->last_generation_success = world_gen_stream_generate_until_level(...).success;`, but repo-wide references show those fields are only written, never read.
- Why it matters: if stream initialization or extension fails, the game keeps running with whatever partial block set remains. Tests cover the generator's failure reporting path in `tests/test_world_gen.c:792`, but the runtime does not surface or recover from that failure. The unused fields also look like planned diagnostics that never made it into HUD, game-over logic, or assertions.
- Suggested direction: either remove these fields if generation failure is intentionally non-fatal, or turn them into real behavior: pause generation, show an error/debug HUD state, retry with a new seed, or end the run cleanly.
- Confidence: high

### 2. Procedural generation has two similar placement paths that can drift

- Severity: `should fix`
- Location: `src/game_core/world_gen.c:588`, `src/game_core/world_gen.c:680`
- Evidence: `world_gen_stream_append_level` and `world_gen_generate_once` both implement level-by-level placement using previous-level anchors, target counts, `world_gen_place_near_frontier`, capacity checks, and failure rollback/return behavior. They differ in details such as stream retries, path segmentation, and validation.
- Why it matters: the project now supports both fixed-count generation and infinite stream generation. Any future change to jumpability, path guarantees, coverage bias, or placement constraints has to be audited in both flows. The large test suite lowers risk, but the duplicated control flow raises review cost and makes subtle policy drift likely.
- Suggested direction: keep the public APIs, but extract the shared "append one level from previous-level anchors" mechanics into a small internal helper that both fixed and stream generation can call. Preserve the stream-specific retry/window behavior around that helper.
- Confidence: medium

### 3. Collision/support geometry helpers and tolerance constants are duplicated

- Severity: `could fix`
- Location: `src/game_core/world_collision.c:6`, `src/game_core/world_collision.c:9`, `src/game_core/world_support.c:5`, `src/game_core/world_support.c:7`
- Evidence: both collision and support define their own `WORLD_SUPPORT_HEIGHT_EPSILON` value of `0.05f`, and both define local clamp helpers for the same rectangle-overlap style of X/Z footprint checks.
- Why it matters: these modules are intentionally close: support determines floors and collision determines wall/ceiling/body overlap for the same climbable surfaces. If the epsilon or footprint calculation changes in only one module, movement can develop edge-case disagreement between "standing on top" and "blocked by side/ceiling."
- Suggested direction: introduce a small shared internal surface/math helper only if this code changes again. The current duplication is mild and readable, so this is not worth a broad refactor by itself.
- Confidence: high

### 4. `src/main.c` is becoming a runtime god file

- Severity: `could fix`
- Location: `src/main.c:22`, `src/main.c:293`, `src/main.c:409`, `src/main.c:492`, `src/main.c:569`
- Evidence: one file contains screen state, app state, camera setup, menu input, menu drawing, gameplay update, generation streaming, collision integration, HUD drawing, and the process entry point.
- Why it matters: the current file is still understandable, and the repository guidance explicitly assigns raylib setup/rendering/input to `src/main.c`. The risk appears when adding difficulty selection, seed display, settings, debug overlays, or additional player systems; each feature will touch the same file and mix UI state with gameplay orchestration.
- Suggested direction: keep behavior unchanged, but when the next UI/runtime feature lands, split by responsibility rather than preemptively abstracting: for example, menu/session helpers separate from gameplay update/render helpers.
- Confidence: high

### 5. The main test translation unit is doing too many jobs

- Severity: `could fix`
- Location: `tests/test_world_gen.c:1`, `tests/test_world_gen.c:405`, `tests/test_world_gen.c:872`, `tests/test_world_gen.c:1026`, `tests/test_world_gen.c:1215`
- Evidence: `tests/test_world_gen.c` contains generator tests, collision tests, support tests, block tests, shared geometry helpers, and the only `main` for the test executable. `tests/test_player_motion.c` and `tests/test_startup_config.c` are invoked through forward declarations from that file.
- Why it matters: this works and keeps CTest simple, but failures are less localized than the source layout. Collision/support changes require reading a file named for world generation, and adding more test categories will make the single test runner harder to navigate.
- Suggested direction: keep one executable if desired, but move the test runner into a small `tests/test_main.c` and let each module-focused test file expose one `test_*` entry point.
- Confidence: high

## Dead Or Unreachable Code Notes

- No `TODO` or `FIXME` markers were found.
- No orphaned source files were found relative to `CMakeLists.txt`.
- `world_gen_difficulty_params` is not used by the runtime, but it is tested in `tests/test_world_gen.c:552` and appears to be a plausible future menu/runtime feature, so it should not be treated as dead code.
- `last_generation_success` and `last_generation_target_level` are the only confirmed dead state fields found in the app layer.

## Duplicated Logic Notes

- Test-side duplication of generation reachability and overlap checks appears intentional and useful because it acts as an independent oracle for generator behavior.
- Production duplication worth watching is concentrated in generation placement flow and surface footprint/tolerance helpers.

## Verification Performed

- Reviewed `AGENTS.md`, `CMakeLists.txt`, `src/main.c`, `src/game_core/*`, and `tests/*`.
- Searched for TODO/FIXME markers, symbol references, unused-looking state, and repeated helper logic.
- Ran `cmake --build --preset debug`: passed.
- Ran `ctest --test-dir build/debug -C Debug --output-on-failure`: passed, 1/1 tests.

## Review Limits

- This was a static/code-structure review plus existing test/build verification. I did not run the interactive raylib executable or profile runtime generation under real play.
- No code changes were made beyond adding this report.
