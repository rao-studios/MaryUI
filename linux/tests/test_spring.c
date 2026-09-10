/* spring.ts's tests, case for case. */
#include "lp_test.h"
#include "maryui/lp_spring.h"

static lp_spring run(lp_spring s, float seconds, lp_spring_params p) {
    float dt = 1.0f / 120;
    for (float t = 0; t < seconds; t += dt) lp_spring_step(&s, dt, p);
    return s;
}

LP_TEST(converges_to_its_target) {
    lp_spring s = run(lp_spring_make(0, 1), 3, (lp_spring_params){ 2, 0.5f });
    LP_ASSERT_NEAR(s.value, 1, 5e-4);
    LP_ASSERT(lp_spring_settled(&s, LP_SPRING_TOLERANCE, LP_SPRING_VELOCITY_TOLERANCE));
}

LP_TEST(overshoots_when_underdamped_and_not_when_critically_damped) {
    lp_spring under = lp_spring_make(0, 1), critical = lp_spring_make(0, 1);
    float under_max = 0, critical_max = 0, dt = 1.0f / 120;
    for (float t = 0; t < 2; t += dt) {
        lp_spring_step(&under, dt, (lp_spring_params){ 2, 0.3f });
        if (under.value > under_max) under_max = under.value;
        lp_spring_step(&critical, dt, (lp_spring_params){ 2, 1 });
        if (critical.value > critical_max) critical_max = critical.value;
    }
    LP_ASSERT(under_max > 1.05f);
    LP_ASSERT(critical_max <= 1.001f);
}

LP_TEST(stays_stable_at_a_30_fps_step_for_the_fastest_system_spring) {
    lp_spring s = lp_spring_make(0, 1);
    for (int i = 0; i < 300; i++) lp_spring_step(&s, 1.0f / 30, (lp_spring_params){ 2.3f, 0.8f });
    LP_ASSERT(isfinite(s.value));
    LP_ASSERT_NEAR(s.value, 1, 5e-3);
}

LP_TEST(snaps_without_residual_velocity) {
    lp_spring s = lp_spring_make(0, 5);
    lp_spring_snap(&s);
    LP_ASSERT_EQ(s.value, 5);
    LP_ASSERT_EQ(s.velocity, 0);
}

LP_TEST(follows_without_ever_overshooting_unlike_a_spring) {
    /* The grain's case: a spring with this settle time springs back past the
     * target when the drag stops, which reads as elastic rather than dragged. */
    float v = 0, max_v = 0;
    for (int i = 0; i < 240; i++) {
        v = lp_follow(v, 10, 1.0f / 120, 90);
        if (v > max_v) max_v = v;
    }
    LP_ASSERT(max_v <= 10.0f);
    LP_ASSERT_NEAR(v, 10, 1e-4);
}

LP_TEST(closes_63_percent_of_the_gap_in_one_time_constant) {
    float v = lp_follow(0, 1, 0.09f, 90);
    LP_ASSERT_NEAR(v, 1 - expf(-1.0f), 1e-6);
    /* Frame-rate independent: two half steps equal one whole one. */
    float half = lp_follow(lp_follow(0, 1, 0.045f, 90), 1, 0.045f, 90);
    LP_ASSERT_NEAR(half, v, 1e-6);
}

int main(void) {
    LP_RUN(converges_to_its_target);
    LP_RUN(overshoots_when_underdamped_and_not_when_critically_damped);
    LP_RUN(stays_stable_at_a_30_fps_step_for_the_fastest_system_spring);
    LP_RUN(snaps_without_residual_velocity);
    LP_RUN(follows_without_ever_overshooting_unlike_a_spring);
    LP_RUN(closes_63_percent_of_the_gap_in_one_time_constant);
    LP_TEST_MAIN_END();
}
