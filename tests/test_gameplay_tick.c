#include <assert.h>
#include <stddef.h>

#include "../src/app_config.h"
#include "../src/game_core/gameplay_tick.h"
#include "../src/game_core/world_config.h"

static void test_tick_reports_generation_failure_when_stream_capacity_is_exhausted(void)
{
    player_motion_state motion = player_motion_create();
    player_pose pose;
    world_gen_stream_state stream = world_gen_stream_create(97531u, NULL);
    world_block blocks[APP_MAX_ACTIVE_BLOCKS];
    world_climbable_surface surfaces[APP_MAX_ACTIVE_BLOCKS];
    size_t block_count;
    int highest_reached_level;
    gameplay_tick_context context;
    gameplay_tick_result tick_result;
    const world_gen_result init_result = world_gen_stream_initialize(&stream, APP_MAX_ACTIVE_BLOCKS, blocks);

    assert(init_result.success);
    block_count = init_result.generated_count;
    highest_reached_level = stream.max_generated_level;
    motion.eye_y = world_block_height_for_level(highest_reached_level) + player_motion_default_eye_height();
    motion.velocity_y = 0.0f;
    motion.grounded = true;
    pose = player_pose_create(0.0f, motion.eye_y, 0.0f);
    world_climbable_surfaces_from_blocks(blocks, block_count, surfaces);

    context.motion = &motion;
    context.pose = &pose;
    context.stream = &stream;
    context.blocks = blocks;
    context.surfaces = surfaces;
    context.block_count = &block_count;
    context.block_capacity = block_count;
    context.highest_reached_level = &highest_reached_level;
    context.delta_seconds = 1.0f / 60.0f;
    context.fallback_ceiling_y = 1000000.0f;

    tick_result = gameplay_tick_update(&context);

    assert(tick_result.status == GAMEPLAY_TICK_GENERATION_FAILED);
    assert(tick_result.attempted_generation_level ==
           highest_reached_level + WORLD_BLOCK_GENERATION_AHEAD_LEVELS);
}

void test_gameplay_tick(void)
{
    test_tick_reports_generation_failure_when_stream_capacity_is_exhausted();
}
