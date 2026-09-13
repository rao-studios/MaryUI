/* lp_text: the styles a layout is keyed on. Italic joined the key for Mary's
 * passages (Spotlight draws her replies in font.display italic); a cached
 * regular layout must never be handed back for an italic request. */
#include <cairo.h>
#include <math.h>
#include "lp_test.h"
#include "maryui/lp_text.h"

static cairo_t *make_cr(void) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 400, 100);
    cairo_t *cr = cairo_create(s);
    cairo_surface_destroy(s);
    return cr;
}

LP_TEST(an_italic_style_measures_and_draws) {
    cairo_t *cr = make_cr();
    lp_text_style st = lp_text_style_default();
    st.font = LP_FONT_DISPLAY;
    st.size_px = 18;
    st.italic = 1;
    st.letter_spacing = 0.3f;
    lp_size italic = lp_text_measure(cr, "I'm Mary, your voice here on MaryOS.", &st);
    LP_ASSERT(italic.w > 0 && italic.h > 0);
    lp_size again = lp_text_measure(cr, "I'm Mary, your voice here on MaryOS.", &st);
    LP_ASSERT(fabsf(again.w - italic.w) < 0.01f);
    st.italic = 0;
    lp_size regular = lp_text_measure(cr, "I'm Mary, your voice here on MaryOS.", &st);
    LP_ASSERT(regular.w > 0 && regular.h > 0);
    /* Both draw; the cap middle is cached per italic too. */
    st.italic = 1;
    lp_text_draw(cr, "passage", (lp_rect){ 0, 0, 200, 30 }, &st, LP_ALIGN_START);
    LP_ASSERT(lp_text_cap_middle(cr, &st) > 0);
    cairo_destroy(cr);
}

LP_TEST(a_wrapped_italic_layout_keeps_its_indices) {
    lp_text_style st = lp_text_style_default();
    st.font = LP_FONT_DISPLAY;
    st.size_px = 18;
    st.italic = 1;
    const char *text = "I can chat, help with your writing, play music, check your calendar, or just keep you company.";
    lp_text_layout *l = lp_text_layout_new(NULL, text, -1, &st, 220);
    LP_ASSERT(l != NULL);
    LP_ASSERT(lp_text_layout_line_count(l) >= 2);
    lp_rect rects[16];
    int n = lp_text_layout_range_rects(l, 0, (int)strlen(text), rects, 16);
    LP_ASSERT(n == lp_text_layout_line_count(l));
    int idx = lp_text_layout_xy_to_index(l, rects[0].x + 1, rects[0].y + rects[0].h / 2);
    LP_ASSERT(idx == 0);
    lp_text_layout_free(l);
}

int main(void) {
    LP_RUN(an_italic_style_measures_and_draws);
    LP_RUN(a_wrapped_italic_layout_keeps_its_indices);
    LP_TEST_MAIN_END();
}
