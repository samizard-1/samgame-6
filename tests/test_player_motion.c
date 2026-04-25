#include <assert.h>
#include <math.h>
#include <stddef.h>

#include "../src/game_core/player_motion.h"

static void assert_float_equal(float left, float right)
{
    assert(fabsf(left - right) <= 0.0001f);
}

static void test_player_motion_starts_grounded(void)
{
    const player_motion_state state = player_motion_create();

    assert_float_equal(player_motion_default_eye_height(), 2.0f);
    assert_float_equal(state.eye_y, 2.0f);
    assert_float_equal(state.velocity_y, 0.0f);
    assert(state.grounded);
}

static void test_player_pose_tracks_resolved_position(void)
{
    player_pose pose = player_pose_create(1.0f, player_motion_default_eye_height(), -2.0f);

    assert_float_equal(pose.x, 1.0f);
    assert_float_equal(pose.eye_y, player_motion_default_eye_height());
    assert_float_equal(pose.z, -2.0f);

    player_pose_set_xz(&pose, 3.0f, 4.0f);
    player_pose_set_eye_y(&pose, 5.0f);

    assert_float_equal(pose.x, 3.0f);
    assert_float_equal(pose.eye_y, 5.0f);
    assert_float_equal(pose.z, 4.0f);
}

static void test_jump_moves_up_from_ground(void)
{
    player_motion_state state = player_motion_create();

    player_motion_request_jump(&state);
    player_motion_update(&state, 0.1f, player_motion_default_eye_height(), 0.0f);

    assert(!state.grounded);
    assert(state.eye_y > player_motion_default_eye_height());
    assert(state.velocity_y > 0.0f);
}

static void test_airborne_jump_request_does_not_double_jump(void)
{
    player_motion_state state = player_motion_create();
    float velocity_after_first_jump;

    player_motion_request_jump(&state);
    velocity_after_first_jump = state.velocity_y;
    player_motion_update(&state, 0.1f, player_motion_default_eye_height(), 0.0f);
    player_motion_request_jump(&state);

    assert(!state.grounded);
    assert(state.velocity_y < velocity_after_first_jump);
}

static void test_fall_clamps_back_to_ground(void)
{
    player_motion_state state = player_motion_create();
    size_t step;

    player_motion_request_jump(&state);

    for (step = 0u; step < 120u; ++step) {
        player_motion_update(&state, 1.0f / 60.0f, player_motion_default_eye_height(), 0.0f);
    }

    assert_float_equal(state.eye_y, player_motion_default_eye_height());
    assert_float_equal(state.velocity_y, 0.0f);
    assert(state.grounded);
}

static void test_fall_clamps_to_raised_support(void)
{
    player_motion_state state = player_motion_create();
    const float support_eye_y = player_motion_default_eye_height() + 1.0f;

    state.eye_y = support_eye_y + 0.1f;
    state.velocity_y = -2.0f;
    state.grounded = false;

    player_motion_update(&state, 0.1f, support_eye_y, 18.0f);

    assert_float_equal(state.eye_y, support_eye_y);
    assert_float_equal(state.velocity_y, 0.0f);
    assert(state.grounded);
}

static void test_grounded_player_remains_on_raised_support(void)
{
    player_motion_state state = player_motion_create();
    const float support_eye_y = player_motion_default_eye_height() + 1.0f;

    state.eye_y = support_eye_y;
    state.grounded = true;

    player_motion_update(&state, 0.1f, support_eye_y, 18.0f);

    assert_float_equal(state.eye_y, support_eye_y);
    assert_float_equal(state.velocity_y, 0.0f);
    assert(state.grounded);
}

static void test_jump_starts_from_raised_support(void)
{
    player_motion_state state = player_motion_create();
    const float support_eye_y = player_motion_default_eye_height() + 1.0f;

    state.eye_y = support_eye_y;
    state.grounded = true;

    player_motion_request_jump(&state);
    player_motion_update(&state, 0.1f, support_eye_y, 18.0f);

    assert(state.eye_y > support_eye_y);
    assert(state.velocity_y > 0.0f);
    assert(!state.grounded);
}

static void test_grounded_player_falls_when_support_drops(void)
{
    player_motion_state state = player_motion_create();
    const float support_eye_y = player_motion_default_eye_height() + 1.0f;

    state.eye_y = support_eye_y;
    state.grounded = true;

    player_motion_update(&state, 0.1f, player_motion_default_eye_height(), 18.0f);

    assert(state.eye_y < support_eye_y);
    assert(state.eye_y > player_motion_default_eye_height());
    assert(state.velocity_y < 0.0f);
    assert(!state.grounded);
}

static void test_ceiling_clamps_upward_motion(void)
{
    player_motion_state state = player_motion_create();
    const float ceiling_y = 2.1f;

    player_motion_request_jump(&state);
    player_motion_update(&state, 0.1f, player_motion_default_eye_height(), ceiling_y);

    assert_float_equal(state.eye_y, ceiling_y);
    assert_float_equal(state.velocity_y, 0.0f);
    assert(!state.grounded);
}

static void test_ceiling_is_noop_below_roof(void)
{
    player_motion_state state = player_motion_create();
    const float ceiling_y = 18.0f;

    player_motion_request_jump(&state);
    player_motion_update(&state, 0.1f, player_motion_default_eye_height(), ceiling_y);

    assert(state.eye_y > player_motion_default_eye_height());
    assert(state.eye_y < ceiling_y);
    assert(state.velocity_y > 0.0f);
    assert(!state.grounded);
}

static void test_zero_or_negative_delta_is_noop(void)
{
    player_motion_state state = player_motion_create();

    player_motion_request_jump(&state);
    player_motion_update(&state, 0.0f, player_motion_default_eye_height(), 0.0f);

    assert_float_equal(state.eye_y, player_motion_default_eye_height());
    assert_float_equal(state.velocity_y, 5.5f);
    assert(!state.grounded);

    player_motion_update(&state, -1.0f, player_motion_default_eye_height(), 0.0f);

    assert_float_equal(state.eye_y, player_motion_default_eye_height());
    assert_float_equal(state.velocity_y, 5.5f);
    assert(!state.grounded);
}

void test_player_motion(void)
{
    test_player_motion_starts_grounded();
    test_player_pose_tracks_resolved_position();
    test_jump_moves_up_from_ground();
    test_airborne_jump_request_does_not_double_jump();
    test_fall_clamps_back_to_ground();
    test_fall_clamps_to_raised_support();
    test_grounded_player_remains_on_raised_support();
    test_jump_starts_from_raised_support();
    test_grounded_player_falls_when_support_drops();
    test_ceiling_clamps_upward_motion();
    test_ceiling_is_noop_below_roof();
    test_zero_or_negative_delta_is_noop();
}
