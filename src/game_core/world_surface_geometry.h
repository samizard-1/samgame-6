#ifndef WORLD_SURFACE_GEOMETRY_H
#define WORLD_SURFACE_GEOMETRY_H

#include "world_surface.h"

#include <stdbool.h>

#define WORLD_SURFACE_HEIGHT_EPSILON 0.05f

static inline float world_surface_clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

static inline bool world_surface_overlaps_circle_xz(const world_climbable_surface *surface,
                                                    float radius,
                                                    float x,
                                                    float z)
{
    const float closest_x = world_surface_clamp(x, surface->min_x, surface->max_x);
    const float closest_z = world_surface_clamp(z, surface->min_z, surface->max_z);
    const float offset_x = x - closest_x;
    const float offset_z = z - closest_z;
    const float distance_squared = (offset_x * offset_x) + (offset_z * offset_z);

    return distance_squared <= (radius * radius);
}

#endif
