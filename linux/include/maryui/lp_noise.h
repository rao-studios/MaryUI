/* feTurbulence, as the SVG 1.1 specification (§15.17) defines it: the same
 * seeded lattice, gradients, s-curve and stitching as every browser, so a
 * tile rendered here matches the one the web app bakes from an SVG filter. */
#ifndef MARYUI_LP_NOISE_H
#define MARYUI_LP_NOISE_H

#include <stdint.h>

#define LP_NOISE_BSIZE 0x100

typedef struct lp_turbulence {
    uint32_t lattice[LP_NOISE_BSIZE + LP_NOISE_BSIZE + 2];
    double gradient[4][LP_NOISE_BSIZE + LP_NOISE_BSIZE + 2][2];
} lp_turbulence;

/* stitchTiles="stitch": the primitive subregion the noise must repeat across. */
typedef struct lp_stitch {
    double x, y, w, h;
} lp_stitch;

void lp_turbulence_init(lp_turbulence *t, int seed);

/* The raw sum for one color channel (0 = R, 1 = G, 2 = B, 3 = A) at a point
 * in filter user space. fractal != 0 selects type="fractalNoise" (the caller
 * maps the sum with lp_noise_fractal_value), else type="turbulence". */
double lp_turbulence_sum(const lp_turbulence *t, int channel, double x, double y,
                         double base_fx, double base_fy, int octaves, int fractal, const lp_stitch *stitch);

/* fractalNoise: (sum + 1) / 2, clamped to 0..1. turbulence: sum clamped. */
double lp_noise_fractal_value(double sum);
double lp_noise_turbulence_value(double sum);

#endif
