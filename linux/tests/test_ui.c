/* The immediate-mode core: ids, hot/active tracking, the click idiom. */
#include "lp_test.h"
#include "maryui/lp_ui.h"

static lp_input pointer(float x, float y, int pressed, int released, int buttons) {
    lp_input in = { 0 };
    in.mx = x; in.my = y; in.pressed = pressed; in.released = released; in.buttons = buttons;
    return in;
}

LP_TEST(ids_are_stable_and_distinct) {
    LP_ASSERT_EQ(LP_ID("button"), LP_ID("button"));
    LP_ASSERT(LP_ID("button") != LP_ID("toggle"));
    LP_ASSERT(lp_id_index(LP_ID("row"), 0) != lp_id_index(LP_ID("row"), 1));
    LP_ASSERT(lp_id_index(LP_ID("row"), 3) == lp_id_index(LP_ID("row"), 3));
}

LP_TEST(a_press_and_release_inside_is_a_click) {
    lp_ctx ctx = { 0 };
    lp_rect r = LP_RECT(10, 10, 100, 22);
    lp_id id = LP_ID("ok");
    lp_input in = pointer(20, 20, LP_BUTTON_LEFT, 0, LP_BUTTON_LEFT);
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 200, 100), 0);
    LP_ASSERT_EQ(lp_clicked(&ctx, id, r), 0);
    lp_ctx_end(&ctx);
    LP_ASSERT(lp_is_active(&ctx, id));
    LP_ASSERT(lp_is_hot(&ctx, id));
    in = pointer(25, 20, 0, LP_BUTTON_LEFT, 0);
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 200, 100), 0);
    LP_ASSERT_EQ(lp_clicked(&ctx, id, r), 1);
    lp_ctx_end(&ctx);
    LP_ASSERT(!lp_is_active(&ctx, id));
}

LP_TEST(a_release_outside_is_not_a_click) {
    lp_ctx ctx = { 0 };
    lp_rect r = LP_RECT(10, 10, 100, 22);
    lp_id id = LP_ID("cancel");
    lp_input in = pointer(20, 20, LP_BUTTON_LEFT, 0, LP_BUTTON_LEFT);
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 200, 100), 0);
    lp_clicked(&ctx, id, r);
    lp_ctx_end(&ctx);
    in = pointer(150, 80, 0, LP_BUTTON_LEFT, 0);
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 200, 100), 0);
    LP_ASSERT_EQ(lp_clicked(&ctx, id, r), 0);
    lp_ctx_end(&ctx);
    LP_ASSERT(!lp_is_hot(&ctx, id));
}

LP_TEST(the_draw_pass_never_reports_clicks) {
    lp_ctx ctx = { 0 };
    lp_input in = pointer(20, 20, LP_BUTTON_LEFT, LP_BUTTON_LEFT, 0);
    lp_ctx_begin(&ctx, LP_PASS_DRAW, NULL, &in, LP_RECT(0, 0, 200, 100), 0);
    LP_ASSERT_EQ(lp_clicked(&ctx, LP_ID("x"), LP_RECT(10, 10, 100, 22)), 0);
    lp_ctx_end(&ctx);
}

int main(void) {
    LP_RUN(ids_are_stable_and_distinct);
    LP_RUN(a_press_and_release_inside_is_a_click);
    LP_RUN(a_release_outside_is_not_a_click);
    LP_RUN(the_draw_pass_never_reports_clicks);
    LP_TEST_MAIN_END();
}
