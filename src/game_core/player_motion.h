#ifndef PLAYER_MOTION_H
#define PLAYER_MOTION_H

#include <stdbool.h>

typedef struct player_motion_state {
    float eye_y;
    float velocity_y;
    bool grounded;
} player_motion_state;

float player_motion_default_eye_height(void);
player_motion_state player_motion_create(void);
void player_motion_request_jump(player_motion_state *state);
void player_motion_update(player_motion_state *state, float delta_seconds);

#endif
