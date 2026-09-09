/* slosh.ts's tests, case for case. */
#include "lp_test.h"
#include "maryui/lp_slosh.h"

static const lp_slosh_params params = { 1.7f, 0.14f, 0.00005f, 0.5f, 12000 };
static const float TAU = 6.283185307179586f;

static int sign(float v) { return (v > 0) - (v < 0); }

LP_TEST(settles_at_the_static_tilt_for_a_constant_acceleration) {
    lp_slosh_state s = lp_slosh_make();
    float ax = 4000;
    for (int i = 0; i < 120 * 10; i++) s = lp_slosh_step(s, ax, 1.0f / 120, params);
    float k = (TAU * params.frequency_hz) * (TAU * params.frequency_hz);
    LP_ASSERT_NEAR(s.theta, -ax * params.gain / k, 5e-5);
}

LP_TEST(rings_at_roughly_its_natural_period_once_released) {
    lp_slosh_state s = { 0.3f, 0 };
    float crossings[64];
    int n = 0;
    float dt = 1.0f / 240, prev = s.theta;
    for (float t = 0; t < 2; t += dt) {
        s = lp_slosh_step(s, 0, dt, params);
        if (sign(s.theta) != sign(prev) && prev != 0 && n < 64) crossings[n++] = t;
        prev = s.theta;
    }
    LP_ASSERT(n >= 4);
    float half = crossings[1] - crossings[0];
    LP_ASSERT_NEAR(half, 1 / (2 * params.frequency_hz), 0.05);
}

LP_TEST(decays_to_rest_and_reports_settled) {
    lp_slosh_state s = { 0.4f, 0 };
    for (int i = 0; i < 120 * 8; i++) s = lp_slosh_step(s, 0, 1.0f / 120, params);
    LP_ASSERT(lp_slosh_settled(s, LP_SLOSH_TOLERANCE, LP_SLOSH_VELOCITY_TOLERANCE));
}

LP_TEST(never_exceeds_the_rim_and_never_produces_nan_at_coarse_steps) {
    lp_slosh_state s = lp_slosh_make();
    for (int i = 0; i < 200; i++) {
        s = lp_slosh_step(s, i % 2 ? 1e6f : -1e6f, 1.0f / 30, params);
        LP_ASSERT(isfinite(s.theta));
        LP_ASSERT(fabsf(s.theta) <= params.max_angle);
    }
}

int main(void) {
    LP_RUN(settles_at_the_static_tilt_for_a_constant_acceleration);
    LP_RUN(rings_at_roughly_its_natural_period_once_released);
    LP_RUN(decays_to_rest_and_reports_settled);
    LP_RUN(never_exceeds_the_rim_and_never_produces_nan_at_coarse_steps);
    LP_TEST_MAIN_END();
}
