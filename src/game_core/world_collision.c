#include "world_collision.h"

#include <math.h>
#include <stdbool.h>

static const size_t WORLD_COLLISION_EXTRA_PASSES = 4u;
static const float WORLD_SUPPORT_HEIGHT_EPSILON = 0.05f;

static float world_collision_clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
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
                                                      float player_top_y,
                                                      float *x,
                                                      float *z)
{
    const float closest_x = world_collision_clamp(*x, surface->min_x, surface->max_x);
    const float closest_z = world_collision_clamp(*z, surface->min_z, surface->max_z);
    const float offset_x = *x - closest_x;
    const float offset_z = *z - closest_z;
    const float distance_squared = (offset_x * offset_x) + (offset_z * offset_z);

    if (player_feet_y + WORLD_SUPPORT_HEIGHT_EPSILON >= surface->top_y) {
        return false;
    }

    if (player_top_y <= surface->bottom_y + WORLD_SUPPORT_HEIGHT_EPSILON) {
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

void world_collision_resolve_player_blocks_xz(const world_collision_walls *walls,
                                              const world_climbable_surface *surfaces,
                                              size_t surface_count,
                                              float player_radius,
                                              float player_feet_y,
                                              float player_top_y,
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

    max_passes = surface_count + WORLD_COLLISION_EXTRA_PASSES;

    if (max_passes == 0u) {
        max_passes = 1u;
    }

    for (pass = 0u; pass < max_passes; ++pass) {
        bool changed = false;
        size_t surface_index;

        changed = world_collision_resolve_player_walls(active_walls, player_radius, x, z) || changed;

        if (surfaces != NULL) {
            for (surface_index = 0u; surface_index < surface_count; ++surface_index) {
                changed = world_collision_resolve_player_surface_xz(&surfaces[surface_index],
                                                                    player_radius,
                                                                    player_feet_y,
                                                                    player_top_y,
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

float world_collision_find_ceiling_y(const world_climbable_surface *surfaces,
                                     size_t surface_count,
                                     float player_radius,
                                     float player_feet_y,
                                     float player_top_y,
                                     float x,
                                     float z,
                                     float fallback_ceiling_y)
{
    float ceiling_y = fallback_ceiling_y;
    size_t surface_index;

    if (player_radius < 0.0f) {
        player_radius = 0.0f;
    }

    if (surfaces == NULL) {
        return ceiling_y;
    }

    for (surface_index = 0u; surface_index < surface_count; ++surface_index) {
        const world_climbable_surface *surface = &surfaces[surface_index];
        const float closest_x = world_collision_clamp(x, surface->min_x, surface->max_x);
        const float closest_z = world_collision_clamp(z, surface->min_z, surface->max_z);
        const float offset_x = x - closest_x;
        const float offset_z = z - closest_z;
        const float distance_squared = (offset_x * offset_x) + (offset_z * offset_z);

        if (surface->bottom_y <= player_feet_y + WORLD_SUPPORT_HEIGHT_EPSILON) {
            continue;
        }

        const float block_ceiling_y = surface->bottom_y - WORLD_SUPPORT_HEIGHT_EPSILON;

        if (block_ceiling_y <= player_feet_y) {
            continue;
        }

        if (block_ceiling_y > ceiling_y) {
            continue;
        }

        if (distance_squared <= (player_radius * player_radius)) {
            ceiling_y = block_ceiling_y;
        }
    }

    return ceiling_y;
}
