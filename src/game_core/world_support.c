#include "world_support.h"

#include <stdbool.h>

static const float WORLD_SUPPORT_HEIGHT_EPSILON = 0.05f;

static float world_support_clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

static bool world_support_surface_overlaps_player(const world_climbable_surface *surface,
                                                  float player_radius,
                                                  float x,
                                                  float z)
{
    const float closest_x = world_support_clamp(x, surface->min_x, surface->max_x);
    const float closest_z = world_support_clamp(z, surface->min_z, surface->max_z);
    const float offset_x = x - closest_x;
    const float offset_z = z - closest_z;
    const float distance_squared = (offset_x * offset_x) + (offset_z * offset_z);

    return distance_squared <= (player_radius * player_radius);
}

float world_support_find_floor_y(const world_climbable_surface *surfaces,
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
