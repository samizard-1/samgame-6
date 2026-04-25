#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include "../src/app_config.h"
#include "../src/game_core/player_motion.h"
#include "../src/game_core/world_collision.h"
#include "../src/game_core/world_config.h"
#include "../src/game_core/world_gen.h"
#include "../src/game_core/world_support.h"
#include "../src/game_core/world_surface.h"

void test_startup_config(void);
void test_player_motion(void);

typedef struct candidate_position {
    float x;
    float y;
    float z;
} candidate_position;

static void assert_column_equal(world_column left, world_column right)
{
    assert(left.x == right.x);
    assert(left.z == right.z);
    assert(left.height == right.height);
    assert(left.radius == right.radius);
    assert(left.level == right.level);
}

static void assert_float_equal(float left, float right)
{
    assert(fabsf(left - right) <= 0.0001f);
}

static void assert_resolved_clear_of_column(world_column column,
                                            float player_radius,
                                            float x,
                                            float z)
{
    const float column_min_x = column.x - column.radius;
    const float column_max_x = column.x + column.radius;
    const float column_min_z = column.z - column.radius;
    const float column_max_z = column.z + column.radius;
    float closest_x = x;
    float closest_z = z;
    const float player_radius_squared = player_radius * player_radius;
    float delta_x;
    float delta_z;
    float distance_squared;

    if (closest_x < column_min_x) {
        closest_x = column_min_x;
    } else if (closest_x > column_max_x) {
        closest_x = column_max_x;
    }

    if (closest_z < column_min_z) {
        closest_z = column_min_z;
    } else if (closest_z > column_max_z) {
        closest_z = column_max_z;
    }

    delta_x = x - closest_x;
    delta_z = z - closest_z;
    distance_squared = (delta_x * delta_x) + (delta_z * delta_z);

    assert(distance_squared + 0.0001f >= player_radius_squared);
}

static void assert_columns_do_not_overlap(const world_column *columns, size_t column_count)
{
    size_t left_index;

    for (left_index = 0u; left_index < column_count; ++left_index) {
        size_t right_index;

        for (right_index = left_index + 1u; right_index < column_count; ++right_index) {
            const float min_clearance = columns[left_index].radius + columns[right_index].radius;
            const bool overlaps_x = fabsf(columns[left_index].x - columns[right_index].x) < min_clearance;
            const bool overlaps_z = fabsf(columns[left_index].z - columns[right_index].z) < min_clearance;

            assert(!overlaps_x || !overlaps_z);
        }
    }
}

static world_collision_walls make_open_walls(void)
{
    world_collision_walls walls = { 0 };

    walls.min_x = -100.0f;
    walls.max_x = 100.0f;
    walls.min_z = -100.0f;
    walls.max_z = 100.0f;
    walls.block_min_x = false;
    walls.block_max_x = false;
    walls.block_min_z = false;
    walls.block_max_z = false;

    return walls;
}

static void resolve_player_against_columns(const world_collision_walls *walls,
                                           const world_column *columns,
                                           size_t column_count,
                                           float player_radius,
                                           float player_feet_y,
                                           float *x,
                                           float *z)
{
    world_climbable_surface surfaces[APP_DEFAULT_COLUMN_COUNT];

    assert(column_count <= APP_DEFAULT_COLUMN_COUNT);
    world_climbable_surfaces_from_columns(columns, column_count, surfaces);
    world_collision_resolve_player_surfaces_xz(walls, surfaces, column_count, player_radius, player_feet_y, x, z);
}

static float find_floor_y_for_columns(const world_column *columns,
                                      size_t column_count,
                                      float player_radius,
                                      float player_feet_y,
                                      float x,
                                      float z)
{
    world_climbable_surface surfaces[APP_DEFAULT_COLUMN_COUNT];

    assert(column_count <= APP_DEFAULT_COLUMN_COUNT);
    world_climbable_surfaces_from_columns(columns, column_count, surfaces);
    return world_support_find_floor_y(surfaces, column_count, player_radius, player_feet_y, x, z);
}

