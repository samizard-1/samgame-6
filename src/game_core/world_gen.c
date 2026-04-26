#include "world_gen.h"

#include "world_config.h"

#include <math.h>
#include <stdbool.h>

enum {
    WORLD_GEN_LCG_MULTIPLIER = 1664525u,
    WORLD_GEN_LCG_INCREMENT = 1013904223u,
    WORLD_GEN_FLOAT_MASK = 0x00FFFFFFu,
    WORLD_GEN_OFFSET_ATTEMPTS = 64,
    WORLD_GEN_RANDOM_PLACEMENT_ATTEMPTS = 128
};

static const float WORLD_GEN_TWO_PI = 6.28318530718f;

static unsigned int world_gen_next(unsigned int *state)
{
    *state = (*state * WORLD_GEN_LCG_MULTIPLIER) + WORLD_GEN_LCG_INCREMENT;
    return *state;
}

static float world_gen_scale(unsigned int value, float minimum, float maximum)
{
    const float ratio = (float)(value & WORLD_GEN_FLOAT_MASK) / (float)WORLD_GEN_FLOAT_MASK;
    return minimum + ((maximum - minimum) * ratio);
}

static float world_gen_clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

static float world_gen_chance(float chance)
{
    return world_gen_clamp(chance, 0.0f, 1.0f);
}

static bool world_gen_roll(unsigned int *state, float chance)
{
    return world_gen_scale(world_gen_next(state), 0.0f, 1.0f) <= world_gen_chance(chance);
}

static size_t world_gen_index(unsigned int *state, size_t count)
{
    if (count == 0u) {
        return 0u;
    }

    return (size_t)(world_gen_next(state) % count);
}

static float world_gen_distance_xz(float left_x, float left_z, float right_x, float right_z)
{
    const float delta_x = left_x - right_x;
    const float delta_z = left_z - right_z;

    return sqrtf((delta_x * delta_x) + (delta_z * delta_z));
}

static bool world_gen_ranges_overlap(float left_min, float left_max, float right_min, float right_max)
{
    return left_min < right_max && right_min < left_max;
}

static float world_gen_block_bottom_y_for_level(int level)
{
    const float top_y = world_block_height_for_level(level);

    return fmaxf(0.0f, top_y - WORLD_BLOCK_HEIGHT_Y);
}

static bool world_gen_block_volumes_overlap(float left_x,
                                            float left_bottom_y,
                                            float left_height,
                                            float left_z,
                                            float left_half_x,
                                            float left_half_z,
                                            const world_block *right)
{
    if (right == NULL) {
        return false;
    }

    return world_gen_ranges_overlap(left_x - left_half_x, left_x + left_half_x,
                                    right->x - right->half_x, right->x + right->half_x) &&
           world_gen_ranges_overlap(left_bottom_y, left_height, right->bottom_y, right->height) &&
           world_gen_ranges_overlap(left_z - left_half_z, left_z + left_half_z,
                                    right->z - right->half_z, right->z + right->half_z);
}

static bool world_gen_block_position_is_clear(float x,
                                              float z,
                                              float bottom_y,
                                              float height,
                                              float half_x,
                                              float half_z,
                                              const world_block *blocks,
                                              size_t block_count)
{
    size_t block_index;

    if (blocks == NULL) {
        return true;
    }

    for (block_index = 0u; block_index < block_count; ++block_index) {
        if (world_gen_block_volumes_overlap(x, bottom_y, height, z, half_x, half_z, &blocks[block_index])) {
            return false;
        }
    }

    return true;
}

static world_gen_params world_gen_sanitize_params(const world_gen_params *params)
{
    world_gen_params active = world_gen_default_params();

    if (params != NULL) {
        active = *params;
    }

    if (active.min_jumpable_distance < 0.0f) {
        active.min_jumpable_distance = 0.0f;
    }

    if (active.max_jumpable_distance < active.min_jumpable_distance) {
        active.max_jumpable_distance = active.min_jumpable_distance;
    }

    active.nearby_block_chance = world_gen_chance(active.nearby_block_chance);
    active.upward_step_chance = world_gen_chance(active.upward_step_chance);

    if (active.max_extra_level_delta < 0) {
        active.max_extra_level_delta = 0;
    }

    return active;
}

