#include "world_gen.h"

#include "world_config.h"

#include <math.h>
#include <stdbool.h>

enum {
    WORLD_GEN_LCG_MULTIPLIER = 1664525u,
    WORLD_GEN_LCG_INCREMENT = 1013904223u,
    WORLD_GEN_FLOAT_MASK = 0x00FFFFFFu,
    WORLD_GEN_OFFSET_ATTEMPTS = 96,
    WORLD_GEN_BROAD_ATTEMPTS = 64,
    WORLD_GEN_RETRY_ATTEMPTS = 16,
    WORLD_GEN_STREAM_RETRY_ATTEMPTS = 64,
    WORLD_GEN_COVERAGE_COLS = 4,
    WORLD_GEN_COVERAGE_ROWS = 4,
    WORLD_GEN_LEVEL_SLOTS = WORLD_BLOCK_MAX_LEVEL + 1
};

static const float WORLD_GEN_TWO_PI = 6.28318530718f;

typedef struct world_gen_level_index {
    size_t start;
    size_t count;
} world_gen_level_index;

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

static bool world_gen_overhead_clearance_blocks(float x,
                                                float z,
                                                int level,
                                                float half_x,
                                                float half_z,
                                                const world_block *other,
                                                const world_gen_params *params)
{
    const float margin = params->overhead_clearance_margin;
    const int level_delta = level - other->level;

    if (level_delta <= 0 || level_delta > params->overhead_clearance_levels) {
        return false;
    }

    return world_gen_ranges_overlap(x - half_x - margin, x + half_x + margin,
                                    other->x - other->half_x - margin, other->x + other->half_x + margin) &&
           world_gen_ranges_overlap(z - half_z - margin, z + half_z + margin,
                                    other->z - other->half_z - margin, other->z + other->half_z + margin);
}

static bool world_gen_block_position_is_clear(float x,
                                              float z,
                                              int level,
                                              float bottom_y,
                                              float height,
                                              float half_x,
                                              float half_z,
                                              const world_block *blocks,
                                              size_t block_count,
                                              const world_gen_params *params)
{
    size_t block_index;

    if (blocks == NULL) {
        return true;
    }

    for (block_index = 0u; block_index < block_count; ++block_index) {
        if (world_gen_block_volumes_overlap(x, bottom_y, height, z, half_x, half_z, &blocks[block_index]) ||
            world_gen_overhead_clearance_blocks(x, z, level, half_x, half_z, &blocks[block_index], params)) {
            return false;
        }
    }

    return true;
}

static int world_gen_coverage_cell(float value, float minimum, float maximum, int cell_count)
{
    const float ratio = (value - minimum) / (maximum - minimum);
    int cell = (int)(ratio * (float)cell_count);

    if (cell < 0) {
        cell = 0;
    }

    if (cell >= cell_count) {
        cell = cell_count - 1;
    }

    return cell;
}

static int world_gen_coverage_score(const world_layout_bounds *bounds,
                                    const world_block *blocks,
                                    size_t block_count,
                                    float x,
                                    float z)
{
    const int candidate_col = world_gen_coverage_cell(x, bounds->min_x, bounds->max_x, WORLD_GEN_COVERAGE_COLS);
    const int candidate_row = world_gen_coverage_cell(z, bounds->min_z, bounds->max_z, WORLD_GEN_COVERAGE_ROWS);
    int score = 0;
    size_t index;

    for (index = 0u; index < block_count; ++index) {
        const int block_col = world_gen_coverage_cell(blocks[index].x, bounds->min_x, bounds->max_x, WORLD_GEN_COVERAGE_COLS);
        const int block_row = world_gen_coverage_cell(blocks[index].z, bounds->min_z, bounds->max_z, WORLD_GEN_COVERAGE_ROWS);

        if (block_col == candidate_col && block_row == candidate_row) {
            ++score;
        }
    }

    return score;
}

