#include "world_gen.h"

#include "world_config.h"

#include <math.h>

enum {
    WORLD_GEN_LCG_MULTIPLIER = 1664525u,
    WORLD_GEN_LCG_INCREMENT = 1013904223u,
    WORLD_GEN_FLOAT_MASK = 0x00FFFFFFu,
    WORLD_GEN_OFFSET_ATTEMPTS = 64,
    WORLD_GEN_RANDOM_PLACEMENT_ATTEMPTS = 128
};

static const size_t WORLD_GEN_COLLISION_EXTRA_PASSES = 4u;
static const float WORLD_GEN_TWO_PI = 6.28318530718f;
static const float WORLD_SUPPORT_HEIGHT_EPSILON = 0.05f;

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

static int world_gen_clamp_int(int value, int minimum, int maximum)
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

static void world_gen_set_column(world_column *column, float x, float z, int level, float radius)
{
    column->x = x;
    column->z = z;
    column->level = world_gen_clamp_int(level, WORLD_PILLAR_MIN_LEVEL, WORLD_PILLAR_MAX_LEVEL);
    column->height = world_pillar_height_for_level(column->level);
    column->radius = radius;
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

static void world_gen_place_clear_random(unsigned int *state,
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
            return;
        }
    }

    for (z = bounds->min_z; z <= bounds->max_z; z += radius * 2.0f) {
        float x;

        for (x = bounds->min_x; x <= bounds->max_x; x += radius * 2.0f) {
            if (world_gen_column_position_is_clear(x, z, radius, columns, column_count)) {
                *out_x = x;
                *out_z = z;
                return;
            }
        }
    }

    *out_x = bounds->min_x;
    *out_z = bounds->min_z;
}

static bool world_collision_resolve_player_walls(const world_collision_walls *walls,
                                                 float player_radius,
                                                 float *x,
                                                 float *z)
{
    bool changed = false;

    if (walls->block_min_x && (*x - player_radius) < walls->min_x) {
        *x = walls->min_x + player_radius;
        changed = true;
    }

    if (walls->block_max_x && (*x + player_radius) > walls->max_x) {
        *x = walls->max_x - player_radius;
        changed = true;
    }

    if (walls->block_min_z && (*z - player_radius) < walls->min_z) {
        *z = walls->min_z + player_radius;
        changed = true;
    }

    if (walls->block_max_z && (*z + player_radius) > walls->max_z) {
        *z = walls->max_z - player_radius;
        changed = true;
    }

    return changed;
}

static bool world_collision_resolve_player_surface_xz(const world_climbable_surface *surface,
                                                      float player_radius,
                                                      float player_feet_y,
                                                      float *x,
                                                      float *z)
{
    const float closest_x = world_gen_clamp(*x, surface->min_x, surface->max_x);
    const float closest_z = world_gen_clamp(*z, surface->min_z, surface->max_z);
    const float offset_x = *x - closest_x;
    const float offset_z = *z - closest_z;
    const float distance_squared = (offset_x * offset_x) + (offset_z * offset_z);

    if (player_feet_y + WORLD_SUPPORT_HEIGHT_EPSILON >= surface->top_y) {
        return false;
    }

    if (distance_squared > 0.0f) {
        const float distance = sqrtf(distance_squared);
        const float overlap = player_radius - distance;

        if (overlap <= 0.0f) {
            return false;
        }

        *x += (offset_x / distance) * overlap;
        *z += (offset_z / distance) * overlap;

        return true;
    }

    {
        const float original_x = *x;
        const float original_z = *z;
        int face = 0;
        float best_distance = *x - surface->min_x;
        const float distance_to_max_x = surface->max_x - *x;
        const float distance_to_min_z = *z - surface->min_z;
        const float distance_to_max_z = surface->max_z - *z;

        if (distance_to_max_x < best_distance) {
            best_distance = distance_to_max_x;
            face = 1;
        }

        if (distance_to_min_z < best_distance) {
            best_distance = distance_to_min_z;
            face = 2;
        }

        if (distance_to_max_z < best_distance) {
            face = 3;
        }

        switch (face) {
            case 0:
                *x = surface->min_x - player_radius;
                break;
            case 1:
                *x = surface->max_x + player_radius;
                break;
            case 2:
                *z = surface->min_z - player_radius;
                break;
            default:
                *z = surface->max_z + player_radius;
                break;
        }

        return (*x != original_x) || (*z != original_z);
    }
}

static bool world_support_surface_overlaps_player(const world_climbable_surface *surface,
                                                  float player_radius,
                                                  float x,
                                                  float z)
{
    const float closest_x = world_gen_clamp(x, surface->min_x, surface->max_x);
    const float closest_z = world_gen_clamp(z, surface->min_z, surface->max_z);
    const float offset_x = x - closest_x;
    const float offset_z = z - closest_z;
    const float distance_squared = (offset_x * offset_x) + (offset_z * offset_z);

    return distance_squared <= (player_radius * player_radius);
}