static void test_generation_defaults_and_determinism(void)
{
    world_layout_bounds bounds = world_layout_default_bounds();
    const world_gen_params params = world_gen_default_params();
    world_column generated[APP_DEFAULT_COLUMN_COUNT];
    world_column repeated[APP_DEFAULT_COLUMN_COUNT];
    world_gen_result generated_result;
    world_gen_result repeated_result;
    world_gen_result empty_result;
    size_t index;

    assert(bounds.min_x == WORLD_ROOM_MIN_X);
    assert(bounds.max_x == WORLD_ROOM_MAX_X);
    assert(bounds.min_z == WORLD_ROOM_MIN_Z);
    assert(bounds.max_z == WORLD_ROOM_MAX_Z);
    assert(bounds.min_height == WORLD_COLUMN_MIN_HEIGHT);
    assert(bounds.max_height == WORLD_COLUMN_MAX_HEIGHT);
    assert(bounds.radius == WORLD_COLUMN_RADIUS);
    assert(params.guaranteed_chain_length == WORLD_PILLAR_DEFAULT_CHAIN_LENGTH);
    assert_float_equal(params.min_jumpable_distance, WORLD_PILLAR_DEFAULT_MIN_JUMPABLE_DISTANCE);
    assert_float_equal(params.max_jumpable_distance, WORLD_PILLAR_DEFAULT_MAX_JUMPABLE_DISTANCE);
    assert_float_equal(world_pillar_height_for_level(WORLD_PILLAR_MIN_LEVEL), WORLD_COLUMN_MIN_HEIGHT);
    assert(world_pillar_height_for_level(WORLD_PILLAR_MAX_LEVEL) <= WORLD_COLUMN_MAX_HEIGHT);
    assert(world_pillar_height_for_level(WORLD_PILLAR_MAX_LEVEL + 1) > WORLD_COLUMN_MAX_HEIGHT - WORLD_PILLAR_LEVEL_HEIGHT);

    generated_result = world_gen_generate(1234u, APP_DEFAULT_COLUMN_COUNT, generated);
    repeated_result = world_gen_generate(1234u, APP_DEFAULT_COLUMN_COUNT, repeated);
    empty_result = world_gen_generate(4321u, 0u, NULL);

    assert(generated_result.success);
    assert(generated_result.generated_count == APP_DEFAULT_COLUMN_COUNT);
    assert(repeated_result.success);
    assert(repeated_result.generated_count == APP_DEFAULT_COLUMN_COUNT);
    assert(empty_result.success);
    assert(empty_result.generated_count == 0u);

    for (index = 0; index < APP_DEFAULT_COLUMN_COUNT; ++index) {
        assert_column_equal(generated[index], repeated[index]);
        assert(generated[index].x >= bounds.min_x);
        assert(generated[index].x <= bounds.max_x);
        assert(generated[index].z >= bounds.min_z);
        assert(generated[index].z <= bounds.max_z);
        assert(generated[index].height >= bounds.min_height);
        assert(generated[index].height <= bounds.max_height);
        assert_float_equal(generated[index].height, world_pillar_height_for_level(generated[index].level));
        assert(generated[index].radius == bounds.radius);
    }

    assert_columns_do_not_overlap(generated, APP_DEFAULT_COLUMN_COUNT);
}

static void test_level_one_is_reachable_but_level_two_is_not_from_floor(void)
{
    player_motion_state state = player_motion_create();
    float peak_feet_y = 0.0f;
    size_t step;

    player_motion_request_jump(&state);

    for (step = 0u; step < 120u; ++step) {
        const float feet_y = state.eye_y - player_motion_default_eye_height();

        if (feet_y > peak_feet_y) {
            peak_feet_y = feet_y;
        }

        player_motion_update(&state, 1.0f / 60.0f, player_motion_default_eye_height(), 0.0f);
    }

    assert(peak_feet_y >= world_pillar_height_for_level(1));
    assert(peak_feet_y < world_pillar_height_for_level(2));
}

