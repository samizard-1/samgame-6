#include "gameplay_tick.h"

#include "world_collision.h"
#include "world_config.h"
#include "world_support.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

static bool gameplay_has_player_fallen_below_prune_level(float player_feet_y, int minimum_active_level)
{
    if (minimum_active_level <= WORLD_BLOCK_MIN_LEVEL) {
        return false;
    }

    return player_feet_y < world_block_height_for_level(minimum_active_level);
}

static gameplay_tick_result gameplay_tick_make_result(gameplay_tick_status status, int attempted_generation_level)
{
    gameplay_tick_result result;

    result.status = status;
    result.attempted_generation_level = attempted_generation_level;

    return result;
}

gameplay_tick_result gameplay_tick_update(gameplay_tick_context *context)
{
    const float default_eye_height = player_motion_default_eye_height();
    const float player_radius = world_collision_player_radius();
    float player_feet_y;
    float player_top_y;
    float support_y;
    float support_eye_y;
    float ceiling_y;
    int player_level;
    int minimum_active_level;
    int target_generated_level;
    world_gen_result gen_result;

    if (context == NULL ||
        context->motion == NULL ||
        context->pose == NULL ||
        context->stream == NULL ||
        context->blocks == NULL ||
        context->surfaces == NULL ||
        context->block_count == NULL ||
        context->highest_reached_level == NULL) {
        return gameplay_tick_make_result(GAMEPLAY_TICK_GENERATION_FAILED, 0);
    }

    player_feet_y = context->pose->eye_y - default_eye_height;
    player_top_y = context->pose->eye_y;
    support_y = world_support_find_floor_y(context->surfaces,
                                           *context->block_count,
                                           player_radius,
                                           player_feet_y,
                                           context->pose->x,
                                           context->pose->z);
    support_eye_y = support_y + default_eye_height;
    ceiling_y = world_collision_find_ceiling_y(context->surfaces,
                                               *context->block_count,
                                               player_radius,
                                               player_feet_y,
                                               player_top_y,
                                               context->pose->x,
                                               context->pose->z,
                                               context->fallback_ceiling_y);

    player_motion_update(context->motion,
                         context->delta_seconds,
                         support_eye_y,
                         ceiling_y);
    player_pose_set_eye_y(context->pose, context->motion->eye_y);

    player_feet_y = context->pose->eye_y - default_eye_height;
    player_level = (int)floorf(player_feet_y / WORLD_BLOCK_LEVEL_HEIGHT);

    if (player_level < 0) {
        player_level = 0;
    }

    if (player_level > *context->highest_reached_level) {
        *context->highest_reached_level = player_level;
    }

    minimum_active_level = *context->highest_reached_level - WORLD_BLOCK_ACTIVE_BEHIND_LEVELS;
    target_generated_level = *context->highest_reached_level + WORLD_BLOCK_GENERATION_AHEAD_LEVELS;

    if (gameplay_has_player_fallen_below_prune_level(player_feet_y, minimum_active_level)) {
        return gameplay_tick_make_result(GAMEPLAY_TICK_PLAYER_FELL, target_generated_level);
    }

    world_gen_stream_prune_below_level(context->stream,
                                       minimum_active_level,
                                       context->blocks,
                                       context->block_count);
    gen_result = world_gen_stream_generate_until_level(context->stream,
                                                       target_generated_level,
                                                       context->block_capacity,
                                                       context->blocks,
                                                       context->block_count);
    if (!gen_result.success) {
        world_climbable_surfaces_from_blocks(context->blocks, *context->block_count, context->surfaces);
        return gameplay_tick_make_result(GAMEPLAY_TICK_GENERATION_FAILED, target_generated_level);
    }

    world_climbable_surfaces_from_blocks(context->blocks, *context->block_count, context->surfaces);

    return gameplay_tick_make_result(GAMEPLAY_TICK_OK, target_generated_level);
}
