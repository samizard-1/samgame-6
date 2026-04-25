#ifndef WORLD_GEN_H
#define WORLD_GEN_H

#include <stdbool.h>
#include <stddef.h>

typedef struct world_column {
    float x;
    float z;
    float height;
    float radius;
    int level;
} world_column;

typedef enum world_climbable_surface_source {
    WORLD_CLIMBABLE_SURFACE_SOURCE_UNKNOWN = 0,
    WORLD_CLIMBABLE_SURFACE_SOURCE_COLUMN = 1
} world_climbable_surface_source;

typedef struct world_climbable_surface {
    float min_x;
    float max_x;
    float min_z;
    float max_z;
    float top_y;
    world_climbable_surface_source source;
    size_t source_index;
} world_climbable_surface;

typedef struct world_layout_bounds {
    float min_x;
    float max_x;
    float min_z;
    float max_z;
    float min_height;
    float max_height;
    float radius;
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

typedef struct world_gen_params {
    size_t guaranteed_chain_length;
    float min_jumpable_distance;
    float max_jumpable_distance;
    float nearby_pillar_chance;
    float upward_step_chance;
    int max_extra_level_delta;
} world_gen_params;

world_layout_bounds world_layout_default_bounds(void);
float world_collision_player_radius(void);
float world_layout_default_roof_y(void);
world_collision_walls world_collision_default_walls(void);
world_gen_params world_gen_default_params(void);
float world_pillar_height_for_level(int level);
void world_gen_generate(unsigned int seed, size_t count, world_column *out_columns);
void world_gen_generate_with_params(unsigned int seed,
                                    size_t count,
                                    const world_gen_params *params,
                                    world_column *out_columns);
world_climbable_surface world_climbable_surface_from_column(const world_column *column,
                                                            size_t column_index);
void world_climbable_surfaces_from_columns(const world_column *columns,
                                           size_t column_count,
                                           world_climbable_surface *out_surfaces);
void world_collision_resolve_player_surfaces_xz(const world_collision_walls *walls,
                                                const world_climbable_surface *surfaces,
                                                size_t surface_count,
                                                float player_radius,
                                                float player_feet_y,
                                                float *x,
                                                float *z);
void world_collision_resolve_player_xz(const world_collision_walls *walls,
                                       const world_column *columns,
                                       size_t column_count,
                                       float player_radius,
                                       float player_feet_y,
                                       float *x,
                                       float *z);
float world_support_find_floor_y_for_surfaces(const world_climbable_surface *surfaces,
                                              size_t surface_count,
                                              float player_radius,
                                              float player_feet_y,
                                              float x,
                                              float z);
float world_support_find_floor_y(const world_column *columns,
                                 size_t column_count,
                                 float player_radius,
                                 float player_feet_y,
                                 float x,
                                 float z);

#endif
