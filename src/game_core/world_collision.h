#ifndef WORLD_COLLISION_H
#define WORLD_COLLISION_H

#include "world_layout.h"
#include "world_surface.h"

#include <stddef.h>

void world_collision_resolve_player_blocks_xz(const world_collision_walls *walls,
                                              const world_climbable_surface *surfaces,
                                              size_t surface_count,
                                              float player_radius,
                                              float player_feet_y,
                                              float player_top_y,
                                              float *x,
                                              float *z);
float world_collision_find_ceiling_y(const world_climbable_surface *surfaces,
                                     size_t surface_count,
                                     float player_radius,
                                     float player_feet_y,
                                     float player_top_y,
                                     float x,
                                     float z,
                                     float fallback_ceiling_y);

#endif
