#include "raylib.h"

#include "app_config.h"
#include "game_core/world_collision.h"
#include "game_core/player_motion.h"
#include "game_core/world_config.h"
#include "game_core/world_gen.h"
#include "game_core/world_support.h"
#include "game_core/world_surface.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define APP_NO_CEILING_Y 1000000.0f
#define APP_WALL_WINDOW_MARGIN_LEVELS 2

typedef struct app_state {
    Camera camera;
    int camera_mode;
    player_motion_state player_motion;
    player_pose player_pose;
    world_gen_stream_state world_stream;
    world_block blocks[APP_MAX_ACTIVE_BLOCKS];
    world_climbable_surface surfaces[APP_MAX_ACTIVE_BLOCKS];
    world_layout_bounds bounds;
    size_t block_count;
    unsigned int world_seed;
    int highest_reached_level;
    int last_generation_target_level;
    bool last_generation_success;
} app_state;

static Camera CreateStartupCamera(void)
{
    Camera camera = { 0 };
    const float eye_y = player_motion_default_eye_height();

    camera.position = (Vector3){ 0.0f, eye_y, 4.0f };
    camera.target = (Vector3){ 0.0f, eye_y, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    return camera;
}

static unsigned int HashBlockColorKey(world_block block)
{
    unsigned int hash = 2166136261u;
    const int x_key = (int)roundf(block.x * 1000.0f);
    const int z_key = (int)roundf(block.z * 1000.0f);
    const unsigned int values[] = {
        (unsigned int)x_key,
        (unsigned int)z_key,
        (unsigned int)block.level
    };
    size_t index;

    for (index = 0u; index < sizeof(values) / sizeof(values[0]); ++index) {
        hash ^= values[index];
        hash *= 16777619u;
    }

    return hash;
}

static Color GetBlockColor(world_block block)
{
    static const Color palette[] = {
        { 64, 145, 255, 255 },
        { 0, 228, 48, 255 },
        { 253, 249, 0, 255 },
        { 255, 161, 0, 255 },
        { 230, 41, 55, 255 }
    };

    return palette[HashBlockColorKey(block) % (sizeof(palette) / sizeof(palette[0]))];
}

static void DrawGeneratedBlocks(const world_block *blocks, size_t count)
{
    for (size_t index = 0; index < count; ++index)
    {
        const world_block block = blocks[index];
        const float block_height = block.height - block.bottom_y;
        const Vector3 position = { block.x, block.bottom_y + (block_height * 0.5f), block.z };
        const float width = block.half_x * 2.0f;
        const float depth = block.half_z * 2.0f;
        const Color fill = GetBlockColor(block);

        DrawCube(position, width, block_height, depth, fill);
        DrawCubeWires(position, width, block_height, depth, MAROON);
    }
}

static void DrawRoomWindow(const world_layout_bounds *bounds, int minActiveLevel, int highestReachedLevel)
{
    const int wall_bottom_level = minActiveLevel > APP_WALL_WINDOW_MARGIN_LEVELS ?
                                  minActiveLevel - APP_WALL_WINDOW_MARGIN_LEVELS :
                                  0;
    const int wall_top_level = highestReachedLevel + WORLD_BLOCK_GENERATION_AHEAD_LEVELS + APP_WALL_WINDOW_MARGIN_LEVELS;
    const float wall_bottom_y = world_block_height_for_level(wall_bottom_level);
    const float wall_top_y = world_block_height_for_level(wall_top_level);
    const float wall_height = wall_top_y - wall_bottom_y;
    const float wall_center_y = wall_bottom_y + (wall_height * 0.5f);
    const float width = (bounds->max_x - bounds->min_x) + (bounds->block_half_x * 2.0f);
    const float depth = (bounds->max_z - bounds->min_z) + (bounds->block_half_z * 2.0f);

    DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ width, depth }, LIGHTGRAY);
    DrawCube((Vector3){ bounds->min_x - bounds->block_half_x, wall_center_y, 0.0f }, WORLD_WALL_THICKNESS, wall_height, depth, BLUE);
    DrawCube((Vector3){ bounds->max_x + bounds->block_half_x, wall_center_y, 0.0f }, WORLD_WALL_THICKNESS, wall_height, depth, LIME);
    DrawCube((Vector3){ 0.0f, wall_center_y, bounds->min_z - bounds->block_half_z }, width, wall_height, WORLD_WALL_THICKNESS, VIOLET);
    DrawCube((Vector3){ 0.0f, wall_center_y, bounds->max_z + bounds->block_half_z }, width, wall_height, WORLD_WALL_THICKNESS, GOLD);
}

