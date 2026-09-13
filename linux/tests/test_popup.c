/* The pop-up button (lp_popup.h): measured wider for a longer choice, not clicked when nobody pressed it, and drawn
 * as a capsule with its choice and chevron, enabled or not. Runs on a Mac as well as Linux. */
#include <cairo.h>
#include <string.h>

#include "lp_test.h"
#include "maryui/lp_popup.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_ui.h"

static uint32_t pixel(cairo_surface_t *s, int x, int y) {
    uint32_t px;
    memcpy(&px, cairo_image_surface_get_data(s) + y * cairo_image_surface_get_stride(s) + 4 * x, 4);
    return px;
}

LP_TEST(a_popup_button_draws_its_choice_and_chevron) {
    lp_settings settings = lp_settings_defaults();
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    LP_ASSERT(lp_popup_button_measure(&ctx, "Marie — French").w > lp_popup_button_measure(&ctx, "Marie").w);
    LP_ASSERT(lp_popup_button_measure(&ctx, "").w > 10);           /* room for the chevron with no choice yet */

    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 240, 40);
    cairo_t *cr = cairo_create(s);
    lp_input in = { .mx = -1, .my = -1 };
    lp_rect r = LP_RECT(10, 10, 200, 22);
    for (int disabled = 0; disabled < 2; disabled++) {
        lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, LP_RECT(0, 0, 240, 40), 10 + disabled);
        LP_ASSERT_EQ(lp_popup_button(&ctx, LP_ID("voice"), r, "Marie — French", disabled), 0);   /* nobody pressed it */
        lp_ctx_end(&ctx);
        lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, &in, LP_RECT(0, 0, 240, 40), 20 + disabled);
        lp_popup_button(&ctx, LP_ID("voice"), r, "Marie — French", disabled);
        lp_ctx_end(&ctx);
        cairo_surface_flush(s);
        LP_ASSERT(pixel(s, 110, 21) >> 24 != 0);                   /* the capsule */
    }
    LP_ASSERT_EQ(pixel(s, 2, 2) >> 24, 0);                          /* and nothing outside it */
    cairo_destroy(cr);
    cairo_surface_destroy(s);
}

int main(void) {
    LP_RUN(a_popup_button_draws_its_choice_and_chevron);
    LP_TEST_MAIN_END();
}