static world_gen_params world_gen_sanitize_params(const world_gen_params *params)
{
    world_gen_params active = world_gen_default_params();

    if (params != NULL) {
        active = *params;
    }

    if (active.level_one_count == 0u) {
        active.level_one_count = 1u;
    }

    if (active.min_blocks_per_level == 0u) {
        active.min_blocks_per_level = 1u;
    }

    if (active.max_blocks_per_level < active.min_blocks_per_level) {
        active.max_blocks_per_level = active.min_blocks_per_level;
    }

    if (active.min_top_level_paths == 0u) {
        active.min_top_level_paths = 1u;
    }

    if (active.min_jumpable_distance < 0.0f) {
        active.min_jumpable_distance = 0.0f;
    }

    if (active.max_jumpable_distance < active.min_jumpable_distance) {
        active.max_jumpable_distance = active.min_jumpable_distance;
    }

    active.preferred_gap_min = world_gen_clamp(active.preferred_gap_min,
                                               active.min_jumpable_distance,
                                               active.max_jumpable_distance);
    active.preferred_gap_max = world_gen_clamp(active.preferred_gap_max,
                                               active.min_jumpable_distance,
                                               active.max_jumpable_distance);

    if (active.preferred_gap_max < active.preferred_gap_min) {
        active.preferred_gap_max = active.preferred_gap_min;
    }

    active.hard_gap_chance = world_gen_chance(active.hard_gap_chance);
    active.coverage_bias = world_gen_chance(active.coverage_bias);

    if (active.overhead_clearance_levels < 0) {
        active.overhead_clearance_levels = 0;
    }

    if (active.overhead_clearance_margin < 0.0f) {
        active.overhead_clearance_margin = 0.0f;
    }

    return active;
}

static world_gen_params world_gen_stream_default_params(void)
{
    world_gen_params params = world_gen_default_params();

    params.level_one_count = WORLD_BLOCK_STREAM_LEVEL_ONE_COUNT;
    params.min_blocks_per_level = WORLD_BLOCK_STREAM_MIN_BLOCKS_PER_LEVEL;
    params.max_blocks_per_level = WORLD_BLOCK_STREAM_MAX_BLOCKS_PER_LEVEL;
    params.min_top_level_paths = WORLD_BLOCK_STREAM_MIN_TOP_LEVEL_PATHS;
    params.coverage_bias = WORLD_BLOCK_STREAM_COVERAGE_BIAS;

    return params;
}

static float world_gen_gap_distance(unsigned int *state, const world_gen_params *params)
{
    if (world_gen_roll(state, params->hard_gap_chance)) {
        return world_gen_scale(world_gen_next(state), params->min_jumpable_distance, params->max_jumpable_distance);
    }

    return world_gen_scale(world_gen_next(state), params->preferred_gap_min, params->preferred_gap_max);
}

static bool world_gen_place_level_one(unsigned int *state,
                                      const world_layout_bounds *bounds,
                                      const world_gen_params *params,
                                      const world_block *blocks,
                                      size_t block_count,
                                      float *out_x,
                                      float *out_z)
{
    int attempt;
    bool found = false;
    float best_x = 0.0f;
    float best_z = 0.0f;
    int best_score = 0;

    for (attempt = 0; attempt < WORLD_GEN_BROAD_ATTEMPTS; ++attempt) {
        const float candidate_x = world_gen_scale(world_gen_next(state), bounds->min_x, bounds->max_x);
        const float candidate_z = world_gen_scale(world_gen_next(state), bounds->min_z, bounds->max_z);
        const int coverage_score = world_gen_coverage_score(bounds, blocks, block_count, candidate_x, candidate_z);
        const int score = (coverage_score * 100) + (int)(world_gen_next(state) & 63u);

        if (world_gen_block_position_is_clear(candidate_x,
                                              candidate_z,
                                              WORLD_BLOCK_MIN_LEVEL,
                                              world_gen_block_bottom_y_for_level(WORLD_BLOCK_MIN_LEVEL),
                                              world_block_height_for_level(WORLD_BLOCK_MIN_LEVEL),
                                              bounds->block_half_x,
                                              bounds->block_half_z,
                                              blocks,
                                              block_count,
                                              params) &&
            (!found || score < best_score || !world_gen_roll(state, params->coverage_bias))) {
            found = true;
            best_score = score;
            best_x = candidate_x;
            best_z = candidate_z;
        }
    }

    if (found) {
        *out_x = best_x;
        *out_z = best_z;
    }

    return found;
}

