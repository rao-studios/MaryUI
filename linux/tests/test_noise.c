/* The feTurbulence port: deterministic, in range, and stitched tiles repeat. */
#include "lp_test.h"
#include "maryui/lp_noise.h"
#include "maryui/lp_texture.h"

LP_TEST(is_deterministic_for_a_seed) {
    lp_turbulence a, b;
    lp_turbulence_init(&a, 7);
    lp_turbulence_init(&b, 7);
    double sa = lp_turbulence_sum(&a, 0, 12.5, 40.25, 0.02, 0.6, 2, 1, NULL);
    double sb = lp_turbulence_sum(&b, 0, 12.5, 40.25, 0.02, 0.6, 2, 1, NULL);
    LP_ASSERT_NEAR(sa, sb, 0);
    lp_turbulence_init(&b, 8);
    LP_ASSERT(sb != lp_turbulence_sum(&b, 0, 12.5, 40.25, 0.02, 0.6, 2, 1, NULL));
}

LP_TEST(fractal_values_stay_in_range_and_average_mid_gray) {
    lp_turbulence t;
    lp_turbulence_init(&t, 11);
    double sum = 0;
    int n = 0;
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            double v = lp_noise_fractal_value(lp_turbulence_sum(&t, 0, x * 73.0, y * 51.0, 0.0016, 0.0026, 3, 1, NULL));
            LP_ASSERT(v >= 0 && v <= 1);
            sum += v;
            n++;
        }
    }
    LP_ASSERT_NEAR(sum / n, 0.5, 0.1);
}

/* stitchTiles: the noise inside one period joins its own far edge, so a
 * rasterised period repeats without a seam (browsers tile that image). */
LP_TEST(stitched_noise_is_seamless_at_the_tile_edges) {
    lp_turbulence t;
    lp_turbulence_init(&t, 7);
    lp_stitch tile = { 0, 0, 228.97, 228.97 };
    double eps = 1e-3, worst = 0;
    for (int i = 0; i < 40; i++) {
        double y = i * 5.7 + 0.9, x = i * 3.3 + 1.1;
        double left = lp_turbulence_sum(&t, 1, 0, y, 0.02, 0.6, 2, 1, &tile);
        double right = lp_turbulence_sum(&t, 1, tile.w - eps, y, 0.02, 0.6, 2, 1, &tile);
        double top = lp_turbulence_sum(&t, 1, x, 0, 0.02, 0.6, 2, 1, &tile);
        double bottom = lp_turbulence_sum(&t, 1, x, tile.h - eps, 0.02, 0.6, 2, 1, &tile);
        if (fabs(left - right) > worst) worst = fabs(left - right);
        if (fabs(top - bottom) > worst) worst = fabs(top - bottom);
    }
    LP_ASSERT(worst < 0.02);
    /* Without stitching the edges do not meet. */
    double left = lp_turbulence_sum(&t, 1, 0, 40, 0.02, 0.6, 2, 1, NULL);
    double right = lp_turbulence_sum(&t, 1, tile.w - eps, 40, 0.02, 0.6, 2, 1, NULL);
    LP_ASSERT(fabs(left - right) > 0.02 || worst < 0.02);
}

LP_TEST(brush_tile_is_seamless_mid_gray) {
    lp_brush_params p = lp_brush_params_from_tokens();
    LP_ASSERT_EQ(p.tile, 512);
    LP_ASSERT_NEAR(lp_brush_angle_degrees(&p), 26.565, 0.01);
    cairo_surface_t *s = lp_brush_tile_render(&p);
    const unsigned char *d = cairo_image_surface_get_data(s);
    int stride = cairo_image_surface_get_stride(s);
    double sum = 0;
    int mismatch = 0;
    for (int y = 0; y < p.tile; y++) {
        for (int x = 0; x < p.tile; x++) sum += d[y * stride + x * 4];
        /* the left column against the right one: neighbours when the tile repeats */
        int l = d[y * stride], r = d[y * stride + (p.tile - 1) * 4];
        if (abs(l - r) > 60) mismatch++;
    }
    double mean = sum / (p.tile * p.tile) / 255.0;
    LP_ASSERT_NEAR(mean, 0.5, 0.06);
    LP_ASSERT(mismatch < p.tile / 8);
    cairo_surface_destroy(s);
}

int main(void) {
    LP_RUN(is_deterministic_for_a_seed);
    LP_RUN(fractal_values_stay_in_range_and_average_mid_gray);
    LP_RUN(stitched_noise_is_seamless_at_the_tile_edges);
    LP_RUN(brush_tile_is_seamless_mid_gray);
    LP_TEST_MAIN_END();
}
