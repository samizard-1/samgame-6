#ifndef WORLD_GEN_H
#define WORLD_GEN_H

#include "world_block.h"
#include "world_layout.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct world_gen_params {
    size_t guaranteed_chain_length;
    float min_jumpable_distance;
    float max_jumpable_distance;
    float nearby_block_chance;
    float upward_step_chance;
    int max_extra_level_delta;
} world_gen_params;

typedef struct world_gen_result {
    size_t generated_count;
    bool success;
} world_gen_result;

world_gen_params world_gen_default_params(void);
world_gen_result world_gen_generate(unsigned int seed, size_t count, world_block *out_blocks);
world_gen_result world_gen_generate_with_params(unsigned int seed,
                                                size_t count,
                                                const world_gen_params *params,
                                                world_block *out_blocks);

#endif
