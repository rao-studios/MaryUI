#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/lp_draw.h"
#include "maryui/lp_shadow.h"

void lp_shadow_extents(const lp_shadow_layer *layers, int n, int *left, int *top, int *right, int *bottom) {
    float l = 0, t = 0, r = 0, b = 0;
    for (int i = 0; i < n; i++) {
        const lp_shadow_layer *s = &layers[i];
        if (s->inset) continue;
        float reach = s->blur * 1.25f + s->spread;
        l = fmaxf(l, reach - s->x);
        r = fmaxf(r, reach + s->x);
        t = fmaxf(t, reach - s->y);
        b = fmaxf(b, reach + s->y);
    }
    if (left) *left = (int)ceilf(fmaxf(0, l));
    if (top) *top = (int)ceilf(fmaxf(0, t));
    if (right) *right = (int)ceilf(fmaxf(0, r));
    if (bottom) *bottom = (int)ceilf(fmaxf(0, b));
}

#define CACHE_MAX 16
static lp_shadow_sprite cache[CACHE_MAX];
static int cache_count = 0;
static unsigned cache_clock = 0;

const lp_shadow_sprite *lp_shadow_get(const lp_shadow_layer *layers, int n, float radius) {
    /* Quantise: rendering a sprite means blurring at the layer's sigma, so an
     * unrounded radius from a live spring would miss the cache every frame.
     * The sprite is stretched to fit anyway. */
    radius = roundf(radius);
    for (int i = 0; i < cache_count; i++) {
        if (cache[i].layers == layers && cache[i].count == n && cache[i].radius == radius) {
            cache[i].used = ++cache_clock;
            return &cache[i];
        }
    }
    int l, t, r, b;
    lp_shadow_extents(layers, n, &l, &t, &r, &b);
    int extent = l > t ? l : t;
    if (r > extent) extent = r;
    if (b > extent) extent = b;
    extent += 2;
    int corner = (int)ceilf(radius) + extent + 2;
    int box = 2 * corner + 16;
    int size = box + 2 * extent;
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(surface);
    lp_draw_outer_shadows(cr, LP_RECT(extent, extent, box, box), radius, layers, n);
    cairo_destroy(cr);
    cairo_surface_flush(surface);
    /* Evict the least recently used, not always the last slot: clobbering one
     * slot leaves the other fifteen holding radii that can never match again. */
    int slot = cache_count;
    if (cache_count < CACHE_MAX) {
        cache_count++;
    } else {
        slot = 0;
        for (int i = 1; i < CACHE_MAX; i++) {
            if (cache[i].used < cache[slot].used) slot = i;
        }
    }
    lp_shadow_sprite *s = &cache[slot];
    if (s->surface) cairo_surface_destroy(s->surface);
    *s = (lp_shadow_sprite){ surface, extent, corner + extent, box, layers, n, radius, ++cache_clock };
    return s;
}

/* Copies sprite region (sx, sy, sw, sh) to destination (dx, dy, dw, dh). */
static void blit(cairo_t *cr, cairo_surface_t *src, double sx, double sy, double sw, double sh, double dx, double dy, double dw, double dh) {
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    cairo_save(cr);
    cairo_translate(cr, dx, dy);
    cairo_scale(cr, dw / sw, dh / sh);
    cairo_set_source_surface(cr, src, -sx, -sy);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
    cairo_rectangle(cr, 0, 0, sw, sh);
    cairo_clip(cr);
    cairo_paint(cr);
    cairo_restore(cr);
}

void lp_shadow_paint(cairo_t *cr, const lp_shadow_sprite *s, lp_rect r) {
    double e = s->extent, c = s->corner;
    double size = s->box + 2 * e;
    double mid = size - 2 * c;               /* the stretchable middle of the sprite */
    double dx = r.x - e, dy = r.y - e;       /* destination outer box */
    double dw = r.w + 2 * e, dh = r.h + 2 * e;
    /* A box shorter than two corner cells (a shaded window is 28 px tall under an
     * 82 px blur) takes the outer half of each cell instead, so the cells never
     * overlap and draw the shadow twice. */
    double cw = dw < 2 * c ? dw / 2 : c, ch = dh < 2 * c ? dh / 2 : c;
    double dmw = dw - 2 * cw, dmh = dh - 2 * ch;
    cairo_surface_t *src = s->surface;
    /* corners */
    blit(cr, src, 0, 0, cw, ch, dx, dy, cw, ch);
    blit(cr, src, size - cw, 0, cw, ch, dx + dw - cw, dy, cw, ch);
    blit(cr, src, 0, size - ch, cw, ch, dx, dy + dh - ch, cw, ch);
    blit(cr, src, size - cw, size - ch, cw, ch, dx + dw - cw, dy + dh - ch, cw, ch);
    /* edges */
    blit(cr, src, c, 0, mid, ch, dx + cw, dy, dmw, ch);
    blit(cr, src, c, size - ch, mid, ch, dx + cw, dy + dh - ch, dmw, ch);
    blit(cr, src, 0, c, cw, mid, dx, dy + ch, cw, dmh);
    blit(cr, src, size - cw, c, cw, mid, dx + dw - cw, dy + ch, cw, dmh);
    /* middle */
    blit(cr, src, c, c, mid, mid, dx + cw, dy + ch, dmw, dmh);
}

void lp_draw_shadow_9slice(cairo_t *cr, lp_rect r, float radius, const lp_shadow_layer *layers, int n) {
    lp_shadow_paint(cr, lp_shadow_get(layers, n, radius), r);
}
