/* The lava wallpaper: a quarter-size frame with a margin, opaque platinum with real shading, smooth in space
 * (a lava lamp, not creased metal), slow in time but never still, the same picture for the same moment, both
 * grades, the stretched still with no dark border, and lava as the default wallpaper that survives a save. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE   /* mkdtemp */
#endif
#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lp_test.h"
#include "maryui/lp_lava.h"
#include "maryui/lp_settings.h"

#define W 1280
#define H 800

static uint32_t pixel(cairo_surface_t *s, int x, int y) {
    const unsigned char *d = cairo_image_surface_get_data(s);
    return ((const uint32_t *)(void *)(d + y * cairo_image_surface_get_stride(s)))[x];
}

static float lum(uint32_t px) {
    return (0.2126f * (float)((px >> 16) & 255) + 0.7152f * (float)((px >> 8) & 255) + 0.0722f * (float)(px & 255)) / 255;
}

static void stats(cairo_surface_t *s, float *mean, float *sd) {
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    double sum = 0, sq = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) { float l = lum(pixel(s, x, y)); sum += l; sq += l * l; }
    double n = (double)w * h, m = sum / n;
    *mean = (float)m;
    *sd = (float)sqrt(sq / n - m * m);
}

LP_TEST(the_frame_is_a_quarter_of_the_output_with_a_margin) {
    int fw, fh;
    lp_lava_frame_size(1280, 800, &fw, &fh);
    LP_ASSERT_EQ(fw, 320 + 2 * LP_LAVA_MARGIN);
    LP_ASSERT_EQ(fh, 200 + 2 * LP_LAVA_MARGIN);
    lp_lava_frame_size(1281, 801, &fw, &fh);
    LP_ASSERT_EQ(fw, 321 + 2 * LP_LAVA_MARGIN);                  /* rounded up: the frame always covers the output */
    LP_ASSERT_EQ(fh, 201 + 2 * LP_LAVA_MARGIN);
    cairo_surface_t *f = lp_lava_frame(W, H, 0, LP_MOLTEN_PLATINUM);
    LP_ASSERT(f != NULL);
    lp_lava_frame_size(W, H, &fw, &fh);
    LP_ASSERT_EQ(cairo_image_surface_get_width(f), fw);
    LP_ASSERT_EQ(cairo_image_surface_get_height(f), fh);
    int translucent = 0;
    for (int y = 0; y < fh; y++) for (int x = 0; x < fw; x++) translucent += (pixel(f, x, y) >> 24) != 255;
    LP_ASSERT_EQ(translucent, 0);
    cairo_surface_destroy(f);
    LP_ASSERT(lp_lava_frame(0, 800, 0, LP_MOLTEN_PLATINUM) == NULL);
}

LP_TEST(it_is_shaded_platinum_not_a_flat_grey) {
    cairo_surface_t *f = lp_lava_frame(W, H, 3, LP_MOLTEN_PLATINUM);
    float mean, sd;
    stats(f, &mean, &sd);
    if (!(mean > 0.25f && mean < 0.8f && sd > 0.04f)) LP_FAIL("mean %.3f, sd %.3f", mean, sd);
    /* cool, as the platinum grade is: blue at least as strong as red on average */
    double r = 0, b = 0;
    int fw = cairo_image_surface_get_width(f), fh = cairo_image_surface_get_height(f);
    for (int y = 0; y < fh; y++) for (int x = 0; x < fw; x++) { uint32_t p = pixel(f, x, y); r += (p >> 16) & 255; b += p & 255; }
    LP_ASSERT(b >= r);
    cairo_surface_destroy(f);
}

