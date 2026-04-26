#include "world_layout.h"

#include "world_config.h"

world_layout_bounds world_layout_default_bounds(void)
{
    world_layout_bounds bounds;

    bounds.min_x = WORLD_ROOM_MIN_X;
    bounds.max_x = WORLD_ROOM_MAX_X;
    bounds.min_z = WORLD_ROOM_MIN_Z;
    bounds.max_z = WORLD_ROOM_MAX_Z;
    bounds.min_height = WORLD_BLOCK_MIN_HEIGHT;
    bounds.max_height = WORLD_BLOCK_MAX_HEIGHT;
    bounds.block_half_x = WORLD_BLOCK_HALF_X;
    bounds.block_half_z = WORLD_BLOCK_HALF_Z;

    return bounds;
}

float world_collision_player_radius(void)
{
    return WORLD_PLAYER_COLLISION_RADIUS;
}

float world_layout_default_roof_y(void)
{
    return WORLD_ROOF_UNDERSIDE_Y;
}

world_collision_walls world_collision_default_walls(void)
{
    const world_layout_bounds bounds = world_layout_default_bounds();
    const float half_wall_thickness = WORLD_WALL_THICKNESS * 0.5f;
    world_collision_walls walls;

    walls.min_x = bounds.min_x - bounds.block_half_x + half_wall_thickness;
    walls.max_x = bounds.max_x + bounds.block_half_x - half_wall_thickness;
    walls.min_z = bounds.min_z - bounds.block_half_z + half_wall_thickness;
    walls.max_z = bounds.max_z + bounds.block_half_z - half_wall_thickness;
    walls.block_min_x = true;
    walls.block_max_x = true;
    walls.block_min_z = true;
    walls.block_max_z = true;

    return walls;
}
