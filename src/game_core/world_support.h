#ifndef WORLD_SUPPORT_H
#define WORLD_SUPPORT_H

#include "world_surface.h"

#include <stddef.h>

float world_support_find_floor_y(const world_climbable_surface *surfaces,
                                 size_t surface_count,
                                 float player_radius,
                                 float player_feet_y,
                                 float x,
                                 float z);

#endif