static void test_default_generation_starts_with_jumpable_chain(void)
{
    const world_gen_params params = world_gen_default_params();
    world_column generated[APP_DEFAULT_COLUMN_COUNT];
    world_gen_result result;
    size_t index;

    result = world_gen_generate(4321u, APP_DEFAULT_COLUMN_COUNT, generated);

    assert(result.success);
    assert(result.generated_count == APP_DEFAULT_COLUMN_COUNT);
    assert(params.guaranteed_chain_length <= APP_DEFAULT_COLUMN_COUNT);

    for (index = 0u; index < params.guaranteed_chain_length; ++index) {
        assert(generated[index].level == (int)index + WORLD_PILLAR_MIN_LEVEL);
        assert_float_equal(generated[index].height, world_pillar_height_for_level(generated[index].level));

        if (index > 0u) {
            const float delta_x = generated[index].x - generated[index - 1u].x;
            const float delta_z = generated[index].z - generated[index - 1u].z;
            const float distance = sqrtf((delta_x * delta_x) + (delta_z * delta_z));

            assert(distance >= params.min_jumpable_distance - 0.0001f);
            assert(distance <= params.max_jumpable_distance + 0.0001f);
            assert(generated[index].level - generated[index - 1u].level == 1);
        }
    }

    assert_columns_do_not_overlap(generated, APP_DEFAULT_COLUMN_COUNT);
}

static void test_generation_params_control_chain_and_nearby_upward_pillars(void)
{
    world_gen_params params = world_gen_default_params();
    world_column generated[6u];
    world_column repeated[6u];
    world_gen_result generated_result;
    world_gen_result repeated_result;
    size_t index;

    params.guaranteed_chain_length = 3u;
    params.min_jumpable_distance = 2.0f;
    params.max_jumpable_distance = 3.0f;
    params.nearby_pillar_chance = 1.0f;
    params.upward_step_chance = 1.0f;
    params.max_extra_level_delta = 1;

    generated_result = world_gen_generate_with_params(99u, 6u, &params, generated);
    repeated_result = world_gen_generate_with_params(99u, 6u, &params, repeated);

    assert(generated_result.success);
    assert(generated_result.generated_count == 6u);
    assert(repeated_result.success);
    assert(repeated_result.generated_count == 6u);

    for (index = 0u; index < 6u; ++index) {
        assert_column_equal(generated[index], repeated[index]);
        assert_float_equal(generated[index].height, world_pillar_height_for_level(generated[index].level));

        if (index < params.guaranteed_chain_length) {
            assert(generated[index].level == (int)index + WORLD_PILLAR_MIN_LEVEL);
        } else {
            size_t anchor_index;
            bool is_near_previous = false;

            for (anchor_index = 0u; anchor_index < index; ++anchor_index) {
                const float delta_x = generated[index].x - generated[anchor_index].x;
                const float delta_z = generated[index].z - generated[anchor_index].z;
                const float distance = sqrtf((delta_x * delta_x) + (delta_z * delta_z));

                if (distance <= params.max_jumpable_distance + 0.0001f) {
                    is_near_previous = true;
                }
            }

            assert(is_near_previous);
        }
    }

    assert_columns_do_not_overlap(generated, 6u);
}

static void test_generation_reports_failure_when_capacity_is_exhausted(void)
{
    world_column generated[300u];
    const world_gen_result result = world_gen_generate(7u, 300u, generated);

    assert(!result.success);
    assert(result.generated_count < 300u);
    assert_columns_do_not_overlap(generated, result.generated_count);
}

static void test_column_create_keeps_level_and_height_aligned(void)
{
    const world_column low = world_column_create(1.0f, 2.0f, WORLD_PILLAR_MIN_LEVEL - 1, 3.0f);
    const world_column high = world_column_create(4.0f, 5.0f, WORLD_PILLAR_MAX_LEVEL + 1, 6.0f);

    assert(low.level == WORLD_PILLAR_MIN_LEVEL);
    assert_float_equal(low.height, world_pillar_height_for_level(low.level));
    assert_float_equal(low.radius, 3.0f);
    assert(high.level == WORLD_PILLAR_MAX_LEVEL);
    assert_float_equal(high.height, world_pillar_height_for_level(high.level));
    assert_float_equal(high.radius, 6.0f);
}

