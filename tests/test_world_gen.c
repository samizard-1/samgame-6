#include <assert.h>
#include <math.h>
#include <stddef.h>

#include "../src/game_core/startup_config.h"
#include "../src/game_core/world_gen.h"

void test_startup_config(void);

typedef struct seam_position {
    float x;
    float y;
    float z;
} seam_position;

static void assert_column_equal(world_column left, world_column right)
{
    assert(left.x == right.x);
    assert(left.z == right.z);
    assert(left.height == right.height);
    assert(left.radius == right.radius);
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

static void test_generation_defaults_and_determinism(void)
{
    world_gen_bounds bounds = world_gen_default_bounds();
    world_column generated[STARTUP_DEFAULT_COLUMN_COUNT];
    world_column repeated[STARTUP_DEFAULT_COLUMN_COUNT];
    size_t index;

    assert(bounds.min_x == -15.0f);
    assert(bounds.max_x == 15.0f);
    assert(bounds.min_z == -15.0f);
    assert(bounds.max_z == 15.0f);
    assert(bounds.min_height == 1.0f);
    assert(bounds.max_height == 12.0f);
    assert(bounds.radius == 1.0f);

    world_gen_generate(1234u, STARTUP_DEFAULT_COLUMN_COUNT, generated);
    world_gen_generate(1234u, STARTUP_DEFAULT_COLUMN_COUNT, repeated);
    world_gen_generate(4321u, 0u, NULL);

    for (index = 0; index < STARTUP_DEFAULT_COLUMN_COUNT; ++index) {
        assert_column_equal(generated[index], repeated[index]);
        assert(generated[index].x >= bounds.min_x);
        assert(generated[index].x <= bounds.max_x);
        assert(generated[index].z >= bounds.min_z);
        assert(generated[index].z <= bounds.max_z);
        assert(generated[index].height >= bounds.min_height);
        assert(generated[index].height <= bounds.max_height);
        assert(generated[index].radius == bounds.radius);
    }
}

static void test_default_collision_walls(void)
{
    const world_collision_walls walls = world_gen_default_collision_walls();

    assert(walls.block_min_x);
    assert(walls.block_max_x);
    assert(!walls.block_min_z);
    assert(walls.block_max_z);
}

static void test_rendered_walls_push_candidates_back_inside(void)
{
    const world_collision_walls walls = world_gen_default_collision_walls();
    const float player_radius = world_gen_player_collision_radius();
    float x = walls.min_x - player_radius - 3.0f;
    float z = 0.0f;

    world_gen_resolve_player_xz(&walls, NULL, 0u, player_radius, &x, &z);
    assert_float_equal(x, walls.min_x + player_radius);
    assert_float_equal(z, 0.0f);

    x = walls.max_x + player_radius + 3.0f;
    z = 0.0f;
    world_gen_resolve_player_xz(&walls, NULL, 0u, player_radius, &x, &z);
    assert_float_equal(x, walls.max_x - player_radius);
    assert_float_equal(z, 0.0f);

    x = 0.0f;
    z = walls.max_z + player_radius + 3.0f;
    world_gen_resolve_player_xz(&walls, NULL, 0u, player_radius, &x, &z);
    assert_float_equal(x, 0.0f);
    assert_float_equal(z, walls.max_z - player_radius);
}

static void test_min_z_wall_remains_non_blocking(void)
{
    const world_collision_walls walls = world_gen_default_collision_walls();
    const float player_radius = world_gen_player_collision_radius();
    float x = 2.0f;
    float z = walls.min_z - player_radius - 3.0f;
    const float initial_x = x;
    const float initial_z = z;

    world_gen_resolve_player_xz(&walls, NULL, 0u, player_radius, &x, &z);
    assert_float_equal(x, initial_x);
    assert_float_equal(z, initial_z);
}

static void test_column_collision_uses_square_footprint(void)
{
    const world_collision_walls walls = make_open_walls();
    const world_column column = { 0.0f, 0.0f, 4.0f, 1.0f };
    const float player_radius = world_gen_player_collision_radius();
    const float combined_radius = column.radius + player_radius;
    float x = column.x + combined_radius - 0.1f;
    float z = column.z + column.radius;
    const float distance_from_center_squared = (x * x) + (z * z);

    assert(distance_from_center_squared > (combined_radius * combined_radius));

    world_gen_resolve_player_xz(&walls, &column, 1u, player_radius, &x, &z);

    assert_float_equal(x, column.x + combined_radius);
    assert_float_equal(z, column.z + column.radius);
    assert_resolved_clear_of_column(column, player_radius, x, z);
}

static void test_exact_center_column_overlap_resolves_deterministically(void)
{
    const world_collision_walls walls = make_open_walls();
    const world_column column = { 0.0f, 0.0f, 3.0f, 1.0f };
    const float player_radius = world_gen_player_collision_radius();
    const float expected_x = column.x - column.radius - player_radius;
    float x_a = column.x;
    float z_a = column.z;
    float x_b = column.x;
    float z_b = column.z;

    world_gen_resolve_player_xz(&walls, &column, 1u, player_radius, &x_a, &z_a);
    world_gen_resolve_player_xz(&walls, &column, 1u, player_radius, &x_b, &z_b);

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
        { 0.0f, 0.0f, 3.0f, 1.0f },
        { 2.0f, 2.0f, 3.0f, 1.0f }
    };
    const float player_radius = world_gen_player_collision_radius();
    float x_a = 1.0f;
    float z_a = 1.0f;
    float x_b = 1.0f;
    float z_b = 1.0f;
    size_t index;

    world_gen_resolve_player_xz(&walls, columns, 2u, player_radius, &x_a, &z_a);
    world_gen_resolve_player_xz(&walls, columns, 2u, player_radius, &x_b, &z_b);

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
    const world_collision_walls walls = world_gen_default_collision_walls();
    const world_column column = { 0.0f, 0.0f, 2.0f, 1.0f };
    const float player_radius = world_gen_player_collision_radius();
    float x = 5.0f;
    float z = -5.0f;
    const float initial_x = x;
    const float initial_z = z;

    world_gen_resolve_player_xz(&walls, &column, 1u, player_radius, &x, &z);

    assert_float_equal(x, initial_x);
    assert_float_equal(z, initial_z);
}

static void test_xz_resolution_leaves_y_unchanged(void)
{
    const world_collision_walls walls = world_gen_default_collision_walls();
    const float player_radius = world_gen_player_collision_radius();
    seam_position candidate = { walls.max_x + player_radius + 2.0f, 2.25f, 0.25f };

    world_gen_resolve_player_xz(&walls, NULL, 0u, player_radius, &candidate.x, &candidate.z);

    assert_float_equal(candidate.x, walls.max_x - player_radius);
    assert_float_equal(candidate.y, 2.25f);
    assert_float_equal(candidate.z, 0.25f);
}

int main(void)
{
    test_startup_config();
    test_generation_defaults_and_determinism();
    test_default_collision_walls();
    test_rendered_walls_push_candidates_back_inside();
    test_min_z_wall_remains_non_blocking();
    test_column_collision_uses_square_footprint();
    test_exact_center_column_overlap_resolves_deterministically();
    test_multi_column_overlap_resolves_deterministically();
    test_valid_position_is_a_noop();
    test_xz_resolution_leaves_y_unchanged();

    return 0;
}
