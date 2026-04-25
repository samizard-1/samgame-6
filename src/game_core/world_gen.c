#include "world_gen.h"

#include "world_config.h"

#include <math.h>

enum {
    WORLD_GEN_LCG_MULTIPLIER = 1664525u,
    WORLD_GEN_LCG_INCREMENT = 1013904223u,
    WORLD_GEN_FLOAT_MASK = 0x00FFFFFFu
};

static const size_t WORLD_GEN_COLLISION_EXTRA_PASSES = 4u;

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

static bool world_gen_resolve_player_walls(const world_collision_walls *walls,
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

static bool world_gen_resolve_player_column_xz(const world_column *column,
                                               float player_radius,
                                               float *x,
                                               float *z)
{
    const float column_min_x = column->x - column->radius;
    const float column_max_x = column->x + column->radius;
    const float column_min_z = column->z - column->radius;
    const float column_max_z = column->z + column->radius;
    const float closest_x = world_gen_clamp(*x, column_min_x, column_max_x);
    const float closest_z = world_gen_clamp(*z, column_min_z, column_max_z);
    const float offset_x = *x - closest_x;
    const float offset_z = *z - closest_z;
    const float distance_squared = (offset_x * offset_x) + (offset_z * offset_z);

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
        float best_distance = *x - column_min_x;
        const float distance_to_max_x = column_max_x - *x;
        const float distance_to_min_z = *z - column_min_z;
        const float distance_to_max_z = column_max_z - *z;

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
                *x = column_min_x - player_radius;
                break;
            case 1:
                *x = column_max_x + player_radius;
                break;
            case 2:
                *z = column_min_z - player_radius;
                break;
            default:
                *z = column_max_z + player_radius;
                break;
        }

        return (*x != original_x) || (*z != original_z);
    }
}

world_gen_bounds world_gen_default_bounds(void)
{
    world_gen_bounds bounds;

    bounds.min_x = WORLD_ROOM_MIN_X;
    bounds.max_x = WORLD_ROOM_MAX_X;
    bounds.min_z = WORLD_ROOM_MIN_Z;
    bounds.max_z = WORLD_ROOM_MAX_Z;
    bounds.min_height = WORLD_COLUMN_MIN_HEIGHT;
    bounds.max_height = WORLD_COLUMN_MAX_HEIGHT;
    bounds.radius = WORLD_COLUMN_RADIUS;

    return bounds;
}

float world_gen_player_collision_radius(void)
{
    return WORLD_PLAYER_COLLISION_RADIUS;
}

float world_gen_default_roof_y(void)
{
    return WORLD_ROOF_UNDERSIDE_Y;
}

world_collision_walls world_gen_default_collision_walls(void)
{
    const world_gen_bounds bounds = world_gen_default_bounds();
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

void world_gen_generate(unsigned int seed, size_t count, world_column *out_columns)
{
    world_gen_bounds bounds;
    unsigned int state;
    size_t index;

    if (count == 0 || out_columns == NULL) {
        return;
    }

    bounds = world_gen_default_bounds();
    state = seed;

    for (index = 0; index < count; ++index) {
        out_columns[index].x = world_gen_scale(world_gen_next(&state), bounds.min_x, bounds.max_x);
        out_columns[index].z = world_gen_scale(world_gen_next(&state), bounds.min_z, bounds.max_z);
        out_columns[index].height = world_gen_scale(world_gen_next(&state), bounds.min_height, bounds.max_height);
        out_columns[index].radius = bounds.radius;
    }
}

void world_gen_resolve_player_xz(const world_collision_walls *walls,
                                 const world_column *columns,
                                 size_t column_count,
                                 float player_radius,
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
        default_walls = world_gen_default_collision_walls();
        active_walls = &default_walls;
    }

    max_passes = column_count + WORLD_GEN_COLLISION_EXTRA_PASSES;

    if (max_passes == 0u) {
        max_passes = 1u;
    }

    for (pass = 0; pass < max_passes; ++pass) {
        bool changed = false;
        size_t column_index;

        changed = world_gen_resolve_player_walls(active_walls, player_radius, x, z) || changed;

        if (columns != NULL) {
            for (column_index = 0; column_index < column_count; ++column_index) {
                changed = world_gen_resolve_player_column_xz(&columns[column_index], player_radius, x, z) || changed;
            }
        }

        if (!changed) {
            break;
        }
    }

    world_gen_resolve_player_walls(active_walls, player_radius, x, z);
}
