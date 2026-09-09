#include <math.h>
#include <stdlib.h>

#include "maryui/lp_noise.h"
#include "maryui/lp_texture.h"
#include "maryui/lp_tokens.h"

lp_brush_params lp_brush_params_from_tokens(void) {
    return (lp_brush_params){
        .freq_x = LP_BRUSH_FREQ_X, .freq_y = LP_BRUSH_FREQ_Y, .octaves = LP_BRUSH_OCTAVES, .seed = LP_BRUSH_SEED,
        .tile = (int)LP_BRUSH_TILE, .rise = LP_BRUSH_ANGLE_RISE, .run = LP_BRUSH_ANGLE_RUN,
        .contrast = LP_BRUSH_CONTRAST, .opacity = LP_BRUSH_OPACITY,
    };
}

double lp_brush_angle_degrees(const lp_brush_params *p) {
    return atan2((double)p->rise, (double)p->run) * 180.0 / M_PI;
}

static unsigned char to_byte(double v) {
    v = v < 0 ? 0 : (v > 1 ? 1 : v);
    return (unsigned char)(v * 255.0 + 0.5);
}

cairo_surface_t *lp_brush_tile_render(const lp_brush_params *p) {
    int n = p->tile;
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, n, n);
    cairo_surface_flush(surface);
    unsigned char *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);

    /* The pattern space: the noise repeats every `period` in x and y, and the
     * whole pattern is rotated by -angle. The rotated lattice contains
     * (tile, 0) and (0, tile), so the tile repeats seamlessly. */
    double period = n / sqrt((double)(p->rise * p->rise + p->run * p->run));
    double angle = -lp_brush_angle_degrees(p) * M_PI / 180.0;
    double c = cos(-angle), s = sin(-angle); /* inverse rotation: tile -> pattern space */
    lp_stitch stitch = { 0, 0, period, period };
    double slope = p->contrast, intercept = (1 - slope) / 2;

    lp_turbulence *t = malloc(sizeof(*t));
    lp_turbulence_init(t, p->seed);
    for (int y = 0; y < n; y++) {
        unsigned char *row = data + (size_t)y * stride;
        for (int x = 0; x < n; x++) {
            double px = x + 0.5, py = y + 0.5;
            double u = c * px - s * py, v = s * px + c * py;
            /* The browser rasterises one period of the pattern and repeats
             * the image; stitched noise is seamless across that period but
             * not periodic as a function, so sample inside it. */
            u = fmod(u, period); if (u < 0) u += period;
            v = fmod(v, period); if (v < 0) v += period;
            /* feTurbulence RGB channels, then feColorMatrix saturate(0), then the linear transfer. */
            double r = lp_noise_fractal_value(lp_turbulence_sum(t, 0, u, v, p->freq_x, p->freq_y, p->octaves, 1, &stitch));
            double g = lp_noise_fractal_value(lp_turbulence_sum(t, 1, u, v, p->freq_x, p->freq_y, p->octaves, 1, &stitch));
            double b = lp_noise_fractal_value(lp_turbulence_sum(t, 2, u, v, p->freq_x, p->freq_y, p->octaves, 1, &stitch));
            double lum = 0.213 * r + 0.715 * g + 0.072 * b;
            unsigned char gray = to_byte(slope * lum + intercept);
            row[x * 4 + 0] = gray;
            row[x * 4 + 1] = gray;
            row[x * 4 + 2] = gray;
            row[x * 4 + 3] = 0xff;
        }
    }
    free(t);
    cairo_surface_mark_dirty(surface);
    return surface;
}

lp_brush_params lp_brush_live;
static int live_initialised = 0;
static cairo_surface_t *shared_tile = NULL;

cairo_surface_t *lp_brush_tile(void) {
    if (!live_initialised) {
        lp_brush_live = lp_brush_params_from_tokens();
        live_initialised = 1;
    }
    if (!shared_tile) shared_tile = lp_brush_tile_render(&lp_brush_live);
    return shared_tile;
}

void lp_brush_tile_reset(void) {
    if (shared_tile) cairo_surface_destroy(shared_tile);
    shared_tile = NULL;
}
