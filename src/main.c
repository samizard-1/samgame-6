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
#define APP_MENU_BUTTON_WIDTH 300.0f
#define APP_MENU_BUTTON_HEIGHT 44.0f
#define APP_MENU_BUTTON_GAP 12.0f

typedef enum app_screen {
    APP_SCREEN_MAIN_MENU,
    APP_SCREEN_INSTRUCTIONS,
    APP_SCREEN_PLAYING,
    APP_SCREEN_PAUSED,
    APP_SCREEN_GAME_OVER
} app_screen;

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

typedef struct app_session {
    app_screen screen;
    app_state game;
    int selected_menu_item;
    bool exit_requested;
} app_session;

static app_state CreateAppState(void);

static bool IsAnyButtonPressed(void)
{
    return GetKeyPressed() != 0 ||
           IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
           IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
           IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
}

static void SetAppScreen(app_session *session, app_screen screen)
{
    session->screen = screen;
    session->selected_menu_item = 0;

    if (screen == APP_SCREEN_PLAYING) {
        DisableCursor();
    } else {
        EnableCursor();
    }
}

static void StartNewGame(app_session *session)
{
    session->game = CreateAppState();
    SetAppScreen(session, APP_SCREEN_PLAYING);
}

static bool HasPlayerFallenBelowPruneLevel(float playerFeetY, int minimumActiveLevel)
{
    if (minimumActiveLevel <= WORLD_BLOCK_MIN_LEVEL) {
        return false;
    }

    return playerFeetY < world_block_height_for_level(minimumActiveLevel);
}

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

static void DrawHud(int highestReachedLevel)
{
    DrawRectangle(8, 8, 190, 34, Fade(SKYBLUE, 0.45f));
    DrawRectangleLines(8, 8, 190, 34, BLUE);
    DrawText(TextFormat("Max level reached: %d", highestReachedLevel), 18, 20, 10, BLACK);
}

static void DrawCenteredText(const char *text, int y, int fontSize, Color color)
{
    const int textWidth = MeasureText(text, fontSize);
    const int x = (APP_SCREEN_WIDTH - textWidth) / 2;

    DrawText(text, x, y, fontSize, color);
}

static Rectangle GetMenuItemBounds(int index, int startY)
{
    const float x = ((float)APP_SCREEN_WIDTH - APP_MENU_BUTTON_WIDTH) * 0.5f;
    const float y = (float)startY + ((float)index * (APP_MENU_BUTTON_HEIGHT + APP_MENU_BUTTON_GAP));

    return (Rectangle){ x, y, APP_MENU_BUTTON_WIDTH, APP_MENU_BUTTON_HEIGHT };
}

static int WrapMenuSelection(int selection, int itemCount)
{
    if (selection < 0) {
        return itemCount - 1;
    }

    if (selection >= itemCount) {
        return 0;
    }

    return selection;
}

static void UpdateMenuSelection(app_session *session, int itemCount, int startY)
{
    const Vector2 mousePosition = GetMousePosition();
    int index;

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        session->selected_menu_item = WrapMenuSelection(session->selected_menu_item - 1, itemCount);
    }

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        session->selected_menu_item = WrapMenuSelection(session->selected_menu_item + 1, itemCount);
    }

    for (index = 0; index < itemCount; ++index) {
        if (CheckCollisionPointRec(mousePosition, GetMenuItemBounds(index, startY))) {
            session->selected_menu_item = index;
        }
    }
}

static bool IsMenuSelectionActivated(int selectedItem, int startY)
{
    const Rectangle bounds = GetMenuItemBounds(selectedItem, startY);
    const bool mouseActivated = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                                CheckCollisionPointRec(GetMousePosition(), bounds);

    return IsKeyPressed(KEY_ENTER) ||
           IsKeyPressed(KEY_SPACE) ||
           mouseActivated;
}

static void DrawMenuItems(const char **items, int itemCount, int selectedItem, int startY)
{
    int index;

    for (index = 0; index < itemCount; ++index) {
        const Rectangle bounds = GetMenuItemBounds(index, startY);
        const bool selected = index == selectedItem;
        const Color fill = selected ? SKYBLUE : Fade(LIGHTGRAY, 0.9f);
        const Color border = selected ? BLUE : GRAY;
        const int fontSize = 22;
        const int textWidth = MeasureText(items[index], fontSize);
        const int textX = (int)(bounds.x + ((bounds.width - (float)textWidth) * 0.5f));
        const int textY = (int)(bounds.y + ((bounds.height - (float)fontSize) * 0.5f));

        DrawRectangleRec(bounds, fill);
        DrawRectangleLinesEx(bounds, selected ? 3.0f : 1.0f, border);
        DrawText(items[index], textX, textY, fontSize, BLACK);
    }
}

