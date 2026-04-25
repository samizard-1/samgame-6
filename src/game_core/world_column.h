#ifndef WORLD_COLUMN_H
#define WORLD_COLUMN_H

typedef struct world_column {
    float x;
    float z;
    float height;
    float radius;
    int level;
} world_column;

float world_pillar_height_for_level(int level);
world_column world_column_create(float x, float z, int level, float radius);

#endif
