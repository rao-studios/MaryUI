/* The ambient clock's letters are brushed platinum: the time's line right-aligned inside the chrome, pale metal
 * inside the glyphs with a dark edge round them, the wallpaper between them and around them, and the grain cut
 * from the desktop's sheet (a shift of one tile is invisible). */
#include <cairo.h>
#include <string.h>

#include "lp_test.h"
#include "maryui/lp_clock.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_texture.h"
#include "maryui/lp_tokens.h"
#include "maryui/lp_ui.h"

#define W 220
#define H 24

/* The clock over a flat wallpaper of grey `g`, or over nothing (g < 0). */
static cairo_surface_t *paint_over(float g, float world_x, float world_y) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *cr = cairo_create(s);
    if (g >= 0) {
        cairo_set_source_rgb(cr, g, g, g);
        cairo_paint(cr);
    }
    lp_settings settings = lp_settings_defaults();
    lp_clock_paint(cr, LP_RECT(0, 0, W, H), "Tue Sep 14 9:41 AM", world_x, world_y, &settings);
    cairo_destroy(cr);
    cairo_surface_flush(s);
    return s;
}

static uint32_t pixel(cairo_surface_t *s, int x, int y) {
    const unsigned char *d = cairo_image_surface_get_data(s);
    return ((const uint32_t *)(d + y * cairo_image_surface_get_stride(s)))[x];
}

/* Straight (unpremultiplied) luminance. */
static float luminance(uint32_t px) {
    float a = (float)((px >> 24) & 255) / 255;
    if (a <= 0) return 0;
    float r = (float)((px >> 16) & 255) / 255 / a, g = (float)((px >> 8) & 255) / 255 / a, b = (float)(px & 255) / 255 / a;
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

LP_TEST(the_line_hugs_the_text_inside_the_chrome) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *cr = cairo_create(s);
    lp_rect c = lp_clock_bounds(cr, LP_RECT(0, 0, W, H), "Tue Sep 14 9:41 AM");
    LP_ASSERT_NEAR(c.x + c.w, W - LP_CLOCK_INSET, 1e-6);
    LP_ASSERT(c.w > 60 && c.w < W - LP_CLOCK_INSET);
    LP_ASSERT(c.y >= 0 && c.y + c.h <= H);
    lp_rect wider = lp_clock_bounds(cr, LP_RECT(0, 0, W, H), "Wed Sep 30 12:38 PM");
    LP_ASSERT(wider.w > c.w);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
}

LP_TEST(the_letters_are_pale_metal_with_a_dark_edge_and_no_plate) {
    /* over nothing: only the letters, their keyline and their shadow have any alpha */
    cairo_surface_t *s = paint_over(-1, 0, 0);
    int solid = 0, dark_edge = 0, clear = 0, n = 0;
    float metal = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint32_t px = pixel(s, x, y);
            int a = (int)((px >> 24) & 255);
            n++;
            if (a == 255) { solid++; metal += luminance(px); }
            else if (a > 60 && luminance(px) < 0.3f) dark_edge++;
            else if (a == 0) clear++;
        }
    LP_ASSERT(solid > 150);                     /* the glyphs' inside is opaque metal */
    LP_ASSERT(metal / solid > 0.55f);           /* and pale: platinum, not ink */
    LP_ASSERT(dark_edge > 40);                  /* a dark edge round them */
    LP_ASSERT(clear > n / 2);                   /* and no plate: most of the chrome is still the wallpaper's */
    cairo_surface_destroy(s);
    /* over a dark wallpaper the metal still stands out, and left of the time the wallpaper is untouched */
    s = paint_over(0.3f, 0, 0);
    LP_ASSERT_NEAR(luminance(pixel(s, 4, H / 2)), 0.3f, 0.02f);
    float brightest = 0;
    for (int x = 0; x < W; x++) { float l = luminance(pixel(s, x, H / 2)); if (l > brightest) brightest = l; }
    LP_ASSERT(brightest > 0.7f);
    cairo_surface_destroy(s);
    /* over a pale wallpaper the keyline keeps the letters' edges visible */
    s = paint_over(0.92f, 0, 0);
    float darkest = 1;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { float l = luminance(pixel(s, x, y)); if (l < darkest) darkest = l; }
    LP_ASSERT(darkest < 0.55f);
    cairo_surface_destroy(s);
}