static bool world_gen_place_near_frontier(unsigned int *state,
                                          const world_layout_bounds *bounds,
                                          const world_gen_params *params,
                                          const world_block *blocks,
                                          size_t block_count,
                                          const world_block *anchors,
                                          size_t anchor_count,
                                          int level,
                                          float *out_x,
                                          float *out_z)
{
    const float bottom_y = world_gen_block_bottom_y_for_level(level);
    const float height = world_block_height_for_level(level);
    int attempt;
    bool found = false;
    float best_x = 0.0f;
    float best_z = 0.0f;
    int best_score = 0;

    for (attempt = 0; attempt < WORLD_GEN_OFFSET_ATTEMPTS; ++attempt) {
        const world_block *anchor = &anchors[world_gen_index(state, anchor_count)];
        const float angle = world_gen_scale(world_gen_next(state), 0.0f, WORLD_GEN_TWO_PI);
        const float distance = world_gen_gap_distance(state, params);
        const float candidate_x = world_gen_clamp(anchor->x + (cosf(angle) * distance), bounds->min_x, bounds->max_x);
        const float candidate_z = world_gen_clamp(anchor->z + (sinf(angle) * distance), bounds->min_z, bounds->max_z);
        const float actual_distance = world_gen_distance_xz(anchor->x, anchor->z, candidate_x, candidate_z);
        const int coverage_score = world_gen_coverage_score(bounds, blocks, block_count, candidate_x, candidate_z);
        const int score = (coverage_score * 100) + (int)(world_gen_next(state) & 63u);

        if (actual_distance >= params->min_jumpable_distance &&
            actual_distance <= params->max_jumpable_distance &&
            world_gen_block_position_is_clear(candidate_x,
                                              candidate_z,
                                              level,
                                              bottom_y,
                                              height,
                                              bounds->block_half_x,
                                              bounds->block_half_z,
                                              blocks,
                                              block_count,
                                              params) &&
            (!found || score < best_score || !world_gen_roll(state, params->coverage_bias))) {
            found = true;
            best_score = score;
            best_x = candidate_x;
            best_z = candidate_z;
        }
    }

    if (found) {
        *out_x = best_x;
        *out_z = best_z;
    }

    return found;
}

static bool world_gen_has_previous_level_anchor(const world_block *blocks,
                                                size_t block_count,
                                                const world_gen_params *params,
                                                size_t block_index)
{
    size_t anchor_index;
    const world_block *block = &blocks[block_index];

    if (block->level <= WORLD_BLOCK_MIN_LEVEL) {
        return true;
    }

    for (anchor_index = 0u; anchor_index < block_count; ++anchor_index) {
        const world_block *anchor = &blocks[anchor_index];

        if (anchor->level == block->level - 1) {
            const float distance = world_gen_distance_xz(block->x, block->z, anchor->x, anchor->z);

            if (distance >= params->min_jumpable_distance - 0.0001f &&
                distance <= params->max_jumpable_distance + 0.0001f) {
                return true;
            }
        }
    }

    return false;
}

static bool world_gen_blocks_are_jump_connected(const world_block *block,
                                                const world_block *anchor,
                                                const world_gen_params *params)
{
    const float distance = world_gen_distance_xz(block->x, block->z, anchor->x, anchor->z);

    return anchor->level == block->level - 1 &&
           distance >= params->min_jumpable_distance - 0.0001f &&
           distance <= params->max_jumpable_distance + 0.0001f;
}

static bool world_gen_block_traces_to_root(const world_block *blocks,
                                           size_t block_count,
                                           const world_gen_params *params,
                                           size_t block_index,
                                           size_t root_index)
{
    size_t anchor_index;
    const world_block *block = &blocks[block_index];

    if (block_index == root_index) {
        return block->level == WORLD_BLOCK_MIN_LEVEL;
    }

    if (block->level <= WORLD_BLOCK_MIN_LEVEL) {
        return false;
    }

    for (anchor_index = 0u; anchor_index < block_count; ++anchor_index) {
        if (world_gen_blocks_are_jump_connected(block, &blocks[anchor_index], params) &&
            world_gen_block_traces_to_root(blocks, block_count, params, anchor_index, root_index)) {
            return true;
        }
    }

    return false;
}

