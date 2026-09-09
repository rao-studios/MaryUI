/* reducer.test.ts, case for case. */
#include <math.h>
#include "lp_test.h"
#include "maryui/lp_geometry.h"
#include "maryui/lp_wm.h"

static const lp_rect bounds = { 0, 24, 1200, 776 };

static void open(lp_wm_state *s, const char *app, const lp_rect *rect) {
    lp_wm_action a = { .type = LP_WM_OPEN, .spec = lp_open_spec_default(app, app) };
    if (rect) a.spec.rect = *rect;
    lp_wm_reduce(s, &a);
}
static uint64_t act(lp_wm_state *s, enum lp_wm_action_type t, const char *id) {
    lp_wm_action a = { .type = t, .id = id };
    return lp_wm_reduce(s, &a);
}
static const lp_window_record *win(const lp_wm_state *s, const char *id) {
    int i = lp_wm_find(s, id);
    return i >= 0 ? &s->windows[i] : NULL;
}

LP_TEST(opens_windows_with_increasing_z_and_focuses_the_newest) {
    lp_wm_state s; lp_wm_init(&s, bounds);
    open(&s, "finder", NULL); open(&s, "gallery", NULL);
    LP_ASSERT_EQ(s.count, 2);
    LP_ASSERT_STR(s.windows[0].id, "w1"); LP_ASSERT_STR(s.windows[1].id, "w2");
    LP_ASSERT_STR(lp_wm_focused(&s)->id, "w2");
    LP_ASSERT(win(&s, "w2")->z > win(&s, "w1")->z);
}

LP_TEST(reuses_a_singleton_window_and_un_shades_it) {
    lp_wm_state s; lp_wm_init(&s, bounds);
    open(&s, "about", NULL);
    act(&s, LP_WM_TOGGLE_SHADE, "w1");
    lp_wm_action a = { .type = LP_WM_OPEN, .spec = lp_open_spec_default("about", "About") };
    a.spec.singleton = 1;
    lp_wm_reduce(&s, &a);
    LP_ASSERT_EQ(s.count, 1);
    LP_ASSERT_EQ(win(&s, "w1")->state, LP_WIN_NORMAL);
    LP_ASSERT_STR(lp_wm_focused(&s)->id, "w1");
}

LP_TEST(focus_raises_with_a_strictly_higher_z_and_keeps_other_records_identical) {
    lp_wm_state s; lp_wm_init(&s, bounds);
    open(&s, "finder", NULL); open(&s, "gallery", NULL);
    lp_window_record before = *win(&s, "w2");
    uint64_t changed = act(&s, LP_WM_FOCUS, "w1");
    LP_ASSERT(win(&s, "w1")->z > win(&s, "w2")->z);
    LP_ASSERT(memcmp(&before, win(&s, "w2"), sizeof before) == 0);
    LP_ASSERT(changed & 1);
    LP_ASSERT(!(changed & 2));
    LP_ASSERT_EQ(act(&s, LP_WM_FOCUS, "w1"), 0);
}

LP_TEST(move_keeps_the_title_bar_inside_the_bounds) {
    lp_wm_state s; lp_wm_init(&s, bounds);
    lp_rect r = LP_RECT(100, 100, 400, 300);
    open(&s, "finder", &r);
    lp_wm_action a = { .type = LP_WM_MOVE, .id = "w1", .x = -900, .y = -100 };
    lp_wm_reduce(&s, &a);
    LP_ASSERT_NEAR(win(&s, "w1")->rect.y, 24, 0);
    LP_ASSERT_NEAR(win(&s, "w1")->rect.x, -360, 0);
}

LP_TEST(resize_enforces_the_minimum_size) {
    lp_wm_state s; lp_wm_init(&s, bounds);
    open(&s, "finder", NULL);
    lp_wm_action a = { .type = LP_WM_RESIZE, .id = "w1", .rect = LP_RECT(10, 30, 10, 10) };
    lp_wm_reduce(&s, &a);
    LP_ASSERT_NEAR(win(&s, "w1")->rect.w, 240, 0);
    LP_ASSERT_NEAR(win(&s, "w1")->rect.h, 160, 0);
}