static int identical(cairo_surface_t *a, cairo_surface_t *b) {
    return memcmp(cairo_image_surface_get_data(a), cairo_image_surface_get_data(b), (size_t)cairo_image_surface_get_stride(a) * H) == 0;
}

LP_TEST(the_grain_is_the_desktops_sheet) {
    cairo_surface_t *at0 = paint_over(0.3f, 0, 0), *shifted = paint_over(0.3f, 37, 11),
                    *tiled = paint_over(0.3f, (float)lp_brush_live.tile, (float)lp_brush_live.tile);
    LP_ASSERT(!identical(at0, shifted));       /* a different part of the sheet shows through the letters */
    LP_ASSERT(identical(at0, tiled));          /* one whole tile away is the same metal */
    cairo_surface_destroy(at0);
    cairo_surface_destroy(shifted);
    cairo_surface_destroy(tiled);
}

LP_TEST(the_clock_reads_weekday_month_and_day_before_the_time) {
    struct tm tm = { 0 };
    tm.tm_wday = 1; tm.tm_mon = 8; tm.tm_mday = 14; tm.tm_hour = 6; tm.tm_min = 21;
    char text[40];
    lp_clock_format(&tm, 0, text, sizeof text);
    LP_ASSERT_STR(text, "Mon Sep 14 6:21 AM");
    lp_clock_format(&tm, 1, text, sizeof text);
    LP_ASSERT_STR(text, "Mon Sep 14 06:21");
    tm.tm_wday = 3; tm.tm_mon = 11; tm.tm_mday = 31; tm.tm_hour = 0; tm.tm_min = 5;
    lp_clock_format(&tm, 0, text, sizeof text);
    LP_ASSERT_STR(text, "Wed Dec 31 12:05 AM");                       /* midnight is twelve */
    tm.tm_hour = 12;
    lp_clock_format(&tm, 0, text, sizeof text);
    LP_ASSERT_STR(text, "Wed Dec 31 12:05 PM");
    /* the widest string fits the compositor's 220-px chrome */
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *cr = cairo_create(s);
    lp_rect b = lp_clock_bounds(cr, LP_RECT(0, 0, W, H), "Wed Sep 30 12:38 PM");
    LP_ASSERT(b.x > 0);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
}

LP_TEST(only_the_text_answers_the_pointer_and_it_rests_at_half) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t *cr = cairo_create(s);
    const char *text = "Mon Sep 14 6:47 AM";
    lp_rect box = LP_RECT(0, 0, W, H), b = lp_clock_bounds(cr, box, text), hit = lp_clock_hit_rect(cr, box, text);
    LP_ASSERT(hit.x <= b.x && hit.x + hit.w >= b.x + b.w);           /* all of the text */
    LP_ASSERT(hit.x >= 0 && hit.x + hit.w <= W && hit.y >= 0 && hit.y + hit.h <= H);
    LP_ASSERT(lp_rect_contains(hit, b.x + b.w / 2, H / 2.0f));
    LP_ASSERT(!lp_rect_contains(hit, 2, H / 2.0f));                  /* left of the text: the wallpaper's */
    lp_rect scratch = lp_clock_hit_rect(NULL, box, text);            /* the compositor measures without a context */
    LP_ASSERT_NEAR(scratch.x, hit.x, 0.5);
    LP_ASSERT_NEAR(scratch.w, hit.w, 0.5);
    LP_ASSERT_NEAR(LP_CLOCK_REST_OPACITY, 0.5, 1e-6);
    LP_ASSERT_NEAR(LP_CLOCK_HOVER_OPACITY, 1.0, 1e-6);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
}

int main(void) {
    LP_RUN(only_the_text_answers_the_pointer_and_it_rests_at_half);
    LP_RUN(the_clock_reads_weekday_month_and_day_before_the_time);
    LP_RUN(the_line_hugs_the_text_inside_the_chrome);
    LP_RUN(the_letters_are_pale_metal_with_a_dark_edge_and_no_plate);
    LP_RUN(the_grain_is_the_desktops_sheet);
    LP_TEST_MAIN_END();
}
