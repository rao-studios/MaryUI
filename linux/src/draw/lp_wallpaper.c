#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "maryui/lp_blur.h"
#include "maryui/lp_noise.h"
#include "maryui/lp_tokens.h"
#include "maryui/lp_wallpaper.h"

/* The `folds` gradient stops from wallpaperSvg.ts. */
static const struct { double offset; unsigned int rgb; } STOPS[] = {
    { 0, 0xdfe1e6 }, { 0.1, 0x6d727c }, { 0.2, 0xd3d6dc }, { 0.29, 0x4a4e57 }, { 0.4, 0xc4c8cf },
    { 0.51, 0x5f636c }, { 0.6, 0xe6e8ec }, { 0.7, 0x3f434b }, { 0.81, 0xb8bcc4 }, { 0.9, 0x5a5e67 }, { 1, 0xd5d8de },
};
#define NSTOPS (sizeof(STOPS) / sizeof(STOPS[0]))

static void gradient_at(double t, double *r, double *g, double *b) {
    if (t <= 0) t = 0;
    if (t >= 1) t = 1;
    size_t i = 0;
    while (i + 1 < NSTOPS && STOPS[i + 1].offset < t) i++;
    double t0 = STOPS[i].offset, t1 = STOPS[i + 1 < NSTOPS ? i + 1 : i].offset;
    double f = t1 > t0 ? (t - t0) / (t1 - t0) : 0;
    unsigned int c0 = STOPS[i].rgb, c1 = STOPS[i + 1 < NSTOPS ? i + 1 : i].rgb;
    *r = (((c0 >> 16) & 255) * (1 - f) + ((c1 >> 16) & 255) * f) / 255.0;
    *g = (((c0 >> 8) & 255) * (1 - f) + ((c1 >> 8) & 255) * f) / 255.0;
    *b = ((c0 & 255) * (1 - f) + (c1 & 255) * f) / 255.0;
}

/* The rect the gradient is painted on: -20%..120% of the 1600×1000 canvas. */
#define RX (-0.2 * LP_WALLPAPER_W)
#define RY (-0.2 * LP_WALLPAPER_H)
#define RW (1.4 * LP_WALLPAPER_W)
#define RH (1.4 * LP_WALLPAPER_H)

/* linearGradient x1=0 y1=0 x2=1 y2=1 in objectBoundingBox units: t = (u + v) / 2. */
static void source_at(double x, double y, double *r, double *g, double *b) {
    double u = (x - RX) / RW, v = (y - RY) / RH;
    gradient_at((u + v) / 2, r, g, b);
}

cairo_surface_t *lp_wallpaper_render(int w, int h) {
    /* Cover: the SVG scaled so it fills w×h, the overflow centred. Everything
     * is evaluated in SVG user space at the output's resolution. */
    double scale = fmax((double)w / LP_WALLPAPER_W, (double)h / LP_WALLPAPER_H);
    double ox = (w - LP_WALLPAPER_W * scale) / 2, oy = (h - LP_WALLPAPER_H * scale) / 2;
    size_t n = (size_t)w * h;

    lp_turbulence *noise = malloc(sizeof(*noise));
    lp_turbulence *noise2 = malloc(sizeof(*noise2));
    lp_turbulence_init(noise, 11);
    lp_turbulence_init(noise2, 4);

    float *R = malloc(n * sizeof(float)), *G = malloc(n * sizeof(float)), *B = malloc(n * sizeof(float));
    float *A2 = malloc(n * sizeof(float)), *scratch = malloc(n * sizeof(float));

    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            double x = (px + 0.5 - ox) / scale, y = (py + 0.5 - oy) / scale;
            /* feDisplacementMap: scale 420, x from the noise's R, y from its G (unpremultiplied). */
            double dx = lp_noise_fractal_value(lp_turbulence_sum(noise, 0, x, y, 0.0016, 0.0026, 3, 1, NULL));
            double dy = lp_noise_fractal_value(lp_turbulence_sum(noise, 1, x, y, 0.0016, 0.0026, 3, 1, NULL));
            double sx = x + 420.0 * (dx - 0.5), sy = y + 420.0 * (dy - 0.5);
            double r, g, b;
            source_at(sx, sy, &r, &g, &b);
            size_t i = (size_t)py * w + px;
            R[i] = (float)r; G[i] = (float)g; B[i] = (float)b;
            /* The second noise's alpha channel is the bump map for the specular pass. */
            A2[i] = (float)lp_noise_fractal_value(lp_turbulence_sum(noise2, 3, x, y, 0.004, 0.006, 2, 1, NULL));
        }
    }
    free(noise);
    free(noise2);

    /* feGaussianBlur stdDeviation 1.5 on the folds, 3 on the bump map (user units → pixels). */
    lp_blur_plane(R, w, h, (float)(1.5 * scale), scratch);
    lp_blur_plane(G, w, h, (float)(1.5 * scale), scratch);
    lp_blur_plane(B, w, h, (float)(1.5 * scale), scratch);
    lp_blur_plane(A2, w, h, (float)(3.0 * scale), scratch);

    /* feSpecularLighting: surfaceScale 3, ks 0.7, exponent 26, white, distant light az 230° el 50°. */
    double az = 230.0 * M_PI / 180.0, el = 50.0 * M_PI / 180.0;
    double Lx = cos(az) * cos(el), Ly = sin(az) * cos(el), Lz = sin(el);
    double Hx = Lx, Hy = Ly, Hz = Lz + 1;
    double Hn = sqrt(Hx * Hx + Hy * Hy + Hz * Hz);
    Hx /= Hn; Hy /= Hn; Hz /= Hn;
    const double surface_scale = 3, ks = 0.7, exponent = 26;

    cairo_surface_t *out = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_surface_flush(out);
    unsigned char *data = cairo_image_surface_get_data(out);
    int stride = cairo_image_surface_get_stride(out);
