/* The ambient clock on its platinum capsule: sized to the text inside the 24-px chrome, dark ink on a
 * light plate, and its grain cut from the desktop's sheet (a shift of one tile is invisible). */
#include <cairo.h>
#include <string.h>

#include "lp_test.h"
#include "maryui/lp_clock.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_texture.h"
#include "maryui/lp_tokens.h"

#define W 160
#define H 24

static cairo_surface_t *paint(float world_x, float world_y) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *cr = cairo_create(s);
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.32);   /* a dark stretch of wallpaper */
    cairo_paint(cr);
    lp_settings settings = lp_settings_defaults();
    lp_clock_paint(cr, LP_RECT(0, 0, W, H), "Tue 9:41 AM", world_x, world_y, &settings);
    cairo_destroy(cr);
    cairo_surface_flush(s);
    return s;
}

static float luminance(cairo_surface_t *s, int x, int y) {
    const unsigned char *d = cairo_image_surface_get_data(s);
    uint32_t px = ((const uint32_t *)(d + y * cairo_image_surface_get_stride(s)))[x];
    float r = (float)((px >> 16) & 255) / 255, g = (float)((px >> 8) & 255) / 255, b = (float)(px & 255) / 255;
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

LP_TEST(the_capsule_hugs_the_text_inside_the_chrome) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *cr = cairo_create(s);
    lp_rect c = lp_clock_capsule(cr, LP_RECT(0, 0, W, H), "Tue 9:41 AM");
    LP_ASSERT_NEAR(c.h, LP_CLOCK_H, 1e-6);
    LP_ASSERT_NEAR(c.x + c.w, W, 1e-6);
    LP_ASSERT(c.w > 60 && c.w < W);
    LP_ASSERT(c.y >= 0 && c.y + c.h <= H);
    lp_rect wider = lp_clock_capsule(cr, LP_RECT(0, 0, W, H), "Wed 12:38 PM");
    LP_ASSERT(wider.w > c.w);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
}

LP_TEST(dark_ink_sits_on_a_light_plate) {
    cairo_surface_t *s = paint(0, 0);
    cairo_t *cr = cairo_create(s);
    lp_rect c = lp_clock_capsule(cr, LP_RECT(0, 0, W, H), "Tue 9:41 AM");
    cairo_destroy(cr);
    float darkest = 1, brightest = 0, sum = 0;
    int n = 0;
    for (int y = (int)c.y + 2; y < (int)(c.y + c.h) - 2; y++)
        for (int x = (int)c.x + 4; x < (int)(c.x + c.w) - 4; x++) {
            float l = luminance(s, x, y);
            if (l < darkest) darkest = l;
            if (l > brightest) brightest = l;
            sum += l;
            n++;
        }
    LP_ASSERT(darkest < 0.35f);        /* the glyphs' ink */
    LP_ASSERT(sum / n > 0.6f);         /* on platinum, not on the wallpaper */
    LP_ASSERT(brightest > 0.8f);
    /* outside the capsule the wallpaper shows through */
    LP_ASSERT(luminance(s, 4, H / 2) < 0.45f);
    cairo_surface_destroy(s);
}

static int identical(cairo_surface_t *a, cairo_surface_t *b) {
    return memcmp(cairo_image_surface_get_data(a), cairo_image_surface_get_data(b), (size_t)cairo_image_surface_get_stride(a) * H) == 0;
}

LP_TEST(the_grain_is_the_desktops_sheet) {
    cairo_surface_t *at0 = paint(0, 0), *shifted = paint(37, 11), *tiled = paint((float)lp_brush_live.tile, (float)lp_brush_live.tile);
    LP_ASSERT(!identical(at0, shifted));       /* a different part of the sheet */
    LP_ASSERT(identical(at0, tiled));          /* one whole tile away is the same metal */
    cairo_surface_destroy(at0);
    cairo_surface_destroy(shifted);
    cairo_surface_destroy(tiled);
}

int main(void) {
    LP_RUN(the_capsule_hugs_the_text_inside_the_chrome);
    LP_RUN(dark_ink_sits_on_a_light_plate);
    LP_RUN(the_grain_is_the_desktops_sheet);
    LP_TEST_MAIN_END();
}
