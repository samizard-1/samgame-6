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

void player_motion_request_jump(player_motion_state *state)
{
    if (state == NULL || !state->grounded) {
        return;
    }

    state->velocity_y = PLAYER_MOTION_JUMP_IMPULSE;
    state->grounded = false;
}

void player_motion_update(player_motion_state *state, float delta_seconds)
{
    if (state == NULL || delta_seconds <= 0.0f) {
        return;
    }

    if (state->grounded) {
        state->eye_y = PLAYER_MOTION_GROUND_EYE_Y;
        state->velocity_y = 0.0f;
        return;
    }

    state->velocity_y -= PLAYER_MOTION_GRAVITY * delta_seconds;
    state->eye_y += state->velocity_y * delta_seconds;

    if (state->eye_y <= PLAYER_MOTION_GROUND_EYE_Y) {
        state->eye_y = PLAYER_MOTION_GROUND_EYE_Y;
        state->velocity_y = 0.0f;
        state->grounded = true;
    }
}
