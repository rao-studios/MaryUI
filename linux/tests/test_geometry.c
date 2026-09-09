/* geometry.ts's tests, case for case. */
#include "lp_test.h"
#include "maryui/lp_geometry.h"

static const lp_rect bounds = { 0, 24, 1200, 776 };
static const lp_size min = { 240, 160 };

LP_TEST(never_lets_the_title_bar_rise_above_the_desktop) {
    LP_ASSERT_NEAR(lp_clamp_to_bounds(LP_RECT(100, -50, 400, 300), bounds, 0, 0).y, 24, 0);
}

LP_TEST(keeps_at_least_40px_visible_horizontally_on_both_sides) {
    LP_ASSERT_NEAR(lp_clamp_to_bounds(LP_RECT(-1000, 100, 400, 300), bounds, 0, 0).x, -360, 0);
    LP_ASSERT_NEAR(lp_clamp_to_bounds(LP_RECT(5000, 100, 400, 300), bounds, 0, 0).x, 1160, 0);
}

LP_TEST(keeps_the_title_bar_reachable_at_the_bottom) {
    LP_ASSERT_NEAR(lp_clamp_to_bounds(LP_RECT(0, 5000, 400, 300), bounds, 0, 0).y, 24 + 776 - 28, 0);
}

LP_TEST(returns_the_same_rect_when_nothing_changes) {
    lp_rect r = LP_RECT(100, 100, 400, 300);
    LP_ASSERT(lp_rects_equal(lp_clamp_to_bounds(r, bounds, 0, 0), r));
}

LP_TEST(moves_only_the_edges_named_by_the_handle) {
    lp_rect rect = LP_RECT(100, 100, 400, 300);
    lp_rect e = lp_resize_from_handle(rect, LP_HANDLE_E, 50, 999, min, NULL);
    LP_ASSERT(lp_rects_equal(e, LP_RECT(100, 100, 450, 300)));
    lp_rect n = lp_resize_from_handle(rect, LP_HANDLE_N, 999, -20, min, NULL);
    LP_ASSERT(lp_rects_equal(n, LP_RECT(100, 80, 400, 320)));
    lp_rect sw = lp_resize_from_handle(rect, LP_HANDLE_SW, -10, 10, min, NULL);
    LP_ASSERT(lp_rects_equal(sw, LP_RECT(90, 100, 410, 310)));
}

LP_TEST(enforces_the_minimum_size_from_every_handle) {
    lp_rect rect = LP_RECT(100, 100, 400, 300);
    for (int h = 0; h < LP_HANDLE_COUNT; h++) {
        int west = h == LP_HANDLE_W || h == LP_HANDLE_NW || h == LP_HANDLE_SW;
        int north = h == LP_HANDLE_N || h == LP_HANDLE_NE || h == LP_HANDLE_NW;
        lp_rect shrunk = lp_resize_from_handle(rect, (enum lp_resize_handle)h, west ? 1000 : -1000, north ? 1000 : -1000, min, NULL);
        LP_ASSERT(shrunk.w >= min.w);
        LP_ASSERT(shrunk.h >= min.h);
    }
}

LP_TEST(pins_the_opposite_edge_while_shrinking) {
    lp_rect shrunk = lp_resize_from_handle(LP_RECT(100, 100, 400, 300), LP_HANDLE_W, 1000, 0, min, NULL);
    LP_ASSERT_NEAR(shrunk.x + shrunk.w, 500, 0);
    LP_ASSERT_NEAR(shrunk.w, min.w, 0);
}

LP_TEST(does_not_push_edges_outside_the_bounds) {
    lp_rect rect = LP_RECT(100, 100, 400, 300);
    lp_rect grown = lp_resize_from_handle(rect, LP_HANDLE_NW, -500, -500, min, &bounds);
    LP_ASSERT_NEAR(grown.x, 0, 0);
    LP_ASSERT_NEAR(grown.y, 24, 0);
    lp_rect se = lp_resize_from_handle(rect, LP_HANDLE_SE, 5000, 5000, min, &bounds);
    LP_ASSERT_NEAR(se.x + se.w, 1200, 0);
    LP_ASSERT_NEAR(se.y + se.h, 800, 0);
}

LP_TEST(shrinks_and_shifts_oversize_windows_into_the_bounds) {
    lp_rect r = lp_constrain_rect(LP_RECT(900, 700, 2000, 2000), bounds, min);
    LP_ASSERT(lp_rects_equal(r, LP_RECT(0, 24, 1200, 776)));
}

LP_TEST(maps_the_destination_rect_back_onto_the_source) {
    lp_flip t = lp_flip_transform(LP_RECT(10, 20, 200, 100), LP_RECT(0, 24, 400, 400));
    LP_ASSERT_NEAR(t.tx, 10, 0); LP_ASSERT_NEAR(t.ty, -4, 0);
    LP_ASSERT_NEAR(t.sx, 0.5, 1e-6); LP_ASSERT_NEAR(t.sy, 0.25, 1e-6);
}

int main(void) {
    LP_RUN(never_lets_the_title_bar_rise_above_the_desktop);
    LP_RUN(keeps_at_least_40px_visible_horizontally_on_both_sides);
    LP_RUN(keeps_the_title_bar_reachable_at_the_bottom);
    LP_RUN(returns_the_same_rect_when_nothing_changes);
    LP_RUN(moves_only_the_edges_named_by_the_handle);
    LP_RUN(enforces_the_minimum_size_from_every_handle);
    LP_RUN(pins_the_opposite_edge_while_shrinking);
    LP_RUN(does_not_push_edges_outside_the_bounds);
    LP_RUN(shrinks_and_shifts_oversize_windows_into_the_bounds);
    LP_RUN(maps_the_destination_rect_back_onto_the_source);
    LP_TEST_MAIN_END();
}
