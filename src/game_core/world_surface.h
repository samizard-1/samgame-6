#ifndef WORLD_SURFACE_H
#define WORLD_SURFACE_H

#include "world_column.h"

#include <stddef.h>

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

world_climbable_surface world_climbable_surface_from_column(const world_column *column,
                                                            size_t column_index);
void world_climbable_surfaces_from_columns(const world_column *columns,
                                           size_t column_count,
                                           world_climbable_surface *out_surfaces);

#endif
