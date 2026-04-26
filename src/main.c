#include "raylib.h"

#include "app_config.h"
#include "game_core/world_collision.h"
#include "game_core/player_motion.h"
#include "game_core/world_config.h"
#include "game_core/world_gen.h"
#include "game_core/world_support.h"
#include "game_core/world_surface.h"

#include <stddef.h>
#include <time.h>

typedef struct app_state {
    Camera camera;
    int camera_mode;
    player_motion_state player_motion;
    player_pose player_pose;
    world_block blocks[APP_DEFAULT_BLOCK_COUNT];
    world_climbable_surface surfaces[APP_DEFAULT_BLOCK_COUNT];
    world_layout_bounds bounds;
    size_t block_count;
    unsigned int world_seed;
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

static Color GetBlockColor(size_t index)
{
    static const Color palette[] = {
        { 64, 145, 255, 255 },
        { 0, 228, 48, 255 },
        { 253, 249, 0, 255 },
        { 255, 161, 0, 255 },
        { 230, 41, 55, 255 }
    };

    return palette[index % (sizeof(palette) / sizeof(palette[0]))];
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
        const Color fill = GetBlockColor(index);

        DrawCube(position, width, block_height, depth, fill);
        DrawCubeWires(position, width, block_height, depth, MAROON);
    }
}

static void DrawRoom(const world_layout_bounds *bounds)
{
    const float wall_height = world_layout_default_roof_y();
    const float wall_center_y = wall_height * 0.5f;
    const float width = (bounds->max_x - bounds->min_x) + (bounds->block_half_x * 2.0f);
    const float depth = (bounds->max_z - bounds->min_z) + (bounds->block_half_z * 2.0f);
    const float roof_center_y = wall_height + (WORLD_ROOF_THICKNESS * 0.5f);

    DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ width, depth }, LIGHTGRAY);
    DrawCube((Vector3){ bounds->min_x - bounds->block_half_x, wall_center_y, 0.0f }, WORLD_WALL_THICKNESS, wall_height, depth, BLUE);
    DrawCube((Vector3){ bounds->max_x + bounds->block_half_x, wall_center_y, 0.0f }, WORLD_WALL_THICKNESS, wall_height, depth, LIME);
    DrawCube((Vector3){ 0.0f, wall_center_y, bounds->min_z - bounds->block_half_z }, width, wall_height, WORLD_WALL_THICKNESS, VIOLET);
    DrawCube((Vector3){ 0.0f, wall_center_y, bounds->max_z + bounds->block_half_z }, width, wall_height, WORLD_WALL_THICKNESS, GOLD);
    DrawCube((Vector3){ 0.0f, roof_center_y, 0.0f }, width, WORLD_ROOF_THICKNESS, depth, Fade(SKYBLUE, 0.35f));
}

static void DrawHud(const Camera *camera, unsigned int worldSeed)
{
    DrawRectangle(8, 8, 340, 110, Fade(SKYBLUE, 0.45f));
    DrawRectangleLines(8, 8, 340, 110, BLUE);
    DrawText("Starter controls:", 18, 18, 10, BLACK);
    DrawText("- Move: W, A, S, D", 18, 35, 10, BLACK);
    DrawText("- Look: mouse or arrow keys", 18, 50, 10, BLACK);
    DrawText("- Jump: Space", 18, 65, 10, BLACK);
    DrawText("- ESC closes the app", 18, 80, 10, BLACK);
    DrawText(TextFormat("- Seed: %u", worldSeed), 18, 95, 10, BLACK);

    DrawRectangle(565, 8, 228, 95, Fade(SKYBLUE, 0.45f));
    DrawRectangleLines(565, 8, 228, 95, BLUE);
    DrawText("Camera status:", 575, 18, 10, BLACK);
    DrawText("Mode: FIRST_PERSON", 575, 35, 10, BLACK);
    DrawText(TextFormat("Pos: %.2f %.2f %.2f", camera->position.x, camera->position.y, camera->position.z), 575, 50, 10, BLACK);
    DrawText(TextFormat("Target: %.2f %.2f %.2f", camera->target.x, camera->target.y, camera->target.z), 575, 65, 10, BLACK);
    DrawText("Projection: PERSPECTIVE", 575, 80, 10, BLACK);
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

    result = world_gen_generate(state.world_seed, APP_DEFAULT_BLOCK_COUNT, state.blocks);
    state.block_count = result.generated_count;
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
                                                             world_layout_default_roof_y());

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
}

static void DrawAppState(const app_state *state)
{
    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(state->camera);
    DrawRoom(&state->bounds);
    DrawGeneratedBlocks(state->blocks, state->block_count);
    EndMode3D();

    DrawHud(&state->camera, state->world_seed);
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
