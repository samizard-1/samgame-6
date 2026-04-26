#include "player_motion.h"

#include <stddef.h>

static const float PLAYER_MOTION_GROUND_EYE_Y = 2.0f;
static const float PLAYER_MOTION_JUMP_IMPULSE = 5.5f;
static const float PLAYER_MOTION_GRAVITY = 14.0f;

float player_motion_default_eye_height(void)
{
    return PLAYER_MOTION_GROUND_EYE_Y;
}

player_motion_state player_motion_create(void)
{
    player_motion_state state;

    state.eye_y = PLAYER_MOTION_GROUND_EYE_Y;
    state.velocity_y = 0.0f;
    state.grounded = true;

    return state;
}

player_pose player_pose_create(float x, float eye_y, float z)
{
    player_pose pose;

    pose.x = x;
    pose.eye_y = eye_y;
    pose.z = z;

    return pose;
}

void player_pose_set_xz(player_pose *pose, float x, float z)
{
    if (pose == NULL) {
        return;
    }

    pose->x = x;
    pose->z = z;
}

void player_pose_set_eye_y(player_pose *pose, float eye_y)
{
    if (pose == NULL) {
        return;
    }

    pose->eye_y = eye_y;
}

void player_motion_request_jump(player_motion_state *state)
{
    if (state == NULL || !state->grounded) {
        return;
    }

    state->velocity_y = PLAYER_MOTION_JUMP_IMPULSE;
    state->grounded = false;
}

void player_motion_update(player_motion_state *state,
                          float delta_seconds,
                          float support_eye_y,
                          float ceiling_y)
{
    if (state == NULL || delta_seconds <= 0.0f) {
        return;
    }

    if (support_eye_y < PLAYER_MOTION_GROUND_EYE_Y) {
        support_eye_y = PLAYER_MOTION_GROUND_EYE_Y;
    }

    if (state->grounded) {
        if (state->eye_y <= support_eye_y) {
            state->eye_y = support_eye_y;
        } else {
            state->grounded = false;
        }
        state->velocity_y = 0.0f;

        if (state->grounded) {
            return;
        }
    }

    state->velocity_y -= PLAYER_MOTION_GRAVITY * delta_seconds;
    state->eye_y += state->velocity_y * delta_seconds;

    if (ceiling_y > support_eye_y && state->eye_y > ceiling_y) {
        state->eye_y = ceiling_y;

        if (state->velocity_y > 0.0f) {
            state->velocity_y = 0.0f;
        }
    }

    if (state->eye_y <= support_eye_y) {
        state->eye_y = support_eye_y;
        state->velocity_y = 0.0f;
        state->grounded = true;
    }
}
