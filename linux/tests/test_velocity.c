/* velocity.ts's PointerTracker tests, case for case. */
#include "lp_test.h"
#include "maryui/lp_velocity.h"

LP_TEST(reports_the_velocity_of_steady_motion_once_the_ema_has_warmed_up) {
    lp_pointer_tracker tracker = { 0 };
    lp_motion_sample result = { 0 };
    for (int i = 0; i <= 30; i++) {
        double t = i * 16;
        lp_pointer_tracker_push(&tracker, t, (float)(i * 8), 0); /* 500 px/s */
        result = lp_pointer_tracker_sample(&tracker, t + 1);
    }
    LP_ASSERT_NEAR(result.vx, 500, 0.5);
    LP_ASSERT_EQ(result.vy, 0);
}

LP_TEST(produces_acceleration_when_velocity_changes_and_zero_once_steady) {
    lp_pointer_tracker tracker = { 0 };
    float peak = 0;
    lp_motion_sample last = { 0 };
    for (int i = 0; i <= 40; i++) {
        double t = i * 16;
        lp_pointer_tracker_push(&tracker, t, (float)(i * 8), 0);
        last = lp_pointer_tracker_sample(&tracker, t + 1);
        if (fabsf(last.ax) > peak) peak = fabsf(last.ax);
    }
    LP_ASSERT(peak > 1000);
    LP_ASSERT(fabsf(last.ax) < 1);
}

LP_TEST(decays_to_rest_when_samples_stop_arriving) {
    lp_pointer_tracker tracker = { 0 };
    for (int i = 0; i <= 10; i++) {
        lp_pointer_tracker_push(&tracker, i * 16, (float)(i * 8), 0);
        lp_pointer_tracker_sample(&tracker, i * 16 + 1);
    }
    lp_motion_sample result = lp_pointer_tracker_sample(&tracker, 400);
    for (int f = 1; f < 20; f++) result = lp_pointer_tracker_sample(&tracker, 400 + f * 16);
    LP_ASSERT(fabsf(result.vx) < 1);
}

LP_TEST(resets_cleanly) {
    lp_pointer_tracker tracker = { 0 };
    lp_pointer_tracker_push(&tracker, 0, 0, 0);
    lp_pointer_tracker_push(&tracker, 16, 100, 0);
    lp_pointer_tracker_sample(&tracker, 17);
    lp_pointer_tracker_reset(&tracker);
    LP_ASSERT(!lp_pointer_tracker_has_samples(&tracker));
    lp_motion_sample s = lp_pointer_tracker_sample(&tracker, 40);
    LP_ASSERT(s.vx == 0 && s.vy == 0 && s.ax == 0 && s.ay == 0);
}

int main(void) {
    LP_RUN(reports_the_velocity_of_steady_motion_once_the_ema_has_warmed_up);
    LP_RUN(produces_acceleration_when_velocity_changes_and_zero_once_steady);
    LP_RUN(decays_to_rest_when_samples_stop_arriving);
    LP_RUN(resets_cleanly);
    LP_TEST_MAIN_END();
}
