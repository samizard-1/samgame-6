#include "world_surface.h"

world_climbable_surface world_climbable_surface_from_column(const world_column *column,
                                                            size_t column_index)
{
    world_climbable_surface surface;

    if (column == NULL) {
        surface.min_x = 0.0f;
        surface.max_x = 0.0f;
        surface.min_z = 0.0f;
        surface.max_z = 0.0f;
        surface.top_y = 0.0f;
        surface.source = WORLD_CLIMBABLE_SURFACE_SOURCE_UNKNOWN;
        surface.source_index = column_index;
        return surface;
    }

    surface.min_x = column->x - column->radius;
    surface.max_x = column->x + column->radius;
    surface.min_z = column->z - column->radius;
    surface.max_z = column->z + column->radius;
    surface.top_y = column->height;
    surface.source = WORLD_CLIMBABLE_SURFACE_SOURCE_COLUMN;
    surface.source_index = column_index;

    return surface;
}

void world_climbable_surfaces_from_columns(const world_column *columns,
                                           size_t column_count,
                                           world_climbable_surface *out_surfaces)
{
    size_t column_index;

    if (columns == NULL || out_surfaces == NULL) {
        return;
    }

    for (column_index = 0u; column_index < column_count; ++column_index) {
        out_surfaces[column_index] = world_climbable_surface_from_column(&columns[column_index], column_index);
    }
}
