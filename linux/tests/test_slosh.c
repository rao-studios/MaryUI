/* slosh.ts's tests, case for case. */
#include "lp_test.h"
#include "maryui/lp_slosh.h"

/* The shipped tuning (motion.slosh-* in tokens.json). */
static const lp_slosh_params params = { 1.3f, 0.16f, 0.0009f, 18.0f, 0.7f, 0.5f, 12000 };
static const lp_slosh_drive still = { 0, 0 };
static const float TAU = 6.283185307179586f;

static int sign(float v) { return (v > 0) - (v < 0); }
static float stiffness(void) { return (TAU * params.frequency_hz) * (TAU * params.frequency_hz); }

/* |theta| after eight seconds of a constant shear. */
static float tilt(float shear) {
    lp_slosh_state s = lp_slosh_make();
    lp_slosh_drive d = { 0, shear };
    for (int i = 0; i < 120 * 8; i++) s = lp_slosh_step(s, d, 1.0f / 120, params);
    return fabsf(s.theta);
}

LP_TEST(settles_at_the_static_tilt_for_a_constant_acceleration) {
    lp_slosh_state s = lp_slosh_make();
    float accel = 4000;
    lp_slosh_drive d = { accel, 0 };
    for (int i = 0; i < 120 * 10; i++) s = lp_slosh_step(s, d, 1.0f / 120, params);
    LP_ASSERT_NEAR(s.theta, -accel * params.gain / stiffness(), 5e-5);
}

LP_TEST(holds_a_steady_lean_while_the_liquid_is_being_sheared) {
    /* A dragged window is mostly constant velocity, where accel is zero. Shear
     * is what keeps the surface tilted through the middle of the gesture. */
    lp_slosh_state s = lp_slosh_make();
    lp_slosh_drive d = { 0, 0.5f };
    for (int i = 0; i < 120 * 8; i++) s = lp_slosh_step(s, d, 1.0f / 120, params);
    LP_ASSERT_NEAR(s.theta, -powf(0.5f, params.shear_gamma) * params.shear_gain / stiffness(), 5e-5);
    LP_ASSERT(fabsf(s.theta) > 0.1f);
}

LP_TEST(lifts_slow_gestures_more_than_a_linear_response_would) {
    /* Quarter the shear, and gamma < 1 leaves well over a quarter of the tilt. */
    LP_ASSERT(tilt(0.25f) / tilt(1.0f) > 0.3f);
}

LP_TEST(rings_at_roughly_its_natural_period_once_released) {
    lp_slosh_state s = { 0.3f, 0 };
    float crossings[64];
    int n = 0;
    float dt = 1.0f / 240, prev = s.theta;
    for (float t = 0; t < 2; t += dt) {
        s = lp_slosh_step(s, still, dt, params);
        if (sign(s.theta) != sign(prev) && prev != 0 && n < 64) crossings[n++] = t;
        prev = s.theta;
    }
    LP_ASSERT(n >= 4);
    float half = crossings[1] - crossings[0];
    LP_ASSERT_NEAR(half, 1 / (2 * params.frequency_hz), 0.05);
}

LP_TEST(decays_to_rest_and_reports_settled) {
    lp_slosh_state s = { 0.4f, 0 };
    for (int i = 0; i < 120 * 8; i++) s = lp_slosh_step(s, still, 1.0f / 120, params);
    LP_ASSERT(lp_slosh_settled(s, LP_SLOSH_TOLERANCE, LP_SLOSH_VELOCITY_TOLERANCE));
}

LP_TEST(never_exceeds_the_rim_and_never_produces_nan_at_coarse_steps) {
    lp_slosh_state s = lp_slosh_make();
    for (int i = 0; i < 200; i++) {
        /* The shear is deliberately far out of range: the clamp is what keeps
         * a fractional power of a negative number from producing NaN. */
        lp_slosh_drive d = { i % 2 ? 1e6f : -1e6f, i % 2 ? 40.0f : -40.0f };
        s = lp_slosh_step(s, d, 1.0f / 30, params);
        LP_ASSERT(isfinite(s.theta));
        LP_ASSERT(fabsf(s.theta) <= params.max_angle);
    }
}

int main(void) {
    LP_RUN(settles_at_the_static_tilt_for_a_constant_acceleration);
    LP_RUN(holds_a_steady_lean_while_the_liquid_is_being_sheared);
    LP_RUN(lifts_slow_gestures_more_than_a_linear_response_would);
    LP_RUN(rings_at_roughly_its_natural_period_once_released);
    LP_RUN(decays_to_rest_and_reports_settled);
    LP_RUN(never_exceeds_the_rim_and_never_produces_nan_at_coarse_steps);
    LP_TEST_MAIN_END();
}
