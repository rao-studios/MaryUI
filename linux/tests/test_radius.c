/* radius.ts's tests, case for case. */
#include "lp_test.h"
#include "maryui/lp_radius.h"

static const lp_radius_params params = { 12, 8, 16, 6, 1100, 0.7f };

LP_TEST(rests_at_the_resting_radius_on_every_corner) {
    lp_corners c = lp_corner_targets(0, 0, params);
    lp_corners rest = lp_rest_corners(12);
    LP_ASSERT_NEAR(c.tl, rest.tl, 0);
    LP_ASSERT_NEAR(c.tr, rest.tr, 0);
    LP_ASSERT_NEAR(c.br, rest.br, 0);
    LP_ASSERT_NEAR(c.bl, rest.bl, 0);
}

LP_TEST(flattens_the_leading_corners_and_rounds_the_trailing_ones) {
    /* Travelling right: the right corners lead, the left corners trail. */
    lp_corners c = lp_corner_targets(params.velocity_ref, 0, params);
    LP_ASSERT(c.tr < params.rest);
    LP_ASSERT(c.br < params.rest);
    LP_ASSERT(c.tl > params.rest);
    LP_ASSERT(c.bl > params.rest);
    /* Pure horizontal travel treats top and bottom alike. */
    LP_ASSERT_NEAR(c.tr, c.br, 1e-6);
    LP_ASSERT_NEAR(c.tl, c.bl, 1e-6);
}

LP_TEST(picks_out_a_single_corner_on_a_diagonal_fling) {
    float v = params.velocity_ref;
    lp_corners c = lp_corner_targets(v, v, params);
    LP_ASSERT(c.br < fminf(c.tr, c.bl));
    LP_ASSERT(c.tl > fmaxf(c.tr, c.bl));
    /* The two side corners are neither leading nor trailing. */
    LP_ASSERT_NEAR(c.tr, params.rest, 1e-6);
    LP_ASSERT_NEAR(c.bl, params.rest, 1e-6);
}

LP_TEST(never_leaves_the_8_to_16px_range_however_hard_it_is_flung) {
    static const float v[4][2] = { { 1e6f, 0 }, { 0, -1e6f }, { -1e6f, 1e6f }, { 3e5f, -7e5f } };
    for (int i = 0; i < 4; i++) {
        lp_corners c = lp_corner_targets(v[i][0], v[i][1], params);
        for (int k = 0; k < LP_CORNER_COUNT; k++) {
            LP_ASSERT(lp_corner_at(c, k) >= params.min);
            LP_ASSERT(lp_corner_at(c, k) <= params.max);
        }
    }
}

LP_TEST(scales_with_speed_between_rest_and_full_travel) {
    lp_corners slow = lp_corner_targets(params.velocity_ref * 0.25f, 0, params);
    lp_corners fast = lp_corner_targets(params.velocity_ref, 0, params);
    LP_ASSERT(slow.tr > fast.tr);
    LP_ASSERT(slow.tr < params.rest);
}

LP_TEST(reports_liquidity_as_normalized_clamped_speed) {
    LP_ASSERT_NEAR(lp_liquidity(0, 0, 2500, 1), 0, 0);
    LP_ASSERT_NEAR(lp_liquidity(2500, 0, 2500, 1), 1, 1e-6);
    LP_ASSERT_NEAR(lp_liquidity(1e6f, 1e6f, 2500, 1), 1, 0);
    LP_ASSERT_NEAR(lp_liquidity(0, 1250, 2500, 1), 0.5, 1e-6);
}

LP_TEST(lifts_an_ordinary_drag_well_clear_of_rest) {
    /* A careful drag peaks near 400 px/s. Against the engine's 2500 px/s jelly
     * reference that spread the corners 1.6px of the 8 available; the corners
     * have their own, much lower reference precisely so this reads. */
    lp_corners careful = lp_corner_targets(400, 0, params);
    LP_ASSERT(careful.tr < 10);
    LP_ASSERT(careful.tl > 14);
}

LP_TEST(reaches_the_full_range_on_a_brisk_drag_not_only_on_a_fling) {
    lp_corners brisk = lp_corner_targets(1050, 0, params);
    LP_ASSERT_NEAR(brisk.tr, params.min, 1e-6);
    LP_ASSERT_NEAR(brisk.tl, params.max, 1e-6);
}

LP_TEST(curves_the_response_so_slow_speeds_are_not_crushed) {
    float quarter = params.rest - lp_corner_targets(params.velocity_ref * 0.25f, 0, params).tr;
    float full = params.rest - lp_corner_targets(params.velocity_ref, 0, params).tr;
    LP_ASSERT(quarter / full > 0.25f);
}

LP_TEST(detunes_the_four_corner_springs_away_from_each_other) {
    float f[LP_CORNER_COUNT];
    for (int i = 0; i < LP_CORNER_COUNT; i++) f[i] = lp_detuned_frequency(2.6f, i, 0.08f);
    for (int i = 0; i < LP_CORNER_COUNT; i++) {
        for (int j = i + 1; j < LP_CORNER_COUNT; j++) LP_ASSERT(f[i] != f[j]);
        /* The web asserts this to 1e-9; these are floats, so the slack is float epsilon. */
        LP_ASSERT(fabsf(f[i] - 2.6f) / 2.6f <= 0.08f + 1e-6f);
    }
}

int main(void) {
    LP_RUN(rests_at_the_resting_radius_on_every_corner);
    LP_RUN(flattens_the_leading_corners_and_rounds_the_trailing_ones);
    LP_RUN(picks_out_a_single_corner_on_a_diagonal_fling);
    LP_RUN(never_leaves_the_8_to_16px_range_however_hard_it_is_flung);
    LP_RUN(scales_with_speed_between_rest_and_full_travel);
    LP_RUN(reports_liquidity_as_normalized_clamped_speed);
    LP_RUN(lifts_an_ordinary_drag_well_clear_of_rest);
    LP_RUN(reaches_the_full_range_on_a_brisk_drag_not_only_on_a_fling);
    LP_RUN(curves_the_response_so_slow_speeds_are_not_crushed);
    LP_RUN(detunes_the_four_corner_springs_away_from_each_other);
    LP_TEST_MAIN_END();
}
