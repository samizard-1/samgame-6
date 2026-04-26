#ifndef WORLD_BLOCK_H
#define WORLD_BLOCK_H

typedef struct world_block {
    float x;
    float z;
    float bottom_y;
    float height;
    float half_x;
    float half_z;
    int level;
} world_block;

float world_block_height_for_level(int level);
world_block world_block_create(float x, float z, int level, float half_x, float half_z);

#endif