world_layout_bounds world_layout_default_bounds(void)
{
    world_layout_bounds bounds;

    bounds.min_x = WORLD_ROOM_MIN_X;
    bounds.max_x = WORLD_ROOM_MAX_X;
    bounds.min_z = WORLD_ROOM_MIN_Z;
    bounds.max_z = WORLD_ROOM_MAX_Z;
    bounds.min_height = WORLD_COLUMN_MIN_HEIGHT;
    bounds.max_height = WORLD_COLUMN_MAX_HEIGHT;
    bounds.radius = WORLD_COLUMN_RADIUS;

    return bounds;
}

float world_collision_player_radius(void)
{
    return WORLD_PLAYER_COLLISION_RADIUS;
}

float world_layout_default_roof_y(void)
{
    return WORLD_ROOF_UNDERSIDE_Y;
}

world_collision_walls world_collision_default_walls(void)
{
    const world_layout_bounds bounds = world_layout_default_bounds();
    const float half_wall_thickness = WORLD_WALL_THICKNESS * 0.5f;
    world_collision_walls walls;

    walls.min_x = bounds.min_x - bounds.radius + half_wall_thickness;
    walls.max_x = bounds.max_x + bounds.radius - half_wall_thickness;
    walls.min_z = bounds.min_z - bounds.radius + half_wall_thickness;
    walls.max_z = bounds.max_z + bounds.radius - half_wall_thickness;
    walls.block_min_x = true;
    walls.block_max_x = true;
    walls.block_min_z = true;
    walls.block_max_z = true;

    return walls;
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

float world_pillar_height_for_level(int level)
{
    const int clamped_level = world_gen_clamp_int(level, WORLD_PILLAR_MIN_LEVEL, WORLD_PILLAR_MAX_LEVEL);

    return (float)clamped_level * WORLD_PILLAR_LEVEL_HEIGHT;
}

void world_gen_generate(unsigned int seed, size_t count, world_column *out_columns)
{
    const world_gen_params params = world_gen_default_params();

    world_gen_generate_with_params(seed, count, &params, out_columns);
}

void world_gen_generate_with_params(unsigned int seed,
                                    size_t count,
                                    const world_gen_params *params,
                                    world_column *out_columns)
{
    world_layout_bounds bounds;
    world_gen_params active_params;
    unsigned int state;
    size_t chain_length;
    size_t index;

    if (count == 0 || out_columns == NULL) {
        return;
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

        if (!world_gen_column_position_is_clear(x, z, bounds.radius, out_columns, 0u)) {
            world_gen_place_clear_random(&state, &bounds, out_columns, 0u, bounds.radius, &x, &z);
        }

        world_gen_set_column(&out_columns[0], x, z, WORLD_PILLAR_MIN_LEVEL, bounds.radius);
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
                                  &z)) {
            world_gen_place_clear_random(&state, &bounds, out_columns, index, bounds.radius, &x, &z);
        }

        world_gen_set_column(&out_columns[index], x, z, level, bounds.radius);
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
                                      &z)) {
                world_gen_place_clear_random(&state, &bounds, out_columns, index, bounds.radius, &x, &z);
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
            world_gen_place_clear_random(&state, &bounds, out_columns, index, bounds.radius, &x, &z);
            level = WORLD_PILLAR_MIN_LEVEL + (int)world_gen_index(&state,
                                                                  (size_t)(WORLD_PILLAR_MAX_LEVEL -
                                                                           WORLD_PILLAR_MIN_LEVEL +
                                                                           1));
        }

        world_gen_set_column(&out_columns[index], x, z, level, bounds.radius);
    }
}

world_climbable_surface world_climbable_surface_from_column(const world_column *column,
                                                            size_t column_index)
{
    world_climbable_surface surface;

    if (column == NULL) {
        surface.min_x = 0.0f;
        surface.max_x = 0.0f;
        surface.min_z = 0.0f;
        surface.max_z = 0.0f;
        surface.top_y = 0.0f;
        surface.source = WORLD_CLIMBABLE_SURFACE_SOURCE_UNKNOWN;
        surface.source_index = column_index;
        return surface;
    }

    surface.min_x = column->x - column->radius;
    surface.max_x = column->x + column->radius;
    surface.min_z = column->z - column->radius;
    surface.max_z = column->z + column->radius;
    surface.top_y = column->height;
    surface.source = WORLD_CLIMBABLE_SURFACE_SOURCE_COLUMN;
    surface.source_index = column_index;

    return surface;
}

void world_climbable_surfaces_from_columns(const world_column *columns,
                                           size_t column_count,
                                           world_climbable_surface *out_surfaces)
{
    size_t column_index;

    if (columns == NULL || out_surfaces == NULL) {
        return;
    }

    for (column_index = 0u; column_index < column_count; ++column_index) {
        out_surfaces[column_index] = world_climbable_surface_from_column(&columns[column_index], column_index);
    }
}

