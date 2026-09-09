/* Layout helpers: cuts, centring, flex rows and columns. */
#include "lp_test.h"
#include "maryui/lp_ui.h"

LP_TEST(cuts_take_from_the_edges) {
    lp_rect a = LP_RECT(0, 0, 100, 50);
    lp_rect top = lp_rect_cut_top(&a, 10);
    LP_ASSERT_NEAR(top.h, 10, 0); LP_ASSERT_NEAR(a.y, 10, 0); LP_ASSERT_NEAR(a.h, 40, 0);
    lp_rect right = lp_rect_cut_right(&a, 30);
    LP_ASSERT_NEAR(right.x, 70, 0); LP_ASSERT_NEAR(a.w, 70, 0);
    lp_rect inset = lp_rect_inset(a, 5, 5);
    LP_ASSERT_NEAR(inset.w, 60, 0); LP_ASSERT_NEAR(inset.h, 30, 0);
}

LP_TEST(rows_share_free_space_by_weight) {
    lp_rect out[3];
    float flex[3] = { 0, 1, 2 }, fixed[3] = { 40, 0, 0 };
    lp_layout_row(LP_RECT(0, 0, 100, 20), 5, 3, flex, fixed, out);
    LP_ASSERT_NEAR(out[0].w, 40, 0);
    LP_ASSERT_NEAR(out[1].x, 45, 0);
    LP_ASSERT_NEAR(out[1].w, 50.0 / 3, 1e-4);
    LP_ASSERT_NEAR(out[2].w, 100.0 / 3, 1e-4);
    LP_ASSERT_NEAR(out[2].x + out[2].w, 100, 1e-4);
}

LP_TEST(columns_and_centring) {
    lp_rect out[2];
    float flex[2] = { 1, 1 };
    lp_layout_column(LP_RECT(0, 0, 10, 100), 0, 2, flex, NULL, out);
    LP_ASSERT_NEAR(out[1].y, 50, 0);
    lp_rect c = lp_rect_center(LP_RECT(0, 0, 100, 100), (lp_size){ 20, 10 });
    LP_ASSERT_NEAR(c.x, 40, 0); LP_ASSERT_NEAR(c.y, 45, 0);
    LP_ASSERT(lp_rect_contains(c, 41, 46));
    LP_ASSERT(!lp_rect_contains(c, 60, 46));
}

int main(void) {
    LP_RUN(cuts_take_from_the_edges);
    LP_RUN(rows_share_free_space_by_weight);
    LP_RUN(columns_and_centring);
    LP_TEST_MAIN_END();
}
