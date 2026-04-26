#ifndef GAMEPLAY_TICK_H
#define GAMEPLAY_TICK_H

#include "player_motion.h"
#include "world_block.h"
#include "world_gen.h"
#include "world_surface.h"

#include <stddef.h>

typedef enum gameplay_tick_status {
    GAMEPLAY_TICK_OK,
    GAMEPLAY_TICK_PLAYER_FELL,
    GAMEPLAY_TICK_GENERATION_FAILED
} gameplay_tick_status;

typedef struct gameplay_tick_context {
    player_motion_state *motion;
    player_pose *pose;
    world_gen_stream_state *stream;
    world_block *blocks;
    world_climbable_surface *surfaces;
    size_t *block_count;
    size_t block_capacity;
    int *highest_reached_level;
    float delta_seconds;
    float fallback_ceiling_y;
} gameplay_tick_context;

typedef struct gameplay_tick_result {
    gameplay_tick_status status;
    int attempted_generation_level;
} gameplay_tick_result;

gameplay_tick_result gameplay_tick_update(gameplay_tick_context *context);

#endif