static bool world_gen_place_near(unsigned int *state,
                                 const world_layout_bounds *bounds,
                                 const world_gen_params *params,
                                 const world_block *blocks,
                                 size_t block_count,
                                 float bottom_y,
                                 float height,
                                 float half_x,
                                 float half_z,
                                 float anchor_x,
                                 float anchor_z,
                                 float *out_x,
                                 float *out_z)
{
    int attempt;

    for (attempt = 0; attempt < WORLD_GEN_OFFSET_ATTEMPTS; ++attempt) {
        const float angle = world_gen_scale(world_gen_next(state), 0.0f, WORLD_GEN_TWO_PI);
        const float distance = world_gen_scale(world_gen_next(state),
                                               params->min_jumpable_distance,
                                               params->max_jumpable_distance);
        const float candidate_x = world_gen_clamp(anchor_x + (cosf(angle) * distance),
                                                  bounds->min_x,
                                                  bounds->max_x);
        const float candidate_z = world_gen_clamp(anchor_z + (sinf(angle) * distance),
                                                  bounds->min_z,
                                                  bounds->max_z);
        const float actual_distance = world_gen_distance_xz(anchor_x, anchor_z, candidate_x, candidate_z);

        if (actual_distance >= params->min_jumpable_distance &&
            actual_distance <= params->max_jumpable_distance &&
            world_gen_block_position_is_clear(candidate_x, candidate_z, bottom_y, height, half_x, half_z, blocks, block_count)) {
            *out_x = candidate_x;
            *out_z = candidate_z;
            return true;
        }
    }

    {
        const float center_x = (bounds->min_x + bounds->max_x) * 0.5f;
        const float center_z = (bounds->min_z + bounds->max_z) * 0.5f;
        const float direction_x = center_x - anchor_x;
        const float direction_z = center_z - anchor_z;
        const float length = sqrtf((direction_x * direction_x) + (direction_z * direction_z));
        const float distance = params->min_jumpable_distance;

        float candidate_x = anchor_x;
        float candidate_z = world_gen_clamp(anchor_z + distance, bounds->min_z, bounds->max_z);

        if (length > 0.0f) {
            candidate_x = world_gen_clamp(anchor_x + ((direction_x / length) * distance),
                                          bounds->min_x,
                                          bounds->max_x);
            candidate_z = world_gen_clamp(anchor_z + ((direction_z / length) * distance),
                                          bounds->min_z,
                                          bounds->max_z);
        }

        if (world_gen_block_position_is_clear(candidate_x, candidate_z, bottom_y, height, half_x, half_z, blocks, block_count)) {
            *out_x = candidate_x;
            *out_z = candidate_z;
            return true;
        }
    }

    return false;
}

static bool world_gen_place_clear_random(unsigned int *state,
                                         const world_layout_bounds *bounds,
                                         const world_block *blocks,
                                         size_t block_count,
                                         float bottom_y,
                                         float height,
                                         float half_x,
                                         float half_z,
                                         float *out_x,
                                         float *out_z)
{
    int attempt;
    float z;

    for (attempt = 0; attempt < WORLD_GEN_RANDOM_PLACEMENT_ATTEMPTS; ++attempt) {
        const float candidate_x = world_gen_scale(world_gen_next(state), bounds->min_x, bounds->max_x);
        const float candidate_z = world_gen_scale(world_gen_next(state), bounds->min_z, bounds->max_z);

        if (world_gen_block_position_is_clear(candidate_x, candidate_z, bottom_y, height, half_x, half_z, blocks, block_count)) {
            *out_x = candidate_x;
            *out_z = candidate_z;
            return true;
        }
    }

    for (z = bounds->min_z; z <= bounds->max_z; z += half_z * 2.0f) {
        float x;

        for (x = bounds->min_x; x <= bounds->max_x; x += half_x * 2.0f) {
            if (world_gen_block_position_is_clear(x, z, bottom_y, height, half_x, half_z, blocks, block_count)) {
                *out_x = x;
                *out_z = z;
                return true;
            }
        }
    }

    return false;
}