static void test_default_collision_walls(void)
{
    const world_collision_walls walls = world_collision_default_walls();

    assert(walls.block_min_x);
    assert(walls.block_max_x);
    assert(walls.block_min_z);
    assert(walls.block_max_z);
}

static void test_default_roof_clears_future_pillar_top_jump(void)
{
    assert_float_equal(world_layout_default_roof_y(), WORLD_ROOF_UNDERSIDE_Y);
    assert(world_layout_default_roof_y() > WORLD_COLUMN_MAX_HEIGHT + player_motion_default_eye_height());
}

static void test_rendered_walls_push_candidates_back_inside(void)
{
    const world_collision_walls walls = world_collision_default_walls();
    const float player_radius = world_collision_player_radius();
    float x = walls.min_x - player_radius - 3.0f;
    float z = 0.0f;

    world_collision_resolve_player_surfaces_xz(&walls, NULL, 0u, player_radius, 0.0f, &x, &z);
    assert_float_equal(x, walls.min_x + player_radius);
    assert_float_equal(z, 0.0f);

    x = 0.0f;
    z = walls.min_z - player_radius - 3.0f;
    world_collision_resolve_player_surfaces_xz(&walls, NULL, 0u, player_radius, 0.0f, &x, &z);
    assert_float_equal(x, 0.0f);
    assert_float_equal(z, walls.min_z + player_radius);

    x = walls.max_x + player_radius + 3.0f;
    z = 0.0f;
    world_collision_resolve_player_surfaces_xz(&walls, NULL, 0u, player_radius, 0.0f, &x, &z);
    assert_float_equal(x, walls.max_x - player_radius);
    assert_float_equal(z, 0.0f);

    x = 0.0f;
    z = walls.max_z + player_radius + 3.0f;
    world_collision_resolve_player_surfaces_xz(&walls, NULL, 0u, player_radius, 0.0f, &x, &z);
    assert_float_equal(x, 0.0f);
    assert_float_equal(z, walls.max_z - player_radius);
}

static void test_min_z_wall_blocks_escape(void)
{
    const world_collision_walls walls = world_collision_default_walls();
    const float player_radius = world_collision_player_radius();
    float x = 2.0f;
    float z = walls.min_z - player_radius - 3.0f;

    world_collision_resolve_player_surfaces_xz(&walls, NULL, 0u, player_radius, 0.0f, &x, &z);
    assert_float_equal(x, 2.0f);
    assert_float_equal(z, walls.min_z + player_radius);
}

static void test_column_collision_uses_square_footprint(void)
{
    const world_collision_walls walls = make_open_walls();
    const world_column column = world_column_create(0.0f, 0.0f, 4, 1.0f);
    const float player_radius = world_collision_player_radius();
    const float combined_radius = column.radius + player_radius;
    float x = column.x + combined_radius - 0.1f;
    float z = column.z + column.radius;
    const float distance_from_center_squared = (x * x) + (z * z);

    assert(distance_from_center_squared > (combined_radius * combined_radius));

    resolve_player_against_columns(&walls, &column, 1u, player_radius, 0.0f, &x, &z);

    assert_float_equal(x, column.x + combined_radius);
    assert_float_equal(z, column.z + column.radius);
    assert_resolved_clear_of_column(column, player_radius, x, z);
}

static void test_support_defaults_to_ground_without_geometry(void)
{
    const float player_radius = world_collision_player_radius();

    assert_float_equal(world_support_find_floor_y(NULL, 0u, player_radius, 3.0f, 0.0f, 0.0f), 0.0f);
}

static void test_column_top_is_support_when_footprint_overlaps(void)
{
    const world_column column = world_column_create(0.0f, 0.0f, 1, 1.0f);
    const float player_radius = world_collision_player_radius();
    const float floor_y = find_floor_y_for_columns(&column, 1u, player_radius, 1.0f, 0.0f, 0.0f);

    assert_float_equal(floor_y, column.height);
}