static size_t world_gen_count_top_level_root_paths(const world_block *blocks,
                                                   size_t block_count,
                                                   const world_gen_params *params)
{
    size_t root_index;
    size_t route_count = 0u;

    for (root_index = 0u; root_index < block_count; ++root_index) {
        size_t top_index;
        bool root_reaches_top = false;

        if (blocks[root_index].level != WORLD_BLOCK_MIN_LEVEL) {
            continue;
        }

        for (top_index = 0u; top_index < block_count && !root_reaches_top; ++top_index) {
            if (blocks[top_index].level == WORLD_BLOCK_MAX_LEVEL &&
                world_gen_block_traces_to_root(blocks, block_count, params, top_index, root_index)) {
                root_reaches_top = true;
            }
        }

        if (root_reaches_top) {
            ++route_count;
        }
    }

    return route_count;
}

static bool world_gen_layout_is_valid(const world_block *blocks,
                                      size_t block_count,
                                      size_t requested_count,
                                      const world_gen_params *params)
{
    size_t index;
    const size_t minimum_top_count = params->level_one_count +
                                     ((size_t)(WORLD_BLOCK_MAX_LEVEL - WORLD_BLOCK_MIN_LEVEL) *
                                      params->min_blocks_per_level);

    for (index = 0u; index < block_count; ++index) {
        if (!world_gen_has_previous_level_anchor(blocks, block_count, params, index)) {
            return false;
        }

    }

    if (requested_count >= minimum_top_count &&
        world_gen_count_top_level_root_paths(blocks, block_count, params) < params->min_top_level_paths) {
        return false;
    }

    return true;
}

