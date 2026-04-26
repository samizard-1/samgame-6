#include "world_support.h"

#include "world_surface_geometry.h"

#include <stdbool.h>

static bool world_support_surface_overlaps_player(const world_climbable_surface *surface,
                                                  float player_radius,
                                                  float x,
                                                  float z)
{
    return world_surface_overlaps_circle_xz(surface, player_radius, x, z);
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

        if (surface->top_y > player_feet_y + WORLD_SURFACE_HEIGHT_EPSILON) {
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