LP_TEST(it_is_smooth_like_wax_not_creased_like_metal) {
    cairo_surface_t *f = lp_lava_frame(W, H, 7, LP_MOLTEN_PLATINUM);
    int fw = cairo_image_surface_get_width(f), fh = cairo_image_surface_get_height(f);
    int n = 0, steep = 0;
    float worst = 0;
    for (int y = 1; y < fh - 1; y++)
        for (int x = 1; x < fw - 1; x++) {
            float l = lum(pixel(f, x, y));
            float dx = fabsf(lum(pixel(f, x + 1, y)) - l), dy = fabsf(lum(pixel(f, x, y + 1)) - l);
            float d = dx > dy ? dx : dy;
            if (d > worst) worst = d;
            steep += d > 8.0f / 255;
            n++;
        }
    /* between neighbouring field pixels — four output pixels apart — the light barely changes: a wax blob's soft rim
     * steps some teens of levels, a crease steps hundreds (the molten shader's fold measured 55% of steps over 8
     * levels and a worst of 222 at this resolution; the lava measures about 1.4% and 17) */
    if (steep > 3 * n / 100 || worst > 40.0f / 255) LP_FAIL("%d of %d steps over 8 levels, worst %.1f levels", steep, n, worst * 255);
    cairo_surface_destroy(f);
}

static float mean_difference(cairo_surface_t *a, cairo_surface_t *b, float *worst) {
    int fw = cairo_image_surface_get_width(a), fh = cairo_image_surface_get_height(a);
    double sum = 0;
    *worst = 0;
    for (int y = 0; y < fh; y++)
        for (int x = 0; x < fw; x++) {
            float d = fabsf(lum(pixel(a, x, y)) - lum(pixel(b, x, y)));
            sum += d;
            if (d > *worst) *worst = d;
        }
    return (float)(sum / ((double)fw * fh));
}

LP_TEST(it_moves_slowly_and_never_stops) {
    double t = 20;
    cairo_surface_t *a = lp_lava_frame(W, H, t, LP_MOLTEN_PLATINUM);
    cairo_surface_t *next = lp_lava_frame(W, H, t + LP_LAVA_SPEED / LP_LAVA_FPS, LP_MOLTEN_PLATINUM);
    cairo_surface_t *later = lp_lava_frame(W, H, t + 12 * LP_LAVA_SPEED, LP_MOLTEN_PLATINUM);
    float worst_next, worst_later;
    float step = mean_difference(a, next, &worst_next), drift = mean_difference(a, later, &worst_later);
    /* one frame apart: almost the same picture, so 15 steps a second read as one motion */
    if (!(step > 0 && step < 1.5f / 255 && worst_next < 12.0f / 255)) LP_FAIL("a frame's step: mean %.2f levels, worst %.1f", step * 255, worst_next * 255);
    /* twelve seconds apart: plainly somewhere else */
    if (!(drift > 6.0f / 255)) LP_FAIL("twelve seconds' drift: mean %.2f levels", drift * 255);
    cairo_surface_destroy(a);
    cairo_surface_destroy(next);
    cairo_surface_destroy(later);
}

LP_TEST(the_same_moment_is_the_same_picture_and_the_grades_differ) {
    cairo_surface_t *a = lp_lava_frame(640, 400, 5.5, LP_MOLTEN_PLATINUM), *b = lp_lava_frame(640, 400, 5.5, LP_MOLTEN_PLATINUM);
    int fw = cairo_image_surface_get_width(a), fh = cairo_image_surface_get_height(a);
    LP_ASSERT(memcmp(cairo_image_surface_get_data(a), cairo_image_surface_get_data(b), (size_t)cairo_image_surface_get_stride(a) * fh) == 0);
    cairo_surface_t *faithful = lp_lava_frame(640, 400, 5.5, LP_MOLTEN_FAITHFUL);
    float pm, ps, fm, fs;
    stats(a, &pm, &ps);
    stats(faithful, &fm, &fs);
    if (!(fm < pm - 0.03f)) LP_FAIL("platinum %.3f, faithful %.3f", pm, fm);   /* the shader's dark original */
    (void)fw;
    cairo_surface_destroy(a);
    cairo_surface_destroy(b);
    cairo_surface_destroy(faithful);
}

