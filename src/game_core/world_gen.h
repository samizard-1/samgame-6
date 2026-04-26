#ifndef WORLD_GEN_H
#define WORLD_GEN_H

#include "world_block.h"
#include "world_layout.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum world_gen_difficulty {
    WORLD_GEN_DIFFICULTY_EASY,
    WORLD_GEN_DIFFICULTY_NORMAL,
    WORLD_GEN_DIFFICULTY_HARD
} world_gen_difficulty;

typedef struct world_gen_params {
    size_t level_one_count;
    size_t min_blocks_per_level;
    size_t max_blocks_per_level;
    size_t min_top_level_paths;
    float min_jumpable_distance;
    float max_jumpable_distance;
    float preferred_gap_min;
    float preferred_gap_max;
    float hard_gap_chance;
    float coverage_bias;
    int overhead_clearance_levels;
    float overhead_clearance_margin;
} world_gen_params;

typedef struct world_gen_result {
    size_t generated_count;
    bool success;
} world_gen_result;

typedef struct world_gen_stream_state {
    unsigned int seed;
    unsigned int rng_state;
    world_gen_params params;
    int min_active_level;
    int max_generated_level;
    bool initialized;
} world_gen_stream_state;

world_gen_params world_gen_default_params(void);
world_gen_params world_gen_difficulty_params(world_gen_difficulty difficulty);
world_gen_result world_gen_generate(unsigned int seed, size_t count, world_block *out_blocks);
world_gen_result world_gen_generate_with_params(unsigned int seed,
                                                size_t count,
                                                const world_gen_params *params,
                                                world_block *out_blocks);
world_gen_stream_state world_gen_stream_create(unsigned int seed, const world_gen_params *params);
world_gen_result world_gen_stream_initialize(world_gen_stream_state *stream,
                                             size_t capacity,
                                             world_block *out_blocks);
world_gen_result world_gen_stream_generate_until_level(world_gen_stream_state *stream,
                                                       int target_level,
                                                       size_t capacity,
                                                       world_block *blocks,
                                                       size_t *block_count);
void world_gen_stream_prune_below_level(world_gen_stream_state *stream,
                                        int minimum_level,
                                        world_block *blocks,
                                        size_t *block_count);

#endif