static world_gen_result world_gen_success(size_t generated_count)
{
    world_gen_result result;

    result.generated_count = generated_count;
    result.success = true;

    return result;
}

static world_gen_result world_gen_failure(size_t generated_count)
{
    world_gen_result result;

    result.generated_count = generated_count;
    result.success = false;

    return result;
}

world_gen_params world_gen_default_params(void)
{
    world_gen_params params;

    params.guaranteed_chain_length = WORLD_BLOCK_DEFAULT_CHAIN_LENGTH;
    params.min_jumpable_distance = WORLD_BLOCK_DEFAULT_MIN_JUMPABLE_DISTANCE;
    params.max_jumpable_distance = WORLD_BLOCK_DEFAULT_MAX_JUMPABLE_DISTANCE;
    params.nearby_block_chance = WORLD_BLOCK_DEFAULT_NEARBY_CHANCE;
    params.upward_step_chance = WORLD_BLOCK_DEFAULT_UPWARD_CHANCE;
    params.max_extra_level_delta = WORLD_BLOCK_DEFAULT_MAX_EXTRA_LEVEL_DELTA;

    return params;
}

world_gen_result world_gen_generate(unsigned int seed, size_t count, world_block *out_blocks)
{
    const world_gen_params params = world_gen_default_params();

    return world_gen_generate_with_params(seed, count, &params, out_blocks);
}

