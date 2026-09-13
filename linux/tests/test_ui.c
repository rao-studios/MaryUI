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

LP_TEST(hot_remembers_when_it_moved_and_what_it_left) {
    lp_ctx ctx = { 0 };
    lp_rect a = LP_RECT(0, 0, 20, 20), b = LP_RECT(40, 0, 20, 20);
    lp_id ida = LP_ID("a"), idb = LP_ID("b");
    const float xs[3] = { 10, 30, 50 };
    const double at[3] = { 100, 200, 300 };
    for (int i = 0; i < 3; i++) {
        lp_input in = pointer(xs[i], 10, 0, 0, 0);
        lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 100, 20), at[i]);
        lp_hot(&ctx, ida, a);
        lp_hot(&ctx, idb, b);
        lp_ctx_end(&ctx);
    }
    LP_ASSERT(lp_is_hot(&ctx, idb));
    LP_ASSERT(ctx.hot_since_ms == 300);
    /* The gap between them is not a widget: a is still the one left behind. */
    LP_ASSERT_EQ(ctx.last_hot, ida);
    LP_ASSERT(ctx.last_hot_since_ms == 100);
    LP_ASSERT(ctx.last_hot_until_ms == 200);
}

LP_TEST(held_is_active_under_the_pointer_and_remembers_its_last) {
    lp_ctx ctx = { 0 };
    lp_rect r = LP_RECT(0, 0, 20, 20);
    lp_id id = LP_ID("orb");
    lp_input in = pointer(10, 10, LP_BUTTON_LEFT, 0, LP_BUTTON_LEFT);
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 100, 40), 100);
    lp_clicked(&ctx, id, r);
    lp_ctx_end(&ctx);
    LP_ASSERT_EQ(ctx.held, id);
    LP_ASSERT(ctx.held_since_ms == 100);
    /* Dragged off while still down: active, but no longer held. */
    in = pointer(60, 30, 0, 0, LP_BUTTON_LEFT);
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 100, 40), 250);
    lp_clicked(&ctx, id, r);
    lp_ctx_end(&ctx);
    LP_ASSERT(lp_is_active(&ctx, id));
    LP_ASSERT_EQ(ctx.held, 0);
    LP_ASSERT_EQ(ctx.last_held, id);
    LP_ASSERT(ctx.last_held_since_ms == 100);
    LP_ASSERT(ctx.last_held_until_ms == 250);
}

int main(void) {
    LP_RUN(ids_are_stable_and_distinct);
    LP_RUN(a_press_and_release_inside_is_a_click);
    LP_RUN(a_release_outside_is_not_a_click);
    LP_RUN(the_draw_pass_never_reports_clicks);
    LP_RUN(hot_remembers_when_it_moved_and_what_it_left);
    LP_RUN(held_is_active_under_the_pointer_and_remembers_its_last);
    LP_TEST_MAIN_END();
}
