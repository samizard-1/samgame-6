#include "world_block.h"

#include "world_config.h"

#include <math.h>

float world_block_height_for_level(int level)
{
    return (float)level * WORLD_BLOCK_LEVEL_HEIGHT;
}

world_block world_block_create(float x, float z, int level, float half_x, float half_z)
{
    world_block block;
    const float top_y = world_block_height_for_level(level);

    block.x = x;
    block.z = z;
    block.level = level;
    block.height = top_y;
    block.bottom_y = fmaxf(0.0f, top_y - WORLD_BLOCK_HEIGHT_Y);
    block.half_x = half_x;
    block.half_z = half_z;

    return block;
}