LP_TEST(toggle_zoom_fills_the_desktop_and_restores_the_exact_previous_rect) {
    lp_wm_state s; lp_wm_init(&s, bounds);
    lp_rect r = LP_RECT(100, 100, 400, 300);
    open(&s, "finder", &r);
    lp_rect original = win(&s, "w1")->rect;
    act(&s, LP_WM_TOGGLE_ZOOM, "w1");
    LP_ASSERT_EQ(win(&s, "w1")->state, LP_WIN_ZOOMED);
    LP_ASSERT(lp_rects_equal(win(&s, "w1")->rect, bounds));
    LP_ASSERT(win(&s, "w1")->has_prev && lp_rects_equal(win(&s, "w1")->prev_rect, original));
    act(&s, LP_WM_TOGGLE_ZOOM, "w1");
    LP_ASSERT_EQ(win(&s, "w1")->state, LP_WIN_NORMAL);
    LP_ASSERT(lp_rects_equal(win(&s, "w1")->rect, original));
    LP_ASSERT(!win(&s, "w1")->has_prev);
}

LP_TEST(toggle_shade_keeps_the_rect_and_toggles_back) {
    lp_wm_state s; lp_wm_init(&s, bounds);
    lp_rect r = LP_RECT(100, 100, 400, 300);
    open(&s, "finder", &r);
    lp_rect rect = win(&s, "w1")->rect;
    act(&s, LP_WM_TOGGLE_SHADE, "w1");
    LP_ASSERT_EQ(win(&s, "w1")->state, LP_WIN_SHADED);
    LP_ASSERT(lp_rects_equal(win(&s, "w1")->rect, rect));
    lp_wm_state moved = s;
    lp_wm_action a = { .type = LP_WM_MOVE, .id = "w1", .x = 200, .y = 200 };
    lp_wm_reduce(&moved, &a);
    LP_ASSERT(lp_rects_equal(win(&moved, "w1")->rect, LP_RECT(200, 200, rect.w, rect.h)));
    act(&s, LP_WM_TOGGLE_SHADE, "w1");
    LP_ASSERT_EQ(win(&s, "w1")->state, LP_WIN_NORMAL);
}

LP_TEST(close_hands_focus_to_the_top_most_remaining_window) {
    lp_wm_state s; lp_wm_init(&s, bounds);
    open(&s, "finder", NULL); open(&s, "gallery", NULL); open(&s, "about", NULL);
    act(&s, LP_WM_FOCUS, "w1");
    act(&s, LP_WM_CLOSE, "w1");
    LP_ASSERT_EQ(s.count, 2);
    LP_ASSERT_STR(s.windows[0].id, "w2"); LP_ASSERT_STR(s.windows[1].id, "w3");
    LP_ASSERT_STR(lp_wm_focused(&s)->id, "w3");
}

LP_TEST(focus_next_cycles_to_the_bottom_most_window) {
    lp_wm_state s; lp_wm_init(&s, bounds);
    open(&s, "finder", NULL); open(&s, "gallery", NULL);
    act(&s, LP_WM_FOCUS_NEXT, NULL);
    LP_ASSERT_STR(lp_wm_focused(&s)->id, "w1");
    act(&s, LP_WM_FOCUS_NEXT, NULL);
    LP_ASSERT_STR(lp_wm_focused(&s)->id, "w2");
}

LP_TEST(set_bounds_refits_zoomed_windows_and_re_clamps_the_rest) {
    lp_wm_state s; lp_wm_init(&s, bounds);
    lp_rect r = LP_RECT(900, 600, 400, 300);
    open(&s, "finder", &r); open(&s, "gallery", NULL);
    act(&s, LP_WM_TOGGLE_ZOOM, "w2");
    lp_rect small = LP_RECT(0, 24, 800, 500);
    lp_wm_action a = { .type = LP_WM_SET_BOUNDS, .bounds = small };
    lp_wm_reduce(&s, &a);
    LP_ASSERT(lp_rects_equal(win(&s, "w2")->rect, small));
    lp_rect w1 = win(&s, "w1")->rect;
    LP_ASSERT(w1.x + w1.w <= 800);
    LP_ASSERT(w1.y + w1.h <= 524);
}

int main(void) {
    LP_RUN(opens_windows_with_increasing_z_and_focuses_the_newest);
    LP_RUN(reuses_a_singleton_window_and_un_shades_it);
    LP_RUN(focus_raises_with_a_strictly_higher_z_and_keeps_other_records_identical);
    LP_RUN(move_keeps_the_title_bar_inside_the_bounds);
    LP_RUN(resize_enforces_the_minimum_size);
    LP_RUN(toggle_zoom_fills_the_desktop_and_restores_the_exact_previous_rect);
    LP_RUN(toggle_shade_keeps_the_rect_and_toggles_back);
    LP_RUN(close_hands_focus_to_the_top_most_remaining_window);
    LP_RUN(focus_next_cycles_to_the_bottom_most_window);
    LP_RUN(set_bounds_refits_zoomed_windows_and_re_clamps_the_rest);
    LP_TEST_MAIN_END();
}
