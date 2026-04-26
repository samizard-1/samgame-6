#include "world_surface.h"

world_climbable_surface world_climbable_surface_from_block(const world_block *block,
                                                           size_t block_index)
{
    world_climbable_surface surface;

    if (block == NULL) {
        surface.min_x = 0.0f;
        surface.max_x = 0.0f;
        surface.min_z = 0.0f;
        surface.max_z = 0.0f;
        surface.bottom_y = 0.0f;
        surface.top_y = 0.0f;
        surface.source = WORLD_CLIMBABLE_SURFACE_SOURCE_UNKNOWN;
        surface.source_index = block_index;
        return surface;
    }

    surface.min_x = block->x - block->half_x;
    surface.max_x = block->x + block->half_x;
    surface.min_z = block->z - block->half_z;
    surface.max_z = block->z + block->half_z;
    surface.bottom_y = block->bottom_y;
    surface.top_y = block->height;
    surface.source = WORLD_CLIMBABLE_SURFACE_SOURCE_BLOCK;
    surface.source_index = block_index;

    return surface;
}

void world_climbable_surfaces_from_blocks(const world_block *blocks,
                                          size_t block_count,
                                          world_climbable_surface *out_surfaces)
{
    size_t block_index;

    if (blocks == NULL || out_surfaces == NULL) {
        return;
    }

    for (block_index = 0u; block_index < block_count; ++block_index) {
        out_surfaces[block_index] = world_climbable_surface_from_block(&blocks[block_index], block_index);
    }
}
