/* The brushed grain is anchored to the desktop, not to the surface it fills:
 * the metal is one continuous sheet the windows are cut out of, so moving a
 * window uncovers a different part of it. The web does the same thing with a
 * background-position, which has no vitest counterpart — hence a C-only test. */
#include <stdlib.h>

#include "lp_test.h"
#include "maryui/components/lp_surface.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_tokens.h"

#define W 240
#define H 60

/* One title-bar surface, painted as if its origin sat at desktop x. */
static cairo_surface_t *paint_at(float world_x) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *cr = cairo_create(s);
    lp_surface_paint(cr, LP_RECT(0, 0, W, H), (lp_surface_opts){ .variant = LP_VARIANT_TITLEBAR, .radius = 0 },
        (lp_surface_motion){ .sheen_x = 0.5f, .world_x = world_x });
    cairo_destroy(cr);
    return s;
}

static double mean_diff(cairo_surface_t *a, cairo_surface_t *b) {
    cairo_surface_flush(a);
    cairo_surface_flush(b);
    int sa = cairo_image_surface_get_stride(a), sb = cairo_image_surface_get_stride(b);
    const unsigned char *da = cairo_image_surface_get_data(a), *db = cairo_image_surface_get_data(b);
    double total = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W * 4; x++) total += abs((int)da[(size_t)y * sa + x] - (int)db[(size_t)y * sb + x]);
    }
    return total / (W * H * 4.0);
}

LP_TEST(moving_a_surface_across_the_desktop_uncovers_different_metal) {
    cairo_surface_t *a = paint_at(0), *b = paint_at(64), *c = paint_at(256);
    LP_ASSERT(mean_diff(a, b) > 0.1);
    LP_ASSERT(mean_diff(a, c) > 0.1);
    cairo_surface_destroy(a);
    cairo_surface_destroy(b);
    cairo_surface_destroy(c);
}

LP_TEST(a_shift_of_one_whole_tile_is_invisible) {
    /* The tile is seamless, so the sheet repeats exactly: this is what proves
     * the grain is anchored to the desktop rather than merely being disturbed
     * by movement. A surface that carried its own grain would pass the test
     * above only by accident and would be identical here for every offset. */
    cairo_surface_t *a = paint_at(0);
    cairo_surface_t *one = paint_at(LP_BRUSH_TILE), *two = paint_at(2 * LP_BRUSH_TILE);
    LP_ASSERT_NEAR(mean_diff(a, one), 0, 1e-9);
    LP_ASSERT_NEAR(mean_diff(a, two), 0, 1e-9);
    cairo_surface_destroy(a);
    cairo_surface_destroy(one);
    cairo_surface_destroy(two);
}

LP_TEST(the_sheet_runs_unbroken_across_neighbouring_surfaces) {
    /* One wide surface, versus two half-width ones side by side in the same
     * chrome. The right half must come out identical: every surface samples the
     * same sheet, so a title bar and the body under it line up. Adding the
     * surface's own offset to the world origin — cairo's user space already
     * carries it — restarts the sheet at each surface and breaks this. */
    cairo_surface_t *one = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *c1 = cairo_create(one);
    lp_surface_paint(c1, LP_RECT(0, 0, W, H), (lp_surface_opts){ .variant = LP_VARIANT_TITLEBAR },
        (lp_surface_motion){ .sheen_x = 0.5f, .world_x = 0 });
    cairo_destroy(c1);

    cairo_surface_t *split = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *c2 = cairo_create(split);
    for (int half = 0; half < 2; half++) {
        lp_surface_paint(c2, LP_RECT(half * (W / 2), 0, W / 2, H), (lp_surface_opts){ .variant = LP_VARIANT_TITLEBAR },
            (lp_surface_motion){ .sheen_x = 0.5f, .world_x = 0 });
    }
    cairo_destroy(c2);

    cairo_surface_flush(one);
    cairo_surface_flush(split);
    int s1 = cairo_image_surface_get_stride(one), s2 = cairo_image_surface_get_stride(split);
    const unsigned char *d1 = cairo_image_surface_get_data(one), *d2 = cairo_image_surface_get_data(split);
    double total = 0;
    int n = 0;
    for (int y = 0; y < H; y++) {
        for (int x = (W / 2) * 4; x < W * 4; x++, n++) total += abs((int)d1[(size_t)y * s1 + x] - (int)d2[(size_t)y * s2 + x]);
    }
    LP_ASSERT_NEAR(total / n, 0, 1e-9);
    cairo_surface_destroy(one);
    cairo_surface_destroy(split);
}

int main(void) {
    LP_RUN(moving_a_surface_across_the_desktop_uncovers_different_metal);
    LP_RUN(a_shift_of_one_whole_tile_is_invisible);
    LP_RUN(the_sheet_runs_unbroken_across_neighbouring_surfaces);
    LP_TEST_MAIN_END();
}
