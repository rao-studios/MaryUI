#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/lp_blur.h"

/* One kernel is reused across the passes of a blur and usually across calls
 * (the same sigma recurs), so it is cached rather than rebuilt each time. */
static float *kernel_cache;
static float kernel_sigma = -1;
static int kernel_radius;

/* The vertical pass accumulates a row at a time. Emboss layers blur a handful
 * of rows hundreds of times a frame, so this buffer is grown and kept rather
 * than allocated per call. Single-threaded, like the kernel and sprite caches. */
static float *acc_row;
static int acc_row_len;

static float *accumulator(int w) {
    if (w > acc_row_len) {
        float *p = realloc(acc_row, sizeof(float) * (size_t)w);
        if (!p) return NULL;
        acc_row = p;
        acc_row_len = w;
    }
    return acc_row;
}

static int kernel(float sigma, float **out) {
    if (kernel_cache && sigma == kernel_sigma) {
        *out = kernel_cache;
        return kernel_radius;
    }
    int radius = (int)ceilf(sigma * 3);
    if (radius < 1) radius = 1;
    int n = radius * 2 + 1;
    float *k = realloc(kernel_cache, sizeof(float) * (size_t)n);
    if (!k) { *out = NULL; return 0; }
    float sum = 0;
    for (int i = 0; i < n; i++) {
        float d = (float)(i - radius);
        k[i] = expf(-d * d / (2 * sigma * sigma));
        sum += k[i];
    }
    for (int i = 0; i < n; i++) k[i] /= sum;
    kernel_cache = k;
    kernel_sigma = sigma;
    kernel_radius = radius;
    *out = k;
    return radius;
}

void lp_blur_plane(float *plane, int w, int h, float sigma, float *scratch) {
    if (sigma <= 0 || w <= 0 || h <= 0) return;
    float *k;
    int r = kernel(sigma, &k);
    if (!k) return;
    /* horizontal: plane -> scratch */
    for (int y = 0; y < h; y++) {
        const float *row = plane + (size_t)y * w;
        float *out = scratch + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            float acc = 0;
            for (int i = -r; i <= r; i++) {
                int sx = x + i;
                if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
                acc += row[sx] * k[i + r];
            }
            out[x] = acc;
        }
    }
    /* vertical: scratch -> plane. Row-major order, so that each tap of the
     * kernel walks contiguous memory instead of striding a whole row: the
     * accumulator is carried in a row buffer rather than a scalar. */
    float *acc = accumulator(w);
    if (!acc) return;
    for (int y = 0; y < h; y++) {
        memset(acc, 0, (size_t)w * sizeof *acc);
        for (int i = -r; i <= r; i++) {
            int sy = y + i;
            if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
            const float *src = scratch + (size_t)sy * w;
            float kw = k[i + r];
            for (int x = 0; x < w; x++) acc[x] += src[x] * kw;
        }
        memcpy(plane + (size_t)y * w, acc, (size_t)w * sizeof *acc);
    }
}

/* channels: a mask of which of B,G,R,A (bits 0..3) to blur. */
static void blur_channels(cairo_surface_t *surface, float sigma, int channels) {
    cairo_surface_flush(surface);
    int w = cairo_image_surface_get_width(surface), h = cairo_image_surface_get_height(surface);
    int stride = cairo_image_surface_get_stride(surface);
    unsigned char *data = cairo_image_surface_get_data(surface);
    if (!data || sigma <= 0 || w <= 0 || h <= 0) return;
    size_t n = (size_t)w * h;
    float *plane = malloc(sizeof(float) * n), *scratch = malloc(sizeof(float) * n);
    if (!plane || !scratch) { free(plane); free(scratch); return; }
    for (int c = 0; c < 4; c++) {
        if (!(channels & (1 << c))) continue;
        for (int y = 0; y < h; y++) {
            const unsigned char *row = data + (size_t)y * stride;
            float *dst = plane + (size_t)y * w;
            for (int x = 0; x < w; x++) dst[x] = row[x * 4 + c];
        }
        lp_blur_plane(plane, w, h, sigma, scratch);
        for (int y = 0; y < h; y++) {
            unsigned char *row = data + (size_t)y * stride;
            const float *src = plane + (size_t)y * w;
            for (int x = 0; x < w; x++) {
                float v = src[x];
                row[x * 4 + c] = (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v + 0.5f));
            }
        }
    }
    free(plane);
    free(scratch);
    cairo_surface_mark_dirty(surface);
}

void lp_blur_surface(cairo_surface_t *surface, float sigma) { blur_channels(surface, sigma, 0xF); }

void lp_blur_surface_tinted(cairo_surface_t *surface, float sigma, lp_color tint) {
    /* A shadow layer is one flat colour, so in premultiplied ARGB every colour
     * channel is that constant times alpha. Blurring alpha and rebuilding the
     * rest is identical to blurring all four, and a quarter of the work. */
    cairo_surface_flush(surface);
    blur_channels(surface, sigma, 1 << 3);
    int w = cairo_image_surface_get_width(surface), h = cairo_image_surface_get_height(surface);
    int stride = cairo_image_surface_get_stride(surface);
    unsigned char *data = cairo_image_surface_get_data(surface);
    if (!data) return;
    for (int y = 0; y < h; y++) {
        unsigned char *row = data + (size_t)y * stride;
        for (int x = 0; x < w; x++) {
            float a = row[x * 4 + 3] / 255.0f;
            row[x * 4 + 0] = (unsigned char)(tint.b * a * 255 + 0.5f);
            row[x * 4 + 1] = (unsigned char)(tint.g * a * 255 + 0.5f);
            row[x * 4 + 2] = (unsigned char)(tint.r * a * 255 + 0.5f);
        }
    }
    cairo_surface_mark_dirty(surface);
}
