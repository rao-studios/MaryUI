#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/lp_blur.h"

static int kernel(float sigma, float **out) {
    int radius = (int)ceilf(sigma * 3);
    if (radius < 1) radius = 1;
    int n = radius * 2 + 1;
    float *k = malloc(sizeof(float) * (size_t)n);
    float sum = 0;
    for (int i = 0; i < n; i++) {
        float d = (float)(i - radius);
        k[i] = expf(-d * d / (2 * sigma * sigma));
        sum += k[i];
    }
    for (int i = 0; i < n; i++) k[i] /= sum;
    *out = k;
    return radius;
}

void lp_blur_plane(float *plane, int w, int h, float sigma, float *scratch) {
    if (sigma <= 0 || w <= 0 || h <= 0) return;
    float *k;
    int r = kernel(sigma, &k);
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
    /* vertical: scratch -> plane */
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            float acc = 0;
            for (int i = -r; i <= r; i++) {
                int sy = y + i;
                if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
                acc += scratch[(size_t)sy * w + x] * k[i + r];
            }
            plane[(size_t)y * w + x] = acc;
        }
    }
    free(k);
}

void lp_blur_surface(cairo_surface_t *surface, float sigma) {
    cairo_surface_flush(surface);
    int w = cairo_image_surface_get_width(surface), h = cairo_image_surface_get_height(surface);
    int stride = cairo_image_surface_get_stride(surface);
    unsigned char *data = cairo_image_surface_get_data(surface);
    if (!data || sigma <= 0) return;
    size_t n = (size_t)w * h;
    float *plane = malloc(sizeof(float) * n), *scratch = malloc(sizeof(float) * n);
    for (int c = 0; c < 4; c++) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) plane[(size_t)y * w + x] = data[(size_t)y * stride + x * 4 + c];
        }
        lp_blur_plane(plane, w, h, sigma, scratch);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                float v = plane[(size_t)y * w + x];
                data[(size_t)y * stride + x * 4 + c] = (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v + 0.5f));
            }
        }
    }
    free(plane);
    free(scratch);
    cairo_surface_mark_dirty(surface);
}
