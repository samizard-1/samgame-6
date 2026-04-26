#ifndef WORLD_LAYOUT_H
#define WORLD_LAYOUT_H

#include <stdbool.h>

typedef struct world_layout_bounds {
    float min_x;
    float max_x;
    float min_z;
    float max_z;
    float min_height;
    float max_height;
    float block_half_x;
    float block_half_z;
} world_layout_bounds;

typedef struct world_collision_walls {
    float min_x;
    float max_x;
    float min_z;
    float max_z;
    bool block_min_x;
    bool block_max_x;
    bool block_min_z;
    bool block_max_z;
} world_collision_walls;

world_layout_bounds world_layout_default_bounds(void);
float world_collision_player_radius(void);
world_collision_walls world_collision_default_walls(void);

#endif
