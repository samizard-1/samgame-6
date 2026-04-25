#ifndef WORLD_GEN_H
#define WORLD_GEN_H

#include <stdbool.h>
#include <stddef.h>

typedef struct world_column {
    float x;
    float z;
    float height;
    float radius;
} world_column;

typedef struct world_gen_bounds {
    float min_x;
    float max_x;
    float min_z;
    float max_z;
    float min_height;
    float max_height;
    float radius;
} world_gen_bounds;

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

world_gen_bounds world_gen_default_bounds(void);
float world_gen_player_collision_radius(void);
world_collision_walls world_gen_default_collision_walls(void);
void world_gen_generate(unsigned int seed, size_t count, world_column *out_columns);
void world_gen_resolve_player_xz(const world_collision_walls *walls,
                                 const world_column *columns,
                                 size_t column_count,
                                 float player_radius,
                                 float *x,
                                 float *z);

#endif
