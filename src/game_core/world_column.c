#include "world_column.h"

#include "world_config.h"

static int world_column_clamp_level(int level)
{
    if (level < WORLD_PILLAR_MIN_LEVEL) {
        return WORLD_PILLAR_MIN_LEVEL;
    }

    if (level > WORLD_PILLAR_MAX_LEVEL) {
        return WORLD_PILLAR_MAX_LEVEL;
    }

    return level;
}

float world_pillar_height_for_level(int level)
{
    return (float)world_column_clamp_level(level) * WORLD_PILLAR_LEVEL_HEIGHT;
}

world_column world_column_create(float x, float z, int level, float radius)
{
    world_column column;

    column.x = x;
    column.z = z;
    column.level = world_column_clamp_level(level);
    column.height = world_pillar_height_for_level(column.level);
    column.radius = radius;

    return column;
}