world_gen_result world_gen_generate_with_params(unsigned int seed,
                                                size_t count,
                                                const world_gen_params *params,
                                                world_block *out_blocks)
{
    world_layout_bounds bounds;
    world_gen_params active_params;
    unsigned int state;
    size_t chain_length;
    size_t index;

    if (count == 0u) {
        return world_gen_success(0u);
    }

    if (out_blocks == NULL) {
        return world_gen_failure(0u);
    }

    bounds = world_layout_default_bounds();
    active_params = world_gen_sanitize_params(params);
    state = seed;
    chain_length = active_params.guaranteed_chain_length;

    if (chain_length > count) {
        chain_length = count;
    }

    if (chain_length > (size_t)(WORLD_BLOCK_MAX_LEVEL - WORLD_BLOCK_MIN_LEVEL + 1)) {
        chain_length = (size_t)(WORLD_BLOCK_MAX_LEVEL - WORLD_BLOCK_MIN_LEVEL + 1);
    }

    if (chain_length > 0u) {
        const float edge_margin = active_params.max_jumpable_distance;
        const float start_min_x = (bounds.min_x + edge_margin < bounds.max_x) ? bounds.min_x + edge_margin : bounds.min_x;
        const float start_max_x = (bounds.max_x - edge_margin > bounds.min_x) ? bounds.max_x - edge_margin : bounds.max_x;
        const float start_min_z = (bounds.min_z + edge_margin < bounds.max_z) ? bounds.min_z + edge_margin : bounds.min_z;
        const float start_max_z = (bounds.max_z - edge_margin > bounds.min_z) ? bounds.max_z - edge_margin : bounds.max_z;
        float x = world_gen_scale(world_gen_next(&state), start_min_x, start_max_x);
        float z = world_gen_scale(world_gen_next(&state), start_min_z, start_max_z);

        if (!world_gen_block_position_is_clear(x,
                                               z,
                                               world_gen_block_bottom_y_for_level(WORLD_BLOCK_MIN_LEVEL),
                                               world_block_height_for_level(WORLD_BLOCK_MIN_LEVEL),
                                               bounds.block_half_x,
                                               bounds.block_half_z,
                                               out_blocks,
                                               0u) &&
            !world_gen_place_clear_random(&state,
                                          &bounds,
                                          out_blocks,
                                          0u,
                                          world_gen_block_bottom_y_for_level(WORLD_BLOCK_MIN_LEVEL),
                                          world_block_height_for_level(WORLD_BLOCK_MIN_LEVEL),
                                          bounds.block_half_x,
                                          bounds.block_half_z,
                                          &x,
                                          &z)) {
            return world_gen_failure(0u);
        }

        out_blocks[0] = world_block_create(x, z, WORLD_BLOCK_MIN_LEVEL, bounds.block_half_x, bounds.block_half_z);
    }

    for (index = 1u; index < chain_length; ++index) {
        float x;
        float z;
        const world_block *anchor = &out_blocks[index - 1u];
        const int level = anchor->level + 1;

        if (!world_gen_place_near(&state,
                                  &bounds,
                                  &active_params,
                                  out_blocks,
                                  index,
                                  world_gen_block_bottom_y_for_level(level),
                                  world_block_height_for_level(level),
                                  bounds.block_half_x,
                                  bounds.block_half_z,
                                  anchor->x,
                                  anchor->z,
                                  &x,
                                  &z) &&
            !world_gen_place_clear_random(&state,
                                          &bounds,
                                          out_blocks,
                                          index,
                                          world_gen_block_bottom_y_for_level(level),
                                          world_block_height_for_level(level),
                                          bounds.block_half_x,
                                          bounds.block_half_z,
                                          &x,
                                          &z)) {
            return world_gen_failure(index);
        }

        out_blocks[index] = world_block_create(x, z, level, bounds.block_half_x, bounds.block_half_z);
    }

    for (index = chain_length; index < count; ++index) {
        int level;
        float x;
        float z;

        if (index > 0u && world_gen_roll(&state, active_params.nearby_block_chance)) {
            const world_block *anchor = &out_blocks[world_gen_index(&state, index)];

            if (world_gen_roll(&state, active_params.upward_step_chance)) {
                const int max_delta = active_params.max_extra_level_delta;
                const int delta = (max_delta > 0) ? 1 + (int)world_gen_index(&state, (size_t)max_delta) : 0;

                level = anchor->level + delta;
            } else if (anchor->level > WORLD_BLOCK_MIN_LEVEL) {
                level = WORLD_BLOCK_MIN_LEVEL + (int)world_gen_index(&state,
                                                                      (size_t)(anchor->level - WORLD_BLOCK_MIN_LEVEL + 1));
            } else {
                level = anchor->level;
            }

            if (!world_gen_place_near(&state,
                                      &bounds,
                                      &active_params,
                                      out_blocks,
                                      index,
                                      world_gen_block_bottom_y_for_level(level),
                                      world_block_height_for_level(level),
                                      bounds.block_half_x,
                                      bounds.block_half_z,
                                      anchor->x,
                                      anchor->z,
                                      &x,
                                      &z) &&
                !world_gen_place_clear_random(&state,
                                              &bounds,
                                              out_blocks,
                                              index,
                                              world_gen_block_bottom_y_for_level(level),
                                              world_block_height_for_level(level),
                                              bounds.block_half_x,
                                              bounds.block_half_z,
                                              &x,
                                              &z)) {
                return world_gen_failure(index);
            }
        } else {
            level = WORLD_BLOCK_MIN_LEVEL + (int)world_gen_index(&state,
                                                                  (size_t)(WORLD_BLOCK_MAX_LEVEL -
                                                                           WORLD_BLOCK_MIN_LEVEL +
                                                                           1));

            if (!world_gen_place_clear_random(&state,
                                              &bounds,
                                              out_blocks,
                                              index,
                                              world_gen_block_bottom_y_for_level(level),
                                              world_block_height_for_level(level),
                                              bounds.block_half_x,
                                              bounds.block_half_z,
                                              &x,
                                              &z)) {
                return world_gen_failure(index);
            }
        }

        out_blocks[index] = world_block_create(x, z, level, bounds.block_half_x, bounds.block_half_z);
    }

    return world_gen_success(count);
}