static void DrawHud(const Camera *camera,
                    unsigned int worldSeed,
                    int highestReachedLevel,
                    size_t blockCount,
                    bool generationSucceeded,
                    int generationTargetLevel)
{
    DrawRectangle(8, 8, 340, 125, Fade(SKYBLUE, 0.45f));
    DrawRectangleLines(8, 8, 340, 125, BLUE);
    DrawText("Starter controls:", 18, 18, 10, BLACK);
    DrawText("- Move: W, A, S, D", 18, 35, 10, BLACK);
    DrawText("- Look: mouse or arrow keys", 18, 50, 10, BLACK);
    DrawText("- Jump: Space", 18, 65, 10, BLACK);
    DrawText("- ESC closes the app", 18, 80, 10, BLACK);
    DrawText(TextFormat("- Seed: %u", worldSeed), 18, 95, 10, BLACK);
    DrawText(TextFormat("- Gen: %s to %d", generationSucceeded ? "OK" : "FAILED", generationTargetLevel), 18, 110, 10, BLACK);

    DrawRectangle(565, 8, 228, 125, Fade(SKYBLUE, 0.45f));
    DrawRectangleLines(565, 8, 228, 125, BLUE);
    DrawText("Camera status:", 575, 18, 10, BLACK);
    DrawText("Mode: FIRST_PERSON", 575, 35, 10, BLACK);
    DrawText(TextFormat("Pos: %.2f %.2f %.2f", camera->position.x, camera->position.y, camera->position.z), 575, 50, 10, BLACK);
    DrawText(TextFormat("Target: %.2f %.2f %.2f", camera->target.x, camera->target.y, camera->target.z), 575, 65, 10, BLACK);
    DrawText(TextFormat("Score: %d", highestReachedLevel), 575, 80, 10, BLACK);
    DrawText(TextFormat("Blocks: %zu", blockCount), 575, 95, 10, BLACK);
    DrawText("Projection: PERSPECTIVE", 575, 110, 10, BLACK);
}

static app_state CreateAppState(void)
{
    app_state state = { 0 };
    world_gen_result result;

    state.camera = CreateStartupCamera();
    state.camera_mode = CAMERA_FIRST_PERSON;
    state.player_motion = player_motion_create();
    state.player_pose = player_pose_create(state.camera.position.x, state.camera.position.y, state.camera.position.z);
    state.bounds = world_layout_default_bounds();
    state.world_seed = (unsigned int)time(NULL);
    state.world_stream = world_gen_stream_create(state.world_seed, NULL);
    state.highest_reached_level = 0;
    state.last_generation_target_level = WORLD_BLOCK_MIN_LEVEL + WORLD_BLOCK_GENERATION_AHEAD_LEVELS;

    result = world_gen_stream_initialize(&state.world_stream, APP_MAX_ACTIVE_BLOCKS, state.blocks);
    state.block_count = result.generated_count;
    state.last_generation_success = result.success;
    world_climbable_surfaces_from_blocks(state.blocks, state.block_count, state.surfaces);

    return state;
}

