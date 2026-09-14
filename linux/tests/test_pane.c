/* The pane kit: the shell Threads, Ambient and Abilities share — the header's cut, the content
 * column's inset, the label column, capsules measured rather than guessed, and cards whose height is
 * the same in both passes. */
#include <cairo.h>

#include "lp_test.h"
#include "maryui/lp_pane.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

static cairo_surface_t *surface;
static cairo_t *cr;
static lp_settings settings;

static void setup(void) {
    if (cr) cairo_destroy(cr);
    if (surface) cairo_surface_destroy(surface);
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 600, 400);
    cr = cairo_create(surface);
    settings = lp_settings_defaults();
}

LP_TEST(the_header_cuts_its_height_and_hands_back_the_controls_rect) {
    setup();
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    lp_rect area = LP_RECT(0, 0, 600, 400);
    lp_pane_header h = { .title = "Drive", .subtitle = "MaryOS · 3 of 3 files in the graph", .live = 1, .status = "threadd" };
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, NULL, area, 0);
    lp_rect controls = lp_pane_header_paint(&ctx, &area, &h);
    lp_ctx_end(&ctx);
    LP_ASSERT_NEAR(area.y, LP_PANE_HEADER_H, 1e-6);
    LP_ASSERT_NEAR(controls.x + controls.w, 600 - LP_PANE_INSET, 1e-6);
    /* a note under the subtitle makes it taller, and the DRAW pass cuts the same */
    lp_pane_header n = h;
    n.note = "One sentence in the serif.";
    area = LP_RECT(0, 0, 600, 400);
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, area, 0);
    lp_pane_header_paint(&ctx, &area, &n);
    lp_ctx_end(&ctx);
    LP_ASSERT_NEAR(area.y, LP_PANE_HEADER_H + LP_PANE_HEADER_NOTE_H, 1e-6);
}

LP_TEST(the_content_column_is_inset_the_same_on_both_sides) {
    lp_rect c = lp_pane_content(LP_RECT(180, 64, 620, 500));
    LP_ASSERT_NEAR(c.x, 180 + LP_PANE_INSET, 1e-6);
    LP_ASSERT_NEAR(c.w, 620 - 2 * LP_PANE_INSET, 1e-6);
    LP_ASSERT_NEAR(c.y, 64 + LP_SPACE_4, 1e-6);
}

LP_TEST(a_card_takes_the_same_room_in_both_passes_and_its_body_sits_under_the_title_band) {
    setup();
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    lp_rect column = LP_RECT(20, 20, 400, 1000), box;
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, NULL, LP_RECT(0, 0, 600, 400), 0);
    lp_rect body = lp_pane_card(&ctx, &column, "The volume", 60, &box);
    lp_ctx_end(&ctx);
    float taken = column.y - 20;
    LP_ASSERT_NEAR(body.y, box.y + LP_PANE_CARD_TITLE_H, 1e-6);
    LP_ASSERT_NEAR(body.x, box.x + LP_PANE_CARD_PAD, 1e-6);
    LP_ASSERT_NEAR(box.h, LP_PANE_CARD_TITLE_H + 60 + LP_PANE_CARD_PAD, 1e-6);
    column = LP_RECT(20, 20, 400, 1000);
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, 600, 400), 0);
    lp_pane_card(&ctx, &column, "The volume", 60, NULL);
    lp_ctx_end(&ctx);
    LP_ASSERT_NEAR(column.y - 20, taken, 1e-6);
}

LP_TEST(rows_share_one_label_column_and_capsules_are_measured) {
    setup();
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    float y = 10;
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, NULL, LP_RECT(0, 0, 600, 400), 0);
    lp_pane_row(&ctx, 0, &y, 400, "Name", "MaryOS");
    lp_pane_row(&ctx, 0, &y, 400, "Size", "12 GB");
    lp_ctx_end(&ctx);
    LP_ASSERT_NEAR(y, 10 + 2 * LP_PANE_ROW, 1e-6);
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, 600, 400), 0);
    float w = lp_pane_capsule(&ctx, 10, 10, "co-active", LP_PANE_SAGE);
    lp_text_style s = lp_text_style_default();
    s.size_px = LP_TEXT_XS;
    s.weight = LP_TEXT_WEIGHT_SEMIBOLD;
    s.uppercase = 1;
    s.letter_spacing = 0.5f;
    LP_ASSERT_NEAR(w, lp_text_measure(cr, "co-active", &s).w + 12, 0.5);
    float right = lp_pane_capsule_right(&ctx, 300, 10, "focus", LP_PANE_GOLD);
    LP_ASSERT(right > 20 && right < 80);
    lp_ctx_end(&ctx);
}

int main(void) {
    LP_RUN(the_header_cuts_its_height_and_hands_back_the_controls_rect);
    LP_RUN(the_content_column_is_inset_the_same_on_both_sides);
    LP_RUN(a_card_takes_the_same_room_in_both_passes_and_its_body_sits_under_the_title_band);
    LP_RUN(rows_share_one_label_column_and_capsules_are_measured);
    LP_TEST_MAIN_END();
}
