cmake_minimum_required(VERSION 3.24)

project(samgame VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)

include(CTest)
include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  raylib
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_MakeAvailable(raylib)

add_library(
  game_core STATIC
  src/game_core/player_motion.c
  src/game_core/world_block.c
  src/game_core/world_collision.c
  src/game_core/world_gen.c
  src/game_core/world_layout.c
  src/game_core/world_support.c
  src/game_core/world_surface.c
)
target_include_directories(
  game_core
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game_core"
)
set_target_properties(game_core PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

add_executable(samgame src/main.c)
target_link_libraries(samgame PRIVATE raylib game_core)
set_target_properties(samgame PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

add_executable(
  samgame_tests
  tests/test_player_motion.c
  tests/test_startup_config.c
  tests/test_world_gen.c
)
target_link_libraries(samgame_tests PRIVATE game_core)
set_target_properties(samgame_tests PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

if(BUILD_TESTING)
  add_test(NAME samgame_tests COMMAND samgame_tests)
endif()