static void DrawGameOver(const app_state *state)
{
    const char *scoreText = TextFormat("Highest level reached: %d", state->highest_reached_level);

    ClearBackground(GRAY);
    DrawCenteredText("YOU LOST", 150, 52, BLACK);
    DrawCenteredText(scoreText, 220, 24, BLACK);
    DrawCenteredText("Press any button to return to main menu", 270, 20, BLACK);
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

static void UpdateGameplay(app_session *session)
{
    app_state *state = &session->game;
    const Vector3 previousCameraPosition = state->camera.position;
    const float defaultEyeHeight = player_motion_default_eye_height();
    const float playerRadius = world_collision_player_radius();

    if (IsKeyPressed(KEY_ESCAPE)) {
        SetAppScreen(session, APP_SCREEN_PAUSED);
        return;
    }

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

        if (HasPlayerFallenBelowPruneLevel(playerFeetY, minimumActiveLevel)) {
            SetAppScreen(session, APP_SCREEN_GAME_OVER);
            return;
        }

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

static void UpdateMainMenu(app_session *session)
{
    const int itemCount = 3;
    const int menuStartY = 210;

    UpdateMenuSelection(session, itemCount, menuStartY);

    if (!IsMenuSelectionActivated(session->selected_menu_item, menuStartY)) {
        return;
    }

    if (session->selected_menu_item == 0) {
        StartNewGame(session);
    } else if (session->selected_menu_item == 1) {
        SetAppScreen(session, APP_SCREEN_INSTRUCTIONS);
    } else {
        session->exit_requested = true;
    }
}

static void UpdateInstructions(app_session *session)
{
    const int itemCount = 1;
    const int menuStartY = 350;

    if (IsKeyPressed(KEY_ESCAPE)) {
        SetAppScreen(session, APP_SCREEN_MAIN_MENU);
        return;
    }

    UpdateMenuSelection(session, itemCount, menuStartY);

    if (IsMenuSelectionActivated(session->selected_menu_item, menuStartY)) {
        SetAppScreen(session, APP_SCREEN_MAIN_MENU);
    }
}

static void UpdatePauseMenu(app_session *session)
{
    const int itemCount = 2;
    const int menuStartY = 230;

    if (IsKeyPressed(KEY_ESCAPE)) {
        SetAppScreen(session, APP_SCREEN_PLAYING);
        return;
    }

    UpdateMenuSelection(session, itemCount, menuStartY);

    if (!IsMenuSelectionActivated(session->selected_menu_item, menuStartY)) {
        return;
    }

    if (session->selected_menu_item == 0) {
        SetAppScreen(session, APP_SCREEN_PLAYING);
    } else {
        SetAppScreen(session, APP_SCREEN_MAIN_MENU);
    }
}

static void UpdateAppSession(app_session *session)
{
    switch (session->screen) {
    case APP_SCREEN_MAIN_MENU:
        UpdateMainMenu(session);
        break;
    case APP_SCREEN_INSTRUCTIONS:
        UpdateInstructions(session);
        break;
    case APP_SCREEN_PLAYING:
        UpdateGameplay(session);
        break;
    case APP_SCREEN_PAUSED:
        UpdatePauseMenu(session);
        break;
    case APP_SCREEN_GAME_OVER:
        if (IsAnyButtonPressed()) {
            SetAppScreen(session, APP_SCREEN_MAIN_MENU);
        }
        break;
    }
}

static void DrawGameplayView(const app_state *state)
{
    ClearBackground(RAYWHITE);

    BeginMode3D(state->camera);
    DrawRoomWindow(&state->bounds, state->world_stream.min_active_level, state->highest_reached_level);
    DrawGeneratedBlocks(state->blocks, state->block_count);
    EndMode3D();

    DrawHud(state->highest_reached_level);
}

static void DrawMainMenu(const app_session *session)
{
    const char *items[] = {
        "Start Game",
        "Instructions",
        "Exit Game"
    };

    ClearBackground(RAYWHITE);
    DrawCenteredText("SAMGAME", 86, 56, BLACK);
    DrawCenteredText("Climb as high as you can", 152, 22, DARKGRAY);
    DrawMenuItems(items, 3, session->selected_menu_item, 210);
}

static void DrawInstructions(const app_session *session)
{
    const char *items[] = { "Back" };

    ClearBackground(RAYWHITE);
    DrawCenteredText("INSTRUCTIONS", 66, 42, BLACK);
    DrawCenteredText("Move: W, A, S, D", 140, 22, DARKGRAY);
    DrawCenteredText("Look: mouse", 178, 22, DARKGRAY);
    DrawCenteredText("Jump: Space", 216, 22, DARKGRAY);
    DrawCenteredText("Pause/Menu: Escape", 254, 22, DARKGRAY);
    DrawMenuItems(items, 1, session->selected_menu_item, 350);
}

static void DrawPauseMenu(const app_session *session)
{
    const char *items[] = {
        "Continue Playing",
        "Return to Main Menu"
    };

    DrawGameplayView(&session->game);
    DrawRectangle(0, 0, APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT, Fade(BLACK, 0.45f));
    DrawCenteredText("PAUSED", 130, 44, RAYWHITE);
    DrawMenuItems(items, 2, session->selected_menu_item, 230);
}

static void DrawAppSession(const app_session *session)
{
    BeginDrawing();

    switch (session->screen) {
    case APP_SCREEN_MAIN_MENU:
        DrawMainMenu(session);
        break;
    case APP_SCREEN_INSTRUCTIONS:
        DrawInstructions(session);
        break;
    case APP_SCREEN_PLAYING:
        DrawGameplayView(&session->game);
        break;
    case APP_SCREEN_PAUSED:
        DrawPauseMenu(session);
        break;
    case APP_SCREEN_GAME_OVER:
        DrawGameOver(&session->game);
        break;
    }

    EndDrawing();
}

int main(void)
{
    InitWindow(APP_SCREEN_WIDTH, APP_SCREEN_HEIGHT, "samgame - raylib first person starter");

    app_session session = { 0 };

    SetTargetFPS(APP_TARGET_FPS);
    SetAppScreen(&session, APP_SCREEN_MAIN_MENU);

    while (!WindowShouldClose() && !session.exit_requested)
    {
        UpdateAppSession(&session);
        DrawAppSession(&session);
    }

    CloseWindow();

    return 0;
}