static void UpdateAppState(app_state *state)
{
    const Vector3 previousCameraPosition = state->camera.position;
    const float defaultEyeHeight = player_motion_default_eye_height();
    const float playerRadius = world_collision_player_radius();

    UpdateCamera(&state->camera, state->camera_mode);

    if (IsKeyPressed(KEY_SPACE)) {
        player_motion_request_jump(&state->player_motion);
    }

    if (state->camera.position.x != previousCameraPosition.x || state->camera.position.z != previousCameraPosition.z)
    {
        const world_collision_walls collisionWalls = world_collision_default_walls();
        const float playerFeetY = state->player_pose.eye_y - defaultEyeHeight;
        const float playerTopY = state->player_pose.eye_y;
        float resolvedX = state->camera.position.x;
        float resolvedZ = state->camera.position.z;

        world_collision_resolve_player_blocks_xz(
            &collisionWalls,
            state->surfaces,
            state->block_count,
            playerRadius,
            playerFeetY,
            playerTopY,
            &resolvedX,
            &resolvedZ
        );

        const float correctionX = resolvedX - state->camera.position.x;
        const float correctionZ = resolvedZ - state->camera.position.z;

        state->camera.position.x = resolvedX;
        state->camera.position.z = resolvedZ;
        state->camera.target.x += correctionX;
        state->camera.target.z += correctionZ;
        player_pose_set_xz(&state->player_pose, resolvedX, resolvedZ);
    }

    {
        const float playerFeetY = state->player_pose.eye_y - defaultEyeHeight;
        const float playerTopY = state->player_pose.eye_y;
        const float supportY = world_support_find_floor_y(state->surfaces,
                                                          state->block_count,
                                                          playerRadius,
                                                          playerFeetY,
                                                          state->player_pose.x,
                                                          state->player_pose.z);
        const float supportEyeY = supportY + defaultEyeHeight;
        const float ceilingY = world_collision_find_ceiling_y(state->surfaces,
                                                             state->block_count,
                                                             playerRadius,
                                                             playerFeetY,
                                                             playerTopY,
                                                             state->player_pose.x,
                                                             state->player_pose.z,
                                                             APP_NO_CEILING_Y);

        player_motion_update(&state->player_motion,
                             GetFrameTime(),
                             supportEyeY,
                             ceilingY);
    }

    if (state->camera.position.y != state->player_motion.eye_y) {
        const float correctionY = state->player_motion.eye_y - state->camera.position.y;

        state->camera.position.y = state->player_motion.eye_y;
        state->camera.target.y += correctionY;
        player_pose_set_eye_y(&state->player_pose, state->player_motion.eye_y);
    }

    {
        const float playerFeetY = state->player_pose.eye_y - defaultEyeHeight;
        int playerLevel = (int)floorf(playerFeetY / WORLD_BLOCK_LEVEL_HEIGHT);
        int minimumActiveLevel;
        int targetGeneratedLevel;

        if (playerLevel < 0) {
            playerLevel = 0;
        }

        if (playerLevel > state->highest_reached_level) {
            state->highest_reached_level = playerLevel;
        }

        minimumActiveLevel = state->highest_reached_level - WORLD_BLOCK_ACTIVE_BEHIND_LEVELS;
        targetGeneratedLevel = state->highest_reached_level + WORLD_BLOCK_GENERATION_AHEAD_LEVELS;
        state->last_generation_target_level = targetGeneratedLevel;

        world_gen_stream_prune_below_level(&state->world_stream,
                                           minimumActiveLevel,
                                           state->blocks,
                                           &state->block_count);
        state->last_generation_success = world_gen_stream_generate_until_level(&state->world_stream,
                                                                               targetGeneratedLevel,
                                                                               APP_MAX_ACTIVE_BLOCKS,
                                                                               state->blocks,
                                                                               &state->block_count).success;
        world_climbable_surfaces_from_blocks(state->blocks, state->block_count, state->surfaces);
    }
}

static void DrawAppState(const app_state *state)
{
    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(state->camera);
    DrawRoomWindow(&state->bounds, state->world_stream.min_active_level, state->highest_reached_level);
    DrawGeneratedBlocks(state->blocks, state->block_count);
    EndMode3D();

    DrawHud(&state->camera,
            state->world_seed,
            state->highest_reached_level,
            state->block_count,
            state->last_generation_success,
            state->last_generation_target_level);
    EndDrawing();
}

int main(void)
{
    InitWindow(APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT, "samgame - raylib first person starter");

    app_state state = CreateAppState();

    DisableCursor();
    SetTargetFPS(APP_TARGET_FPS);

    while (!WindowShouldClose())
    {
        UpdateAppState(&state);
        DrawAppState(&state);
    }

    CloseWindow();

    return 0;
}
