#include <math.h>
#include <stdlib.h>

#include "maryui/lp_blur.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_texture.h"
#include "maryui/lp_tokens.h"

void lp_set_color(cairo_t *cr, lp_color c) { cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a); }

lp_color lp_color_mix(lp_color a, lp_color b, float t) {
    return LP_RGBA(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

lp_color lp_color_with_alpha(lp_color c, float alpha) { c.a = alpha; return c; }

int lp_clip_intersects(cairo_t *cr, lp_rect r) {
    cairo_rectangle_list_t *list = cairo_copy_clip_rectangle_list(cr);
    if (!list) return 1;
    if (list->status != CAIRO_STATUS_SUCCESS) {
        /* Not representable as rectangles: fall back to painting. */
        cairo_rectangle_list_destroy(list);
        return 1;
    }
    int hit = 0;
    for (int i = 0; i < list->num_rectangles && !hit; i++) {
        const cairo_rectangle_t *c = &list->rectangles[i];
        if (c->x < r.x + r.w && c->x + c->width > r.x && c->y < r.y + r.h && c->y + c->height > r.y) hit = 1;
    }
    cairo_rectangle_list_destroy(list);
    return hit;
}

lp_distant_light lp_distant_light_make(double azimuth_deg, double elevation_deg) {
    double az = azimuth_deg * M_PI / 180.0, el = elevation_deg * M_PI / 180.0;
    double lx = cos(az) * cos(el), ly = sin(az) * cos(el), lz = sin(el);
    /* The eye is straight on, so the halfway vector is L + (0, 0, 1). */
    double hx = lx, hy = ly, hz = lz + 1;
    double n = sqrt(hx * hx + hy * hy + hz * hz);
    return (lp_distant_light){ hx / n, hy / n, hz / n };
}

double lp_specular_at(const float *alpha, int w, int h, int x, int y, double surface_scale, double ks,
                      double exponent, lp_distant_light l) {
#define AT(xx, yy) alpha[(size_t)((yy) < 0 ? 0 : ((yy) >= h ? h - 1 : (yy))) * w + ((xx) < 0 ? 0 : ((xx) >= w ? w - 1 : (xx)))]
    /* The Sobel surface normal of the height map, per the spec's interior kernel. */
    double nx = -surface_scale * 0.25 * ((AT(x + 1, y - 1) + 2 * AT(x + 1, y) + AT(x + 1, y + 1)) -
                                         (AT(x - 1, y - 1) + 2 * AT(x - 1, y) + AT(x - 1, y + 1)));
    double ny = -surface_scale * 0.25 * ((AT(x - 1, y + 1) + 2 * AT(x, y + 1) + AT(x + 1, y + 1)) -
                                         (AT(x - 1, y - 1) + 2 * AT(x, y - 1) + AT(x + 1, y - 1)));
#undef AT
    double nn = sqrt(nx * nx + ny * ny + 1);
    double ndoth = (nx * l.hx + ny * l.hy + l.hz) / nn;
    double spec = ndoth > 0 ? ks * pow(ndoth, exponent) : 0;
    return spec > 1 ? 1 : spec;
}

static float clamp_radius(lp_rect r, float radius) {
    float m = fminf(r.w, r.h) / 2;
    return radius < 0 ? 0 : (radius > m ? m : radius);
}

void lp_path_rrect4(cairo_t *cr, lp_rect r, float tl, float tr, float br, float bl) {
    tl = clamp_radius(r, tl); tr = clamp_radius(r, tr); br = clamp_radius(r, br); bl = clamp_radius(r, bl);
    cairo_new_sub_path(cr);
    cairo_arc(cr, r.x + r.w - tr, r.y + tr, tr, -M_PI / 2, 0);
    cairo_arc(cr, r.x + r.w - br, r.y + r.h - br, br, 0, M_PI / 2);
    cairo_arc(cr, r.x + bl, r.y + r.h - bl, bl, M_PI / 2, M_PI);
    cairo_arc(cr, r.x + tl, r.y + tl, tl, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

void lp_path_rrect(cairo_t *cr, lp_rect r, float radius) { lp_path_rrect4(cr, r, radius, radius, radius, radius); }

void lp_fill_vgradient(cairo_t *cr, lp_rect r, lp_color top, lp_color bottom, float radius) {
    cairo_pattern_t *p = cairo_pattern_create_linear(0, r.y, 0, r.y + r.h);
    cairo_pattern_add_color_stop_rgba(p, 0, top.r, top.g, top.b, top.a);
    cairo_pattern_add_color_stop_rgba(p, 1, bottom.r, bottom.g, bottom.b, bottom.a);
    lp_path_rrect(cr, r, radius);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
}

void lp_fill_solid(cairo_t *cr, lp_rect r, lp_color c, float radius) {
    lp_path_rrect(cr, r, radius);
    lp_set_color(cr, c);
    cairo_fill(cr);
}

void lp_fill_hsplit(cairo_t *cr, lp_rect r, lp_color a, lp_color b, float split, float radius) {
    cairo_pattern_t *p = cairo_pattern_create_linear(r.x, 0, r.x + r.w, 0);
    if (split < 0) split = 0;
    if (split > 1) split = 1;
    cairo_pattern_add_color_stop_rgba(p, 0, a.r, a.g, a.b, a.a);
    cairo_pattern_add_color_stop_rgba(p, split, a.r, a.g, a.b, a.a);
    cairo_pattern_add_color_stop_rgba(p, split, b.r, b.g, b.b, b.a);
    cairo_pattern_add_color_stop_rgba(p, 1, b.r, b.g, b.b, b.a);
    lp_path_rrect(cr, r, radius);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
}

/* CSS blur radius ≈ 2σ. */
static float sigma_for(float blur) { return blur / 2.0f; }

/* One shadow layer rendered into a temporary ARGB surface positioned at
 * (ox, oy) in user space, then painted. inset: the colour fills the box and
 * the offset/spread-shrunk shape is cleared; outer: the offset/spread-grown
 * shape is filled. Either way a blur softens it before it is clipped. */
static void paint_layer(cairo_t *cr, lp_rect r, float radius, const lp_shadow_layer *l) {
    float margin = ceilf(fabsf(l->blur) * 1.5f + fabsf(l->spread) + fabsf(l->x) + fabsf(l->y)) + 2;
    /* Only the part that can show: the current clip grown by the blur's reach. A
     * title-strip repaint of a tall window then blurs a strip, not the window. */
    double cx1, cy1, cx2, cy2;
    cairo_clip_extents(cr, &cx1, &cy1, &cx2, &cy2);
    float ax = fmaxf(r.x - margin, (float)floor(cx1) - margin), ay = fmaxf(r.y - margin, (float)floor(cy1) - margin);
    float bx = fminf(r.x + r.w + margin, (float)ceil(cx2) + margin), by = fminf(r.y + r.h + margin, (float)ceil(cy2) + margin);
    int w = (int)ceilf(bx - ax), h = (int)ceilf(by - ay);
    if (w <= 0 || h <= 0) return;
    cairo_surface_t *tmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *tc = cairo_create(tmp);
    lp_rect local = { r.x - ax, r.y - ay, r.w, r.h };
    if (l->inset) {
        lp_set_color(tc, l->color);
        cairo_paint(tc);
        cairo_set_operator(tc, CAIRO_OPERATOR_CLEAR);
        lp_rect hole = { local.x + l->x + l->spread, local.y + l->y + l->spread, local.w - 2 * l->spread, local.h - 2 * l->spread };
        lp_path_rrect(tc, hole, radius - l->spread);
        cairo_fill(tc);
    } else {
        lp_rect box = { local.x + l->x - l->spread, local.y + l->y - l->spread, local.w + 2 * l->spread, local.h + 2 * l->spread };
        lp_path_rrect(tc, box, radius + l->spread);
        lp_set_color(tc, l->color);
        cairo_fill(tc);
    }
    cairo_destroy(tc);
    /* The layer is one flat colour, so only its alpha carries shape. */
    if (l->blur > 0) lp_blur_surface_tinted(tmp, sigma_for(l->blur), l->color);

    cairo_save(cr);
    if (l->inset) {
        lp_path_rrect(cr, r, radius);
        cairo_clip(cr);
    } else {
        /* Outside the shape only: the element paints over its own box. */
        cairo_rectangle(cr, ax, ay, w, h);
        lp_path_rrect(cr, r, radius);
        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_clip(cr);
    }
    cairo_set_source_surface(cr, tmp, ax, ay);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_surface_destroy(tmp);
}

void lp_draw_inset_shadows(cairo_t *cr, lp_rect r, float radius, const lp_shadow_layer *layers, int n) {
    for (int i = 0; i < n; i++) if (layers[i].inset) paint_layer(cr, r, radius, &layers[i]);
}

void lp_draw_outer_shadows(cairo_t *cr, lp_rect r, float radius, const lp_shadow_layer *layers, int n) {
    /* CSS paints the first layer on top: draw in reverse. */
    for (int i = n - 1; i >= 0; i--) if (!layers[i].inset) paint_layer(cr, r, radius, &layers[i]);
}

void lp_draw_box_shadow(cairo_t *cr, lp_rect r, float radius, const lp_shadow_layer *layers, int n) {
    lp_draw_outer_shadows(cr, r, radius, layers, n);
    lp_draw_inset_shadows(cr, r, radius, layers, n);
}

void lp_draw_brush(cairo_t *cr, lp_rect r, float radius, float opacity, float world_x, float world_y) {
    if (opacity <= 0) return;
    cairo_surface_t *tile = lp_brush_tile();
    cairo_save(cr);
    lp_path_rrect(cr, r, radius);
    cairo_clip(cr);
    cairo_pattern_t *p = cairo_pattern_create_for_surface(tile);
    cairo_pattern_set_extend(p, CAIRO_EXTEND_REPEAT);
    /* Sample the tile at desktop coordinates: the pattern matrix maps user space
     * (surface-local) to pattern space, so translating it by the surface's world
     * origin anchors the grain to the desktop. Reduced modulo the tile, which is
     * seamless, so the result is identical and the numbers stay small. Whole
     * pixels, because a fractional translate takes pixman off its tiled-repeat
     * fast path and half a pixel of noise is not visible. */
    float period = LP_BRUSH_TILE > 1 ? LP_BRUSH_TILE : 1;
    float ox = fmodf(roundf(world_x), period), oy = fmodf(roundf(world_y), period);
    if (ox != 0 || oy != 0) {
        cairo_matrix_t m;
        cairo_matrix_init_translate(&m, ox, oy);
        cairo_pattern_set_matrix(p, &m);
    }
    cairo_set_source(cr, p);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVERLAY);
    cairo_paint_with_alpha(cr, opacity);
    cairo_pattern_destroy(p);
    cairo_restore(cr);
}

void lp_draw_sheen(cairo_t *cr, lp_rect r, float radius, float sheen_x, float tilt_deg, float alpha) {
    if (alpha <= 0) return;
    /* The band element: 200% wide, 160% tall, centred on the surface. */
    double ew = r.w * 2.0, eh = r.h * 1.6;
    double angle = LP_SHEEN_ANGLE_DEG * M_PI / 180.0;
    double dx = sin(angle), dy = -cos(angle);
    double len = fabs(ew * dx) + fabs(eh * dy);
    cairo_save(cr);
    lp_path_rrect(cr, r, radius);
    cairo_clip(cr);
    cairo_translate(cr, r.x + r.w / 2.0, r.y + r.h / 2.0);
    cairo_translate(cr, (sheen_x - 0.5) * r.w, 0);
    cairo_rotate(cr, tilt_deg * M_PI / 180.0);
    cairo_pattern_t *p = cairo_pattern_create_linear(-dx * len / 2, -dy * len / 2, dx * len / 2, dy * len / 2);
    lp_color c = LP_SHEEN_COLOR;
    cairo_pattern_add_color_stop_rgba(p, 0.36, c.r, c.g, c.b, 0);
    cairo_pattern_add_color_stop_rgba(p, 0.50, c.r, c.g, c.b, 1);
    cairo_pattern_add_color_stop_rgba(p, 0.64, c.r, c.g, c.b, 0);
    cairo_rectangle(cr, -ew / 2, -eh / 2, ew, eh);
    cairo_set_source(cr, p);
    cairo_set_operator(cr, CAIRO_OPERATOR_SCREEN);
    cairo_clip(cr);
    cairo_paint_with_alpha(cr, alpha);
    cairo_pattern_destroy(p);
    cairo_restore(cr);
}

void lp_draw_hairline(cairo_t *cr, lp_rect r, int edges, lp_color c) {
    lp_set_color(cr, c);
    if (edges & LP_EDGE_TOP) cairo_rectangle(cr, r.x, r.y - 1, r.w, 1);
    if (edges & LP_EDGE_BOTTOM) cairo_rectangle(cr, r.x, r.y + r.h, r.w, 1);
    if (edges & LP_EDGE_LEFT) cairo_rectangle(cr, r.x - 1, r.y, 1, r.h);
    if (edges & LP_EDGE_RIGHT) cairo_rectangle(cr, r.x + r.w, r.y, 1, r.h);
    cairo_fill(cr);
}

void lp_draw_focus_ring(cairo_t *cr, lp_rect r, float radius, lp_color c, float width) {
    lp_rect outer = { r.x - width, r.y - width, r.w + 2 * width, r.h + 2 * width };
    cairo_save(cr);
    lp_path_rrect(cr, outer, radius + width);
    lp_path_rrect(cr, r, radius);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    lp_set_color(cr, c);
    cairo_fill(cr);
    cairo_restore(cr);
}
