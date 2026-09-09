/* The motion engine and the per-window target (motionEngine.ts has no unit
 * tests of its own; these pin the behaviour the compositor relies on). */
#include "lp_test.h"
#include "maryui/lp_motion.h"
#include "maryui/lp_tokens.h"

/* MARK: - Easing */

LP_TEST(cubic_bezier_linear_is_the_identity) {
    lp_cubic_bezier linear = { 0, 0, 1, 1 };
    for (int i = 0; i <= 10; i++) LP_ASSERT_NEAR(lp_cubic_bezier_eval(linear, i / 10.0f), i / 10.0f, 1e-3);
}

LP_TEST(cubic_bezier_ease_out_starts_fast_ends_at_one_and_never_reverses) {
    lp_cubic_bezier ease = LP_MOTION_EASE_OUT;
    LP_ASSERT_NEAR(lp_cubic_bezier_eval(ease, 0), 0, 1e-6);
    LP_ASSERT_NEAR(lp_cubic_bezier_eval(ease, 1), 1, 1e-6);
    LP_ASSERT(lp_cubic_bezier_eval(ease, 0.25f) > 0.5f);
    float prev = 0;
    for (int i = 1; i <= 100; i++) {
        float v = lp_cubic_bezier_eval(ease, i / 100.0f);
        LP_ASSERT(v >= prev - 1e-5f);
        prev = v;
    }
}

/* MARK: - Engine */

static int wake_calls = 0;
static void count_wake(void *user) { (*(int *)user)++; }

typedef struct { lp_motion_target target; int remaining; float last_dt; } counting_target;
static int counting_step(lp_motion_target *t, float dt, double now_ms) {
    counting_target *c = t->user;
    c->last_dt = dt;
    return c->remaining > 0 ? c->remaining-- > 0 : 0;
}

LP_TEST(engine_wakes_once_until_the_loop_goes_idle) {
    lp_motion_engine e;
    wake_calls = 0;
    lp_motion_engine_init(&e, count_wake, &wake_calls);
    counting_target c = { .target = { .step = counting_step }, .remaining = 3 };
    c.target.user = &c;
    lp_motion_engine_add(&e, &c.target);
    LP_ASSERT_EQ(lp_motion_engine_tick(&e, 0), 0); /* idle: nothing runs before a wake */
    lp_motion_engine_wake(&e, 0);
    lp_motion_engine_wake(&e, 1);
    LP_ASSERT_EQ(wake_calls, 1);
    LP_ASSERT_EQ(lp_motion_engine_tick(&e, 16), 1);
    LP_ASSERT_EQ(lp_motion_engine_tick(&e, 32), 1);
    LP_ASSERT_EQ(lp_motion_engine_tick(&e, 48), 1);
    LP_ASSERT_EQ(lp_motion_engine_tick(&e, 64), 0);
    LP_ASSERT(!e.running);
    LP_ASSERT_EQ(e.idles, 1u);
    lp_motion_engine_wake(&e, 80);
    LP_ASSERT_EQ(wake_calls, 2);
    lp_motion_engine_remove(&e, &c.target);
    LP_ASSERT(e.targets == NULL);
    LP_ASSERT_EQ(lp_motion_engine_tick(&e, 96), 0);
}

LP_TEST(engine_clamps_dt_to_a_30_fps_step) {
    lp_motion_engine e;
    lp_motion_engine_init(&e, NULL, NULL);
    counting_target c = { .target = { .step = counting_step }, .remaining = 5 };
    c.target.user = &c;
    lp_motion_engine_add(&e, &c.target);
    lp_motion_engine_wake(&e, 0);
    lp_motion_engine_tick(&e, 500);
    LP_ASSERT_NEAR(c.last_dt, 1.0 / 30, 1e-6);
    lp_motion_engine_tick(&e, 508);
    LP_ASSERT_NEAR(c.last_dt, 0.008, 1e-6);
}

/* MARK: - Window motion */

static const lp_rect rect = { 150, 100, 400, 300 };

static int settle(lp_window_motion *m, double *now, int reduced) {
    for (int i = 0; i < 60 * 10; i++) {
        *now += 1000.0 / 60;
        if (!lp_window_motion_step(m, 1.0f / 60, *now, rect, 1000, &lp_motion_live, reduced)) return i;
    }
    return -1;
}

LP_TEST(the_sheen_follows_the_room_light) {
    lp_window_motion m;
    lp_window_motion_init(&m);
    double now = 0;
    LP_ASSERT(settle(&m, &now, 0) >= 0);
    /* light 0.35 × 1000 = 350; (350 - 150) / 400 = 0.5 */
    LP_ASSERT_NEAR(m.out.sheen_x, 0.5, 1e-3);
    LP_ASSERT(m.out.identity);
}