static void test_column_top_support_allows_small_height_error(void)
{
    const world_column column = world_column_create(0.0f, 0.0f, 1, 1.0f);
    const float player_radius = world_collision_player_radius();
    const float floor_y = find_floor_y_for_columns(&column, 1u, player_radius, 0.98f, 0.0f, 0.0f);

    assert_float_equal(floor_y, column.height);
}

static void test_highest_overlapping_support_wins(void)
{
    const world_column columns[] = {
        world_column_create(0.0f, 0.0f, 1, 1.0f),
        world_column_create(0.0f, 0.0f, 2, 1.0f),
        world_column_create(0.0f, 0.0f, 3, 1.0f)
    };
    const float player_radius = world_collision_player_radius();
    const float floor_y = find_floor_y_for_columns(columns, 3u, player_radius, 2.5f, 0.0f, 0.0f);

    assert_float_equal(floor_y, world_pillar_height_for_level(2));
}

static void test_column_side_blocks_below_top(void)
{
    const world_collision_walls walls = make_open_walls();
    const world_column column = world_column_create(0.0f, 0.0f, 1, 1.0f);
    const float player_radius = world_collision_player_radius();
    const float combined_radius = column.radius + player_radius;
    float x = column.x + combined_radius - 0.1f;
    float z = column.z;

    resolve_player_against_columns(&walls, &column, 1u, player_radius, column.height - 0.1f, &x, &z);

    assert_float_equal(x, column.x + combined_radius);
    assert_float_equal(z, column.z);
    assert_resolved_clear_of_column(column, player_radius, x, z);
}

static void test_column_side_does_not_block_on_top(void)
{
    const world_collision_walls walls = make_open_walls();
    const world_column column = world_column_create(0.0f, 0.0f, 1, 1.0f);
    const float player_radius = world_collision_player_radius();
    float x = column.x;
    float z = column.z;

    resolve_player_against_columns(&walls, &column, 1u, player_radius, column.height, &x, &z);

    assert_float_equal(x, column.x);
    assert_float_equal(z, column.z);
}

static void test_column_side_does_not_block_when_effectively_on_top(void)
{
    const world_collision_walls walls = make_open_walls();
    const world_column column = world_column_create(0.0f, 0.0f, 1, 1.0f);
    const float player_radius = world_collision_player_radius();
    float x = column.x;
    float z = column.z;

    resolve_player_against_columns(&walls, &column, 1u, player_radius, column.height - 0.02f, &x, &z);

    assert_float_equal(x, column.x);
    assert_float_equal(z, column.z);
}

static void test_generic_surface_supports_future_geometry(void)
{
    const world_climbable_surface surface = { -1.0f, 1.0f, -1.0f, 1.0f, 2.0f,
                                             WORLD_CLIMBABLE_SURFACE_SOURCE_UNKNOWN, 7u };
    const float player_radius = world_collision_player_radius();
    const float floor_y = world_support_find_floor_y(&surface,
                                                     1u,
                                                     player_radius,
                                                     2.0f,
                                                     0.0f,
                                                     0.0f);

    assert_float_equal(floor_y, 2.0f);
}

static void test_column_surface_conversion_preserves_support_footprint(void)
{
    const world_column column = world_column_create(4.0f, -3.0f, 2, 1.0f);
    const world_climbable_surface surface = world_climbable_surface_from_column(&column, 11u);

    assert_float_equal(surface.min_x, 3.0f);
    assert_float_equal(surface.max_x, 5.0f);
    assert_float_equal(surface.min_z, -4.0f);
    assert_float_equal(surface.max_z, -2.0f);
    assert_float_equal(surface.top_y, column.height);
    assert(surface.source == WORLD_CLIMBABLE_SURFACE_SOURCE_COLUMN);
    assert(surface.source_index == 11u);
}