LP_TEST(the_still_is_the_frame_stretched_without_a_dark_border) {
    cairo_surface_t *s = lp_lava_still(W, H, 9, LP_MOLTEN_PLATINUM), *f = lp_lava_frame(W, H, 9, LP_MOLTEN_PLATINUM);
    LP_ASSERT_EQ(cairo_image_surface_get_width(s), W);
    LP_ASSERT_EQ(cairo_image_surface_get_height(s), H);
    /* the middle of a field pixel is that pixel */
    int fx = 160, fy = 100;
    float there = lum(pixel(f, fx + LP_LAVA_MARGIN, fy + LP_LAVA_MARGIN));
    float here = lum(pixel(s, fx * LP_LAVA_SCALE + LP_LAVA_SCALE / 2, fy * LP_LAVA_SCALE + LP_LAVA_SCALE / 2));
    if (fabsf(here - there) > 3.0f / 255) LP_FAIL("field %.1f, still %.1f levels", there * 255, here * 255);
    /* the margin carries the field on, rather than the vignette's nothing past the edge: the stretch samples it for
     * the outermost output pixels, which then sit with their neighbours (the shader's vignette still falls there) */
    int fw = cairo_image_surface_get_width(f), fh = cairo_image_surface_get_height(f);
    float worst = 0, outer = 0;
    for (int y = LP_LAVA_MARGIN; y < fh - LP_LAVA_MARGIN; y++) {
        float d = fabsf(lum(pixel(f, 0, y)) - lum(pixel(f, 1, y)));
        if (d > worst) worst = d;
        d = fabsf(lum(pixel(f, fw - 1, y)) - lum(pixel(f, fw - 2, y)));
        if (d > worst) worst = d;
    }
    for (int y = 8; y < H - 8; y += 16) {
        float d = fabsf(lum(pixel(s, 0, y)) - lum(pixel(s, 1, y)));
        if (d > outer) outer = d;
        d = fabsf(lum(pixel(s, W - 1, y)) - lum(pixel(s, W - 2, y)));
        if (d > outer) outer = d;
    }
    if (worst > 12.0f / 255 || outer > 8.0f / 255) LP_FAIL("the margin steps %.1f levels, the outermost output pixel %.1f", worst * 255, outer * 255);
    cairo_surface_destroy(s);
    cairo_surface_destroy(f);
}

LP_TEST(lava_is_the_default_and_every_mode_survives_a_save) {
    LP_ASSERT_EQ(lp_settings_defaults().wallpaper, LP_WALLPAPER_LAVA);
    lp_settings s = lp_settings_defaults();
    static const enum lp_wallpaper_mode modes[] = { LP_WALLPAPER_MOLTEN, LP_WALLPAPER_PROCEDURAL, LP_WALLPAPER_LAVA };
    for (size_t i = 0; i < sizeof modes / sizeof *modes; i++) {
        s.wallpaper = modes[i];
        LP_ASSERT_EQ(lp_settings_save(&s), 0);
        LP_ASSERT_EQ(lp_settings_load().wallpaper, modes[i]);
    }
    char path[1024];
    snprintf(path, sizeof path, "%s/maryui/settings.conf", getenv("XDG_CONFIG_HOME"));
    FILE *f = fopen(path, "r");
    LP_ASSERT(f != NULL);
    char text[4096] = "";
    if (f) { size_t n = fread(text, 1, sizeof text - 1, f); text[n] = 0; fclose(f); }
    LP_ASSERT(strstr(text, "wallpaper=lava") != NULL);
    f = fopen(path, "w");
    if (f) { fputs("wallpaper=something-new\n", f); fclose(f); }
    LP_ASSERT_EQ(lp_settings_load().wallpaper, LP_WALLPAPER_LAVA);          /* an unknown value is the default */
}

int main(void) {
    char root[] = "/tmp/lp-lava-XXXXXX";
    if (!mkdtemp(root)) return 1;
    setenv("HOME", root, 1);
    setenv("XDG_CONFIG_HOME", root, 1);
    LP_RUN(the_frame_is_a_quarter_of_the_output_with_a_margin);
    LP_RUN(it_is_shaded_platinum_not_a_flat_grey);
    LP_RUN(it_is_smooth_like_wax_not_creased_like_metal);
    LP_RUN(it_moves_slowly_and_never_stops);
    LP_RUN(the_same_moment_is_the_same_picture_and_the_grades_differ);
    LP_RUN(the_still_is_the_frame_stretched_without_a_dark_border);
    LP_RUN(lava_is_the_default_and_every_mode_survives_a_save);
    LP_TEST_MAIN_END();
}