void world_collision_resolve_player_surfaces_xz(const world_collision_walls *walls,
                                                const world_climbable_surface *surfaces,
                                                size_t surface_count,
                                                float player_radius,
                                                float player_feet_y,
                                                float *x,
                                                float *z)
{
    world_collision_walls default_walls;
    const world_collision_walls *active_walls = walls;
    size_t pass;
    size_t max_passes;

    if (x == NULL || z == NULL) {
        return;
    }

    if (player_radius < 0.0f) {
        player_radius = 0.0f;
    }

    if (active_walls == NULL) {
        default_walls = world_collision_default_walls();
        active_walls = &default_walls;
    }

    max_passes = surface_count + WORLD_GEN_COLLISION_EXTRA_PASSES;

    if (max_passes == 0u) {
        max_passes = 1u;
    }

    for (pass = 0; pass < max_passes; ++pass) {
        bool changed = false;
        size_t surface_index;

        changed = world_collision_resolve_player_walls(active_walls, player_radius, x, z) || changed;

        if (surfaces != NULL) {
            for (surface_index = 0u; surface_index < surface_count; ++surface_index) {
                changed = world_collision_resolve_player_surface_xz(&surfaces[surface_index],
                                                                    player_radius,
                                                                    player_feet_y,
                                                                    x,
                                                                    z) || changed;
            }
        }

        if (!changed) {
            break;
        }
    }

    world_collision_resolve_player_walls(active_walls, player_radius, x, z);
}

void world_collision_resolve_player_xz(const world_collision_walls *walls,
                                       const world_column *columns,
                                       size_t column_count,
                                       float player_radius,
                                       float player_feet_y,
                                       float *x,
                                       float *z)
{
    world_collision_walls default_walls;
    const world_collision_walls *active_walls = walls;
    size_t pass;
    size_t max_passes;

    if (x == NULL || z == NULL) {
        return;
    }

    if (player_radius < 0.0f) {
        player_radius = 0.0f;
    }

    if (active_walls == NULL) {
        default_walls = world_collision_default_walls();
        active_walls = &default_walls;
    }

    max_passes = column_count + WORLD_GEN_COLLISION_EXTRA_PASSES;

    if (max_passes == 0u) {
        max_passes = 1u;
    }

    for (pass = 0; pass < max_passes; ++pass) {
        bool changed = false;
        size_t column_index;

        changed = world_collision_resolve_player_walls(active_walls, player_radius, x, z) || changed;

        if (columns != NULL) {
            for (column_index = 0; column_index < column_count; ++column_index) {
                const world_climbable_surface surface =
                    world_climbable_surface_from_column(&columns[column_index], column_index);

                changed = world_collision_resolve_player_surface_xz(&surface,
                                                                    player_radius,
                                                                    player_feet_y,
                                                                    x,
                                                                    z) || changed;
            }
        }

        if (!changed) {
            break;
        }
    }

    world_collision_resolve_player_walls(active_walls, player_radius, x, z);
}

float world_support_find_floor_y_for_surfaces(const world_climbable_surface *surfaces,
                                              size_t surface_count,
                                              float player_radius,
                                              float player_feet_y,
                                              float x,
                                              float z)
{
    float floor_y = 0.0f;
    size_t surface_index;

    if (player_radius < 0.0f) {
        player_radius = 0.0f;
    }

    if (surfaces == NULL) {
        return floor_y;
    }

    for (surface_index = 0u; surface_index < surface_count; ++surface_index) {
        const world_climbable_surface *surface = &surfaces[surface_index];

        if (surface->top_y > player_feet_y + WORLD_SUPPORT_HEIGHT_EPSILON) {
            continue;
        }

        if (surface->top_y <= floor_y) {
            continue;
        }

        if (world_support_surface_overlaps_player(surface, player_radius, x, z)) {
            floor_y = surface->top_y;
        }
    }

    return floor_y;
}

float world_support_find_floor_y(const world_column *columns,
                                 size_t column_count,
                                 float player_radius,
                                 float player_feet_y,
                                 float x,
                                 float z)
{
    float floor_y = 0.0f;
    size_t column_index;

    if (player_radius < 0.0f) {
        player_radius = 0.0f;
    }

    if (columns == NULL) {
        return floor_y;
    }

    for (column_index = 0; column_index < column_count; ++column_index) {
        const world_climbable_surface surface =
            world_climbable_surface_from_column(&columns[column_index], column_index);

        if (surface.top_y <= player_feet_y + WORLD_SUPPORT_HEIGHT_EPSILON &&
            surface.top_y > floor_y &&
            world_support_surface_overlaps_player(&surface, player_radius, x, z)) {
            floor_y = surface.top_y;
        }
    }

    return floor_y;
}