static size_t world_gen_target_count_for_level(size_t remaining_count,
                                               int level,
                                               const world_gen_params *params)
{
    const size_t levels_left = (size_t)(WORLD_BLOCK_MAX_LEVEL - level + 1);
    size_t target;

    if (remaining_count == 0u || levels_left == 0u) {
        return 0u;
    }

    if (remaining_count >= levels_left * params->min_blocks_per_level) {
        const size_t future_minimum = (levels_left - 1u) * params->min_blocks_per_level;

        target = remaining_count - future_minimum;
        if (target < params->min_blocks_per_level) {
            target = params->min_blocks_per_level;
        }
    } else if (remaining_count >= levels_left) {
        target = 1u;
    } else {
        target = remaining_count;
    }

    if (target > params->max_blocks_per_level) {
        target = params->max_blocks_per_level;
    }

    return target;
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

static world_gen_level_index world_gen_find_level(const world_block *blocks, size_t block_count, int level)
{
    world_gen_level_index index = { 0u, 0u };
    size_t block_index;

    for (block_index = 0u; block_index < block_count; ++block_index) {
        if (blocks[block_index].level == level) {
            if (index.count == 0u) {
                index.start = block_index;
            }

            ++index.count;
        } else if (index.count > 0u) {
            break;
        }
    }

    return index;
}

static size_t world_gen_stream_target_count_for_level(unsigned int *state, const world_gen_params *params)
{
    const size_t range = params->max_blocks_per_level - params->min_blocks_per_level + 1u;
    size_t target = params->min_blocks_per_level;

    if (range > 1u) {
        target += world_gen_index(state, range);
    }

    if (target < params->min_top_level_paths) {
        target = params->min_top_level_paths;
    }

    return target;
}

static world_gen_result world_gen_stream_append_level(world_gen_stream_state *stream,
                                                      int level,
                                                      size_t capacity,
                                                      world_block *blocks,
                                                      size_t *block_count)
{
    const world_layout_bounds bounds = world_layout_default_bounds();
    const world_gen_level_index previous_level = world_gen_find_level(blocks, *block_count, level - 1);
    world_gen_result last_result = world_gen_failure(*block_count);
    unsigned int attempt_seed = stream->rng_state;
    int attempt;

    if (previous_level.count == 0u) {
        return world_gen_failure(*block_count);
    }

    if (*block_count >= capacity) {
        return world_gen_failure(*block_count);
    }

    for (attempt = 0; attempt < WORLD_GEN_STREAM_RETRY_ATTEMPTS; ++attempt) {
        const size_t start_count = *block_count;
        size_t target;
        size_t placed_this_level = 0u;
        bool failed = false;

        stream->rng_state = attempt_seed;
        target = world_gen_stream_target_count_for_level(&stream->rng_state, &stream->params);

        if (*block_count + target > capacity) {
            target = capacity - *block_count;
        }

        if (target == 0u) {
            return world_gen_failure(*block_count);
        }

        while (placed_this_level < target) {
            const world_block *anchors = &blocks[previous_level.start];
            size_t anchor_count = previous_level.count;
            float x;
            float z;

            if (placed_this_level < stream->params.min_top_level_paths &&
                placed_this_level < previous_level.count) {
                const size_t path_count = stream->params.min_top_level_paths < previous_level.count ?
                                          stream->params.min_top_level_paths :
                                          previous_level.count;
                const size_t segment_start = (placed_this_level * previous_level.count) / path_count;
                const size_t segment_end = ((placed_this_level + 1u) * previous_level.count) / path_count;
                size_t segment_count = segment_end - segment_start;

                if (segment_count == 0u) {
                    segment_count = 1u;
                }

                anchors = &blocks[previous_level.start + segment_start + world_gen_index(&stream->rng_state, segment_count)];
                anchor_count = 1u;
            }

            if (!world_gen_place_near_frontier(&stream->rng_state,
                                               &bounds,
                                               &stream->params,
                                               blocks,
                                               *block_count,
                                               anchors,
                                               anchor_count,
                                               level,
                                               &x,
                                               &z)) {
                failed = true;
                break;
            }

            blocks[*block_count] = world_block_create(x, z, level, bounds.block_half_x, bounds.block_half_z);
            ++*block_count;
            ++placed_this_level;
        }

        if (!failed) {
            stream->max_generated_level = level;
            return world_gen_success(*block_count);
        }

        *block_count = start_count;
        last_result = world_gen_failure(*block_count);
        (void)world_gen_next(&attempt_seed);
    }

    return last_result;
}

static world_gen_result world_gen_generate_once(unsigned int seed,
                                                size_t count,
                                                const world_gen_params *params,
                                                world_block *out_blocks)
{
    const world_layout_bounds bounds = world_layout_default_bounds();
    world_gen_level_index levels[WORLD_GEN_LEVEL_SLOTS] = { { 0u, 0u } };
    unsigned int state = seed;
    size_t generated_count = 0u;
    size_t target;
    int level;

    target = params->level_one_count;
    if (target > count) {
        target = count;
    }

    levels[WORLD_BLOCK_MIN_LEVEL].start = 0u;
    for (; generated_count < target; ++generated_count) {
        float x;
        float z;

        if (!world_gen_place_level_one(&state, &bounds, params, out_blocks, generated_count, &x, &z)) {
            return world_gen_failure(generated_count);
        }

        out_blocks[generated_count] = world_block_create(x,
                                                         z,
                                                         WORLD_BLOCK_MIN_LEVEL,
                                                         bounds.block_half_x,
                                                         bounds.block_half_z);
        ++levels[WORLD_BLOCK_MIN_LEVEL].count;
    }

    for (level = WORLD_BLOCK_MIN_LEVEL + 1; level <= WORLD_BLOCK_MAX_LEVEL && generated_count < count; ++level) {
        size_t placed_this_level = 0u;
        const size_t remaining_count = count - generated_count;
        const world_gen_level_index previous_level = levels[level - 1];

        if (previous_level.count == 0u) {
            return world_gen_failure(generated_count);
        }

        target = world_gen_target_count_for_level(remaining_count, level, params);
        levels[level].start = generated_count;

        while (placed_this_level < target && generated_count < count) {
            const world_block *anchors = &out_blocks[previous_level.start];
            size_t anchor_count = previous_level.count;
            float x;
            float z;

            if (placed_this_level < params->min_top_level_paths &&
                placed_this_level < previous_level.count) {
                anchors = &out_blocks[previous_level.start + placed_this_level];
                anchor_count = 1u;
            }

            if (!world_gen_place_near_frontier(&state,
                                               &bounds,
                                               params,
                                               out_blocks,
                                               generated_count,
                                               anchors,
                                               anchor_count,
                                               level,
                                               &x,
                                               &z)) {
                return world_gen_failure(generated_count);
            }

            out_blocks[generated_count] = world_block_create(x, z, level, bounds.block_half_x, bounds.block_half_z);
            ++generated_count;
            ++placed_this_level;
            ++levels[level].count;
        }
    }

    if (generated_count < count) {
        return world_gen_failure(generated_count);
    }

    if (!world_gen_layout_is_valid(out_blocks, generated_count, count, params)) {
        return world_gen_failure(generated_count);
    }

    return world_gen_success(generated_count);
}

world_gen_params world_gen_default_params(void)
{
    world_gen_params params;

    params.level_one_count = WORLD_BLOCK_DEFAULT_LEVEL_ONE_COUNT;
    params.min_blocks_per_level = WORLD_BLOCK_DEFAULT_MIN_BLOCKS_PER_LEVEL;
    params.max_blocks_per_level = WORLD_BLOCK_DEFAULT_MAX_BLOCKS_PER_LEVEL;
    params.min_top_level_paths = WORLD_BLOCK_DEFAULT_MIN_TOP_LEVEL_PATHS;
    params.min_jumpable_distance = WORLD_BLOCK_DEFAULT_MIN_JUMPABLE_DISTANCE;
    params.max_jumpable_distance = WORLD_BLOCK_DEFAULT_MAX_JUMPABLE_DISTANCE;
    params.preferred_gap_min = WORLD_BLOCK_DEFAULT_PREFERRED_GAP_MIN;
    params.preferred_gap_max = WORLD_BLOCK_DEFAULT_PREFERRED_GAP_MAX;
    params.hard_gap_chance = WORLD_BLOCK_DEFAULT_HARD_GAP_CHANCE;
    params.coverage_bias = WORLD_BLOCK_DEFAULT_COVERAGE_BIAS;
    params.overhead_clearance_levels = WORLD_BLOCK_DEFAULT_OVERHEAD_CLEARANCE_LEVELS;
    params.overhead_clearance_margin = WORLD_BLOCK_DEFAULT_OVERHEAD_CLEARANCE_MARGIN;

    return params;
}

world_gen_params world_gen_difficulty_params(world_gen_difficulty difficulty)
{
    world_gen_params params = world_gen_default_params();

    if (difficulty == WORLD_GEN_DIFFICULTY_EASY) {
        params.level_one_count = 4u;
        params.min_top_level_paths = 2u;
        params.preferred_gap_min = 2.35f;
        params.preferred_gap_max = 3.35f;
        params.hard_gap_chance = 0.10f;
        params.coverage_bias = 0.85f;
    } else if (difficulty == WORLD_GEN_DIFFICULTY_HARD) {
        params.level_one_count = 3u;
        params.min_top_level_paths = 2u;
        params.preferred_gap_min = 3.15f;
        params.preferred_gap_max = 3.95f;
        params.hard_gap_chance = 0.75f;
        params.coverage_bias = 0.55f;
    }

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
    world_gen_params active_params;
    world_gen_result last_result = world_gen_failure(0u);
    unsigned int attempt_seed = seed;
    int attempt;

    if (count == 0u) {
        return world_gen_success(0u);
    }

    if (out_blocks == NULL) {
        return world_gen_failure(0u);
    }

    active_params = world_gen_sanitize_params(params);

    for (attempt = 0; attempt < WORLD_GEN_RETRY_ATTEMPTS; ++attempt) {
        const world_gen_result result = world_gen_generate_once(attempt_seed, count, &active_params, out_blocks);

        if (result.success) {
            return result;
        }

        last_result = result;
        (void)world_gen_next(&attempt_seed);
    }

    return last_result;
}

world_gen_stream_state world_gen_stream_create(unsigned int seed, const world_gen_params *params)
{
    world_gen_stream_state stream;
    world_gen_params default_params = world_gen_stream_default_params();

    stream.seed = seed;
    stream.rng_state = seed;
    stream.params = world_gen_sanitize_params(params != NULL ? params : &default_params);
    stream.min_active_level = WORLD_BLOCK_MIN_LEVEL;
    stream.max_generated_level = WORLD_BLOCK_MIN_LEVEL - 1;
    stream.initialized = false;

    return stream;
}

world_gen_result world_gen_stream_initialize(world_gen_stream_state *stream,
                                             size_t capacity,
                                             world_block *out_blocks)
{
    const world_layout_bounds bounds = world_layout_default_bounds();
    world_gen_result last_result = world_gen_failure(0u);
    unsigned int attempt_seed;
    int attempt;

    if (stream == NULL) {
        return world_gen_failure(0u);
    }

    if (capacity == 0u) {
        return world_gen_failure(0u);
    }

    if (out_blocks == NULL) {
        return world_gen_failure(0u);
    }

    attempt_seed = stream->seed;

    for (attempt = 0; attempt < WORLD_GEN_RETRY_ATTEMPTS; ++attempt) {
        size_t generated_count = 0u;
        size_t target = stream->params.level_one_count;
        size_t index;
        world_gen_result result;

        stream->rng_state = attempt_seed;
        stream->min_active_level = WORLD_BLOCK_MIN_LEVEL;
        stream->max_generated_level = WORLD_BLOCK_MIN_LEVEL - 1;
        stream->initialized = false;

        if (target > capacity) {
            target = capacity;
        }

        for (index = 0u; index < target; ++index) {
            float x;
            float z;

            if (!world_gen_place_level_one(&stream->rng_state,
                                           &bounds,
                                           &stream->params,
                                           out_blocks,
                                           generated_count,
                                           &x,
                                           &z)) {
                result = world_gen_failure(generated_count);
                break;
            }

            out_blocks[generated_count] = world_block_create(x,
                                                             z,
                                                             WORLD_BLOCK_MIN_LEVEL,
                                                             bounds.block_half_x,
                                                             bounds.block_half_z);
            ++generated_count;
        }

        if (generated_count == target) {
            stream->max_generated_level = WORLD_BLOCK_MIN_LEVEL;
            stream->initialized = true;

            result = world_gen_stream_generate_until_level(stream,
                                                           WORLD_BLOCK_MIN_LEVEL + WORLD_BLOCK_GENERATION_AHEAD_LEVELS,
                                                           capacity,
                                                           out_blocks,
                                                           &generated_count);
        }

        if (result.success) {
            return result;
        }

        last_result = result;
        (void)world_gen_next(&attempt_seed);
    }

    stream->initialized = false;

    return last_result;
}

world_gen_result world_gen_stream_generate_until_level(world_gen_stream_state *stream,
                                                       int target_level,
                                                       size_t capacity,
                                                       world_block *blocks,
                                                       size_t *block_count)
{
    if (stream == NULL || blocks == NULL || block_count == NULL) {
        return world_gen_failure(0u);
    }

    if (!stream->initialized) {
        return world_gen_failure(*block_count);
    }

    if (*block_count > capacity) {
        return world_gen_failure(*block_count);
    }

    if (target_level < WORLD_BLOCK_MIN_LEVEL) {
        target_level = WORLD_BLOCK_MIN_LEVEL;
    }

    while (stream->max_generated_level < target_level) {
        const int next_level = stream->max_generated_level + 1;
        const world_gen_result result = world_gen_stream_append_level(stream,
                                                                      next_level,
                                                                      capacity,
                                                                      blocks,
                                                                      block_count);

        if (!result.success) {
            return result;
        }
    }

    return world_gen_success(*block_count);
}

void world_gen_stream_prune_below_level(world_gen_stream_state *stream,
                                        int minimum_level,
                                        world_block *blocks,
                                        size_t *block_count)
{
    size_t read_index;
    size_t write_index = 0u;

    if (stream == NULL || blocks == NULL || block_count == NULL) {
        return;
    }

    if (minimum_level < WORLD_BLOCK_MIN_LEVEL) {
        minimum_level = WORLD_BLOCK_MIN_LEVEL;
    }

    for (read_index = 0u; read_index < *block_count; ++read_index) {
        if (blocks[read_index].level >= minimum_level) {
            if (write_index != read_index) {
                blocks[write_index] = blocks[read_index];
            }

            ++write_index;
        }
    }

    *block_count = write_index;
    stream->min_active_level = minimum_level;

    if (*block_count == 0u) {
        stream->max_generated_level = minimum_level - 1;
        stream->initialized = false;
    }
}
