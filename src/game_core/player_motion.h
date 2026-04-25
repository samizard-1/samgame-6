#ifndef PLAYER_MOTION_H
#define PLAYER_MOTION_H

#include <stdbool.h>

typedef struct player_motion_state {
    float eye_y;
    float velocity_y;
    bool grounded;
} player_motion_state;

typedef struct player_pose {
    float x;
    float eye_y;
    float z;
} player_pose;

float player_motion_default_eye_height(void);
player_motion_state player_motion_create(void);
player_pose player_pose_create(float x, float eye_y, float z);
void player_pose_set_xz(player_pose *pose, float x, float z);
void player_pose_set_eye_y(player_pose *pose, float eye_y);
void player_motion_request_jump(player_motion_state *state);
void player_motion_update(player_motion_state *state,
                          float delta_seconds,
                          float support_eye_y,
                          float ceiling_y);

#endif
