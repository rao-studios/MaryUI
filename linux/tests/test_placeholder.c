/* An empty text field's caret stands clear of its placeholder (PARITY D36): in a TextField and a TextArea the
 * placeholder's first inked column is at least two clear pixels right of the caret, and it sits in the same place
 * whether the field has the focus or not. */
#include <cairo.h>
#include <string.h>

#include "lp_test.h"
#include "maryui/components/lp_controls.h"
#include "maryui/components/lp_text_area.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"

#define W 320
#define H 120
enum kind { FIELD, AREA };
static lp_settings settings;
static const lp_rect FIELD_RECT = { 10, 10, 300, 28 }, AREA_RECT = { 10, 10, 300, 100 };

static cairo_surface_t *render(enum kind kind, int focused, const char *placeholder) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    lp_ctx ctx = { 0 };
    ctx.settings = &settings;
    ctx.active_window = 1;
    lp_id id = LP_ID("field");
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, W, H), 100);
    ctx.focus = focused ? id : 0;
    if (kind == FIELD) {
        lp_text_buffer b = { 0 };
        lp_text_field(&ctx, id, FIELD_RECT, &b, (lp_text_field_opts){ .placeholder = placeholder, .icon = LP_ICON_COUNT });
    } else {
        lp_text_area_state a;
        memset(&a, 0, sizeof a);
        lp_text_doc_init(&a.doc);
        lp_text_area(&ctx, id, AREA_RECT, &a, (lp_text_area_opts){ .placeholder = placeholder });
        lp_text_doc_free(&a.doc);
    }
    lp_ctx_end(&ctx);
    cairo_destroy(cr);
    cairo_surface_flush(s);
    return s;
}

static uint32_t px(cairo_surface_t *s, int x, int y) {
    return ((const uint32_t *)(void *)(cairo_image_surface_get_data(s) + y * cairo_image_surface_get_stride(s)))[x];
}

/* The leftmost (or rightmost) column where two renders differ inside [x0, x1) × [y0, y1); -1 when none does. */
static int diff_column(cairo_surface_t *a, cairo_surface_t *b, int x0, int x1, int y0, int y1, int rightmost) {
    int found = -1;
    for (int x = x0; x < x1; x++)
        for (int y = y0; y < y1; y++)
            if (px(a, x, y) != px(b, x, y)) {
                if (!rightmost) return x;
                found = x;
                break;
            }
    return found;
}

static void check(enum kind kind, lp_rect r, int band0, int band1) {
    cairo_surface_t *plain = render(kind, 0, NULL), *focused = render(kind, 1, NULL);
    cairo_surface_t *named = render(kind, 0, "Search"), *named_focused = render(kind, 1, "Search");
    /* inside the rect, clear of the focus ring at its edge: the caret is all that focus adds */
    int x0 = (int)r.x + 6, x1 = (int)(r.x + r.w) - 6, y0 = (int)r.y + band0, y1 = (int)r.y + band1;
    int caret = diff_column(plain, focused, x0, x1, y0, y1, 1);
    int ink = diff_column(plain, named, x0, x1, y0, y1, 0);
    int ink_focused = diff_column(focused, named_focused, x0, x1, y0, y1, 0);
    LP_ASSERT(caret >= 0 && ink >= 0);
    if (ink - caret < 3) LP_FAIL("%s: the caret's column %d, the placeholder's first ink %d", kind == FIELD ? "TextField" : "TextArea", caret, ink);
    LP_ASSERT_EQ(ink_focused, ink);                                   /* nothing moves when the field takes the focus */
    cairo_surface_destroy(plain);
    cairo_surface_destroy(focused);
    cairo_surface_destroy(named);
    cairo_surface_destroy(named_focused);
}

LP_TEST(a_text_fields_caret_stands_clear_of_its_placeholder) { check(FIELD, FIELD_RECT, 8, 20); }
LP_TEST(a_text_areas_caret_stands_clear_of_its_placeholder) { check(AREA, AREA_RECT, 6, 24); }

int main(void) {
    settings = lp_settings_defaults();
    LP_RUN(a_text_fields_caret_stands_clear_of_its_placeholder);
    LP_RUN(a_text_areas_caret_stands_clear_of_its_placeholder);
    LP_TEST_MAIN_END();
}