#define AT(xx, yy) A2[(size_t)((yy) < 0 ? 0 : ((yy) >= h ? h - 1 : (yy))) * w + ((xx) < 0 ? 0 : ((xx) >= w ? w - 1 : (xx)))]
    for (int y = 0; y < h; y++) {
        unsigned char *row = data + (size_t)y * stride;
        for (int x = 0; x < w; x++) {
            double nx = -surface_scale * 0.25 * ((AT(x + 1, y - 1) + 2 * AT(x + 1, y) + AT(x + 1, y + 1)) - (AT(x - 1, y - 1) + 2 * AT(x - 1, y) + AT(x - 1, y + 1)));
            double ny = -surface_scale * 0.25 * ((AT(x - 1, y + 1) + 2 * AT(x, y + 1) + AT(x + 1, y + 1)) - (AT(x - 1, y - 1) + 2 * AT(x, y - 1) + AT(x + 1, y - 1)));
            double nn = sqrt(nx * nx + ny * ny + 1);
            double ndoth = (nx * Hx + ny * Hy + Hz) / nn;
            double spec = ndoth > 0 ? ks * pow(ndoth, exponent) : 0;
            if (spec > 1) spec = 1;
            /* feComposite arithmetic k2 = 0.35 (spec) k3 = 1 (soft), then alpha := 1. */
            size_t i = (size_t)y * w + x;
            double r = 0.35 * spec + R[i], g = 0.35 * spec + G[i], b = 0.35 * spec + B[i];
            row[x * 4 + 0] = (unsigned char)(fmin(1, b) * 255 + 0.5);
            row[x * 4 + 1] = (unsigned char)(fmin(1, g) * 255 + 0.5);
            row[x * 4 + 2] = (unsigned char)(fmin(1, r) * 255 + 0.5);
            row[x * 4 + 3] = 0xff;
        }
    }
#undef AT
    free(R); free(G); free(B); free(A2); free(scratch);
    cairo_surface_mark_dirty(out);
    return out;
}

void lp_wallpaper_vignette(cairo_t *cr, int w, int h) {
    /* radial-gradient(ellipse at 50% 40%, transparent 55%, rgba(20,22,28,0.35)) */
    cairo_save(cr);
    cairo_translate(cr, w / 2.0, h * 0.4);
    cairo_scale(cr, w / 2.0, h / 2.0);
    cairo_pattern_t *p = cairo_pattern_create_radial(0, 0, 0, 0, 0, 1);
    cairo_pattern_add_color_stop_rgba(p, 0.55, 20 / 255.0, 22 / 255.0, 28 / 255.0, 0);
    cairo_pattern_add_color_stop_rgba(p, 1.0, 20 / 255.0, 22 / 255.0, 28 / 255.0, 0.35);
    cairo_set_source(cr, p);
    cairo_rectangle(cr, -1.5, -1.5, 3, 3);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
    cairo_restore(cr);
}

static int mkdir_p(const char *path) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') { *p = 0; mkdir(buf, 0755); *p = '/'; }
    }
    return mkdir(buf, 0755) == 0 || 1;
}

cairo_surface_t *lp_wallpaper_cached(int w, int h) {
    char path[1400];
    const char *data_dir = getenv("MARYUI_DATA_DIR");
    if (data_dir && *data_dir) {
        snprintf(path, sizeof path, "%s/wallpaper-%dx%d.png", data_dir, w, h);
        cairo_surface_t *s = cairo_image_surface_create_from_png(path);
        if (cairo_surface_status(s) == CAIRO_STATUS_SUCCESS) return s;
        cairo_surface_destroy(s);
    }
    snprintf(path, sizeof path, "/usr/share/maryui/wallpaper-%dx%d.png", w, h);
    cairo_surface_t *s = cairo_image_surface_create_from_png(path);
    if (cairo_surface_status(s) == CAIRO_STATUS_SUCCESS) return s;
    cairo_surface_destroy(s);

    const char *cache = getenv("XDG_CACHE_HOME");
    char dir[1024];
    if (cache && *cache) snprintf(dir, sizeof dir, "%s/maryui", cache);
    else snprintf(dir, sizeof dir, "%s/.cache/maryui", getenv("HOME") ? getenv("HOME") : "/tmp");
    snprintf(path, sizeof path, "%s/wallpaper-%dx%d.png", dir, w, h);
    s = cairo_image_surface_create_from_png(path);
    if (cairo_surface_status(s) == CAIRO_STATUS_SUCCESS) return s;
    cairo_surface_destroy(s);

    s = lp_wallpaper_render(w, h);
    mkdir_p(dir);
    cairo_surface_write_to_png(s, path);
    return s;
}