LP_TEST(dragging_fast_tilts_the_sheen_and_stretches_the_jelly) {
    lp_window_motion m;
    lp_window_motion_init(&m);
    double now = 0;
    settle(&m, &now, 0);
    lp_window_motion_begin_drag(&m, 200, 14);
    LP_ASSERT_NEAR(m.origin_x, 200, 0);
    for (int i = 1; i <= 20; i++) {
        now += 16;
        lp_window_motion_move_drag(&m, i * 32.0f, 0, now, 350 + i * 32.0f, 114); /* 2000 px/s */
        LP_ASSERT_EQ(lp_window_motion_step(&m, 0.016f, now + 1, rect, 1000, &lp_motion_live, 0), 1);
    }
    LP_ASSERT(m.out.vx > 0.5f);
    LP_ASSERT(m.out.tilt_deg > 0.5f);
    LP_ASSERT(m.out.jelly_sx > 1.005f);
    LP_ASSERT(m.out.jelly_sy < 0.999f);
    LP_ASSERT_NEAR(m.out.tx, 640, 0);
    LP_ASSERT(!m.out.identity);
    LP_ASSERT(fabsf(m.out.slosh_deg) > 0.01f);
}

LP_TEST(a_released_drag_settles_to_the_identity_transform) {
    lp_window_motion m;
    lp_window_motion_init(&m);
    double now = 0;
    settle(&m, &now, 0);
    lp_window_motion_begin_drag(&m, 200, 14);
    for (int i = 1; i <= 20; i++) {
        now += 16;
        lp_window_motion_move_drag(&m, i * 32.0f, 0, now, 350 + i * 32.0f, 114);
        lp_window_motion_step(&m, 0.016f, now + 1, rect, 1000, &lp_motion_live, 0);
    }
    lp_window_motion_end_drag(&m);
    LP_ASSERT_NEAR(m.out.tx, 0, 0);
    LP_ASSERT(!m.out.identity); /* the jelly is still relaxing */
    int frames = settle(&m, &now, 0);
    LP_ASSERT(frames > 5 && frames < 60 * 8);
    LP_ASSERT(m.out.identity);
    LP_ASSERT_NEAR(m.out.sx, 1, 0);
    LP_ASSERT_NEAR(m.out.tilt_deg, 0, 0.005);
    LP_ASSERT_NEAR(lp_window_motion_velocity_x(&m), 0, 0);
}

LP_TEST(a_flight_springs_home_from_the_previous_rect) {
    lp_window_motion m;
    lp_window_motion_init(&m);
    double now = 0;
    settle(&m, &now, 0);
    lp_window_motion_fly_from(&m, LP_RECT(100, 100, 400, 300), LP_RECT(0, 24, 1200, 776), 0);
    LP_ASSERT(m.flying);
    LP_ASSERT_NEAR(m.out.tx, 100, 1e-4);
    LP_ASSERT_NEAR(m.out.ty, 76, 1e-4);
    LP_ASSERT_NEAR(m.out.fly_sx, 400.0 / 1200, 1e-5);
    LP_ASSERT_NEAR(m.out.origin_x, 0, 0);
    now += 1000.0 / 60;
    LP_ASSERT_EQ(lp_window_motion_step(&m, 1.0f / 60, now, LP_RECT(0, 24, 1200, 776), 1000, &lp_motion_live, 0), 1);
    LP_ASSERT(m.out.tx < 100 && m.out.tx > 0);
    int frames = settle(&m, &now, 0);
    LP_ASSERT(frames > 10 && frames < 60 * 4);
    LP_ASSERT(!m.flying);
    LP_ASSERT(m.out.identity);
}

LP_TEST(reduced_motion_snaps_everything) {
    lp_window_motion m;
    lp_window_motion_init(&m);
    lp_window_motion_fly_from(&m, LP_RECT(100, 100, 400, 300), rect, 1);
    LP_ASSERT(!m.flying);
    lp_window_motion_begin_drag(&m, 10, 10);
    lp_window_motion_move_drag(&m, 300, 0, 16, 400, 100);
    LP_ASSERT_EQ(lp_window_motion_step(&m, 0.016f, 17, rect, 1000, &lp_motion_live, 1), 1); /* dragging keeps the loop alive */
    LP_ASSERT_NEAR(m.out.jelly_sx, 1, 0);
    LP_ASSERT_NEAR(m.out.tilt_deg, 0, 0);
    LP_ASSERT_NEAR(m.out.tx, 300, 0);
    lp_window_motion_end_drag(&m);
    LP_ASSERT_EQ(lp_window_motion_step(&m, 0.016f, 33, rect, 1000, &lp_motion_live, 1), 0);
    LP_ASSERT(m.out.identity);
    LP_ASSERT_NEAR(m.out.sheen_x, 0.5, 1e-6); /* snapped straight to the light */
}

int main(void) {
    LP_RUN(cubic_bezier_linear_is_the_identity);
    LP_RUN(cubic_bezier_ease_out_starts_fast_ends_at_one_and_never_reverses);
    LP_RUN(engine_wakes_once_until_the_loop_goes_idle);
    LP_RUN(engine_clamps_dt_to_a_30_fps_step);
    LP_RUN(the_sheen_follows_the_room_light);
    LP_RUN(dragging_fast_tilts_the_sheen_and_stretches_the_jelly);
    LP_RUN(a_released_drag_settles_to_the_identity_transform);
    LP_RUN(a_flight_springs_home_from_the_previous_rect);
    LP_RUN(reduced_motion_snaps_everything);
    LP_TEST_MAIN_END();
}