static void test_exact_center_column_overlap_resolves_deterministically(void)
{
    const world_collision_walls walls = make_open_walls();
    const world_column column = world_column_create(0.0f, 0.0f, 3, 1.0f);
    const float player_radius = world_collision_player_radius();
    const float expected_x = column.x - column.radius - player_radius;
    float x_a = column.x;
    float z_a = column.z;
    float x_b = column.x;
    float z_b = column.z;

    resolve_player_against_columns(&walls, &column, 1u, player_radius, 0.0f, &x_a, &z_a);
    resolve_player_against_columns(&walls, &column, 1u, player_radius, 0.0f, &x_b, &z_b);

    assert_float_equal(x_a, expected_x);
    assert_float_equal(z_a, column.z);
    assert_float_equal(x_b, x_a);
    assert_float_equal(z_b, z_a);
    assert_resolved_clear_of_column(column, player_radius, x_a, z_a);
}

static void test_multi_column_overlap_resolves_deterministically(void)
{
    const world_collision_walls walls = make_open_walls();
    const world_column columns[] = {
        world_column_create(0.0f, 0.0f, 3, 1.0f),
        world_column_create(2.0f, 2.0f, 3, 1.0f)
    };
    const float player_radius = world_collision_player_radius();
    float x_a = 1.0f;
    float z_a = 1.0f;
    float x_b = 1.0f;
    float z_b = 1.0f;
    size_t index;

    resolve_player_against_columns(&walls, columns, 2u, player_radius, 0.0f, &x_a, &z_a);
    resolve_player_against_columns(&walls, columns, 2u, player_radius, 0.0f, &x_b, &z_b);

    assert_float_equal(x_a, 1.5f);
    assert_float_equal(z_a, 0.5f);
    assert_float_equal(x_b, x_a);
    assert_float_equal(z_b, z_a);

    for (index = 0; index < 2u; ++index) {
        assert_resolved_clear_of_column(columns[index], player_radius, x_a, z_a);
    }
}

static void test_valid_position_is_a_noop(void)
{
    const world_collision_walls walls = world_collision_default_walls();
    const world_column column = world_column_create(0.0f, 0.0f, 2, 1.0f);
    const float player_radius = world_collision_player_radius();
    float x = 5.0f;
    float z = -5.0f;
    const float initial_x = x;
    const float initial_z = z;

    resolve_player_against_columns(&walls, &column, 1u, player_radius, 0.0f, &x, &z);

    assert_float_equal(x, initial_x);
    assert_float_equal(z, initial_z);
}

static void test_xz_resolution_leaves_y_unchanged(void)
{
    const world_collision_walls walls = world_collision_default_walls();
    const float player_radius = world_collision_player_radius();
    candidate_position candidate = { walls.max_x + player_radius + 2.0f, 2.25f, 0.25f };

    world_collision_resolve_player_surfaces_xz(&walls, NULL, 0u, player_radius, 0.0f, &candidate.x, &candidate.z);

    assert_float_equal(candidate.x, walls.max_x - player_radius);
    assert_float_equal(candidate.y, 2.25f);
    assert_float_equal(candidate.z, 0.25f);
}

int main(void)
{
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

    test_startup_config();
    test_player_motion();
    test_generation_defaults_and_determinism();
    test_level_one_is_reachable_but_level_two_is_not_from_floor();
    test_default_generation_starts_with_jumpable_chain();
    test_generation_params_control_chain_and_nearby_upward_pillars();
    test_generation_reports_failure_when_capacity_is_exhausted();
    test_column_create_keeps_level_and_height_aligned();
    test_default_collision_walls();
    test_default_roof_clears_future_pillar_top_jump();
    test_rendered_walls_push_candidates_back_inside();
    test_min_z_wall_blocks_escape();
    test_column_collision_uses_square_footprint();
    test_support_defaults_to_ground_without_geometry();
    test_column_top_is_support_when_footprint_overlaps();
    test_column_top_support_allows_small_height_error();
    test_highest_overlapping_support_wins();
    test_column_side_blocks_below_top();
    test_column_side_does_not_block_on_top();
    test_column_side_does_not_block_when_effectively_on_top();
    test_generic_surface_supports_future_geometry();
    test_column_surface_conversion_preserves_support_footprint();
    test_exact_center_column_overlap_resolves_deterministically();
    test_multi_column_overlap_resolves_deterministically();
    test_valid_position_is_a_noop();
    test_xz_resolution_leaves_y_unchanged();

    return 0;
}
