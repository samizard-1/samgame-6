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

static bool world_gen_column_footprints_overlap(float left_x,
                                                float left_z,
                                                float left_radius,
                                                float right_x,
                                                float right_z,
                                                float right_radius)
{
    const float min_clearance = left_radius + right_radius;

    return fabsf(left_x - right_x) < min_clearance && fabsf(left_z - right_z) < min_clearance;
}

static bool world_gen_column_position_is_clear(float x,
                                               float z,
                                               float radius,
                                               const world_column *columns,
                                               size_t column_count)
{
    size_t column_index;

    if (columns == NULL) {
        return true;
    }

    for (column_index = 0u; column_index < column_count; ++column_index) {
        if (world_gen_column_footprints_overlap(x,
                                                z,
                                                radius,
                                                columns[column_index].x,
                                                columns[column_index].z,
                                                columns[column_index].radius)) {
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

    active.nearby_pillar_chance = world_gen_chance(active.nearby_pillar_chance);
    active.upward_step_chance = world_gen_chance(active.upward_step_chance);

    if (active.max_extra_level_delta < 0) {
        active.max_extra_level_delta = 0;
    }

    return active;
}

static bool world_gen_place_near(unsigned int *state,
                                 const world_layout_bounds *bounds,
                                 const world_gen_params *params,
                                 const world_column *columns,
                                 size_t column_count,
                                 float radius,
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
            world_gen_column_position_is_clear(candidate_x, candidate_z, radius, columns, column_count)) {
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

        if (world_gen_column_position_is_clear(candidate_x, candidate_z, radius, columns, column_count)) {
            *out_x = candidate_x;
            *out_z = candidate_z;
            return true;
        }
    }

    return false;
}

static bool world_gen_place_clear_random(unsigned int *state,
                                         const world_layout_bounds *bounds,
                                         const world_column *columns,
                                         size_t column_count,
                                         float radius,
                                         float *out_x,
                                         float *out_z)
{
    int attempt;
    float z;

    for (attempt = 0; attempt < WORLD_GEN_RANDOM_PLACEMENT_ATTEMPTS; ++attempt) {
        const float candidate_x = world_gen_scale(world_gen_next(state), bounds->min_x, bounds->max_x);
        const float candidate_z = world_gen_scale(world_gen_next(state), bounds->min_z, bounds->max_z);

        if (world_gen_column_position_is_clear(candidate_x, candidate_z, radius, columns, column_count)) {
            *out_x = candidate_x;
            *out_z = candidate_z;
            return true;
        }
    }

    for (z = bounds->min_z; z <= bounds->max_z; z += radius * 2.0f) {
        float x;

        for (x = bounds->min_x; x <= bounds->max_x; x += radius * 2.0f) {
            if (world_gen_column_position_is_clear(x, z, radius, columns, column_count)) {
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

    params.guaranteed_chain_length = WORLD_PILLAR_DEFAULT_CHAIN_LENGTH;
    params.min_jumpable_distance = WORLD_PILLAR_DEFAULT_MIN_JUMPABLE_DISTANCE;
    params.max_jumpable_distance = WORLD_PILLAR_DEFAULT_MAX_JUMPABLE_DISTANCE;
    params.nearby_pillar_chance = WORLD_PILLAR_DEFAULT_NEARBY_CHANCE;
    params.upward_step_chance = WORLD_PILLAR_DEFAULT_UPWARD_CHANCE;
    params.max_extra_level_delta = WORLD_PILLAR_DEFAULT_MAX_EXTRA_LEVEL_DELTA;

    return params;
}

world_gen_result world_gen_generate(unsigned int seed, size_t count, world_column *out_columns)
{
    const world_gen_params params = world_gen_default_params();

    return world_gen_generate_with_params(seed, count, &params, out_columns);
}

world_gen_result world_gen_generate_with_params(unsigned int seed,
                                                size_t count,
                                                const world_gen_params *params,
                                                world_column *out_columns)
{
    world_layout_bounds bounds;
    world_gen_params active_params;
    unsigned int state;
    size_t chain_length;
    size_t index;

    if (count == 0u) {
        return world_gen_success(0u);
    }

    if (out_columns == NULL) {
        return world_gen_failure(0u);
    }

    bounds = world_layout_default_bounds();
    active_params = world_gen_sanitize_params(params);
    state = seed;
    chain_length = active_params.guaranteed_chain_length;

    if (chain_length > count) {
        chain_length = count;
    }

    if (chain_length > (size_t)(WORLD_PILLAR_MAX_LEVEL - WORLD_PILLAR_MIN_LEVEL + 1)) {
        chain_length = (size_t)(WORLD_PILLAR_MAX_LEVEL - WORLD_PILLAR_MIN_LEVEL + 1);
    }

    if (chain_length > 0u) {
        const float edge_margin = active_params.max_jumpable_distance;
        const float start_min_x = (bounds.min_x + edge_margin < bounds.max_x) ? bounds.min_x + edge_margin : bounds.min_x;
        const float start_max_x = (bounds.max_x - edge_margin > bounds.min_x) ? bounds.max_x - edge_margin : bounds.max_x;
        const float start_min_z = (bounds.min_z + edge_margin < bounds.max_z) ? bounds.min_z + edge_margin : bounds.min_z;
        const float start_max_z = (bounds.max_z - edge_margin > bounds.min_z) ? bounds.max_z - edge_margin : bounds.max_z;
        float x = world_gen_scale(world_gen_next(&state), start_min_x, start_max_x);
        float z = world_gen_scale(world_gen_next(&state), start_min_z, start_max_z);

        if (!world_gen_column_position_is_clear(x, z, bounds.radius, out_columns, 0u) &&
            !world_gen_place_clear_random(&state, &bounds, out_columns, 0u, bounds.radius, &x, &z)) {
            return world_gen_failure(0u);
        }

        out_columns[0] = world_column_create(x, z, WORLD_PILLAR_MIN_LEVEL, bounds.radius);
    }

    for (index = 1u; index < chain_length; ++index) {
        float x;
        float z;
        const world_column *anchor = &out_columns[index - 1u];
        const int level = anchor->level + 1;

        if (!world_gen_place_near(&state,
                                  &bounds,
                                  &active_params,
                                  out_columns,
                                  index,
                                  bounds.radius,
                                  anchor->x,
                                  anchor->z,
                                  &x,
                                  &z) &&
            !world_gen_place_clear_random(&state, &bounds, out_columns, index, bounds.radius, &x, &z)) {
            return world_gen_failure(index);
        }

        out_columns[index] = world_column_create(x, z, level, bounds.radius);
    }

    for (index = chain_length; index < count; ++index) {
        int level;
        float x;
        float z;

        if (index > 0u && world_gen_roll(&state, active_params.nearby_pillar_chance)) {
            const world_column *anchor = &out_columns[world_gen_index(&state, index)];

            if (!world_gen_place_near(&state,
                                      &bounds,
                                      &active_params,
                                      out_columns,
                                      index,
                                      bounds.radius,
                                      anchor->x,
                                      anchor->z,
                                      &x,
                                      &z) &&
                !world_gen_place_clear_random(&state, &bounds, out_columns, index, bounds.radius, &x, &z)) {
                return world_gen_failure(index);
            }

            if (world_gen_roll(&state, active_params.upward_step_chance)) {
                const int max_delta = active_params.max_extra_level_delta;
                const int delta = (max_delta > 0) ? 1 + (int)world_gen_index(&state, (size_t)max_delta) : 0;

                level = anchor->level + delta;
            } else if (anchor->level > WORLD_PILLAR_MIN_LEVEL) {
                level = WORLD_PILLAR_MIN_LEVEL + (int)world_gen_index(&state,
                                                                      (size_t)(anchor->level - WORLD_PILLAR_MIN_LEVEL + 1));
            } else {
                level = anchor->level;
            }
        } else {
            if (!world_gen_place_clear_random(&state, &bounds, out_columns, index, bounds.radius, &x, &z)) {
                return world_gen_failure(index);
            }

            level = WORLD_PILLAR_MIN_LEVEL + (int)world_gen_index(&state,
                                                                  (size_t)(WORLD_PILLAR_MAX_LEVEL -
                                                                           WORLD_PILLAR_MIN_LEVEL +
                                                                           1));
        }

        out_columns[index] = world_column_create(x, z, level, bounds.radius);
    }

    return world_gen_success(count);
}
