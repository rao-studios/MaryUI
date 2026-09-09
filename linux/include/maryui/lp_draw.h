/* Cairo primitives every component is built from: the vertical gradient, the
 * emboss (CSS inset box-shadows), the brushed grain (OVERLAY), the sliding
 * sheen (SCREEN), hairlines, focus rings and outer shadows. Coordinates are
 * CSS pixels in the current cairo user space. */
#ifndef MARYUI_LP_DRAW_H
#define MARYUI_LP_DRAW_H

#include <cairo.h>

#include "maryui/lp_types.h"

void lp_set_color(cairo_t *cr, lp_color c);
/* color-mix(in srgb, a (1-t), b t) */
lp_color lp_color_mix(lp_color a, lp_color b, float t);
lp_color lp_color_with_alpha(lp_color c, float alpha);

/* Rounded rectangle paths; the radius is clamped to half the shorter side. */
void lp_path_rrect(cairo_t *cr, lp_rect r, float radius);
void lp_path_rrect4(cairo_t *cr, lp_rect r, float tl, float tr, float br, float bl);

/* linear-gradient(180deg, top, bottom) inside a rounded rectangle. */
void lp_fill_vgradient(cairo_t *cr, lp_rect r, lp_color top, lp_color bottom, float radius);
void lp_fill_solid(cairo_t *cr, lp_rect r, lp_color c, float radius);
/* linear-gradient(to right, a <pct>, b <pct>) — the slider rail. */
void lp_fill_hsplit(cairo_t *cr, lp_rect r, lp_color a, lp_color b, float split, float radius);

/* CSS box-shadow layers. Inset layers are clipped inside the shape; outer
 * layers are painted around it (the shape itself is left for the caller). */
void lp_draw_inset_shadows(cairo_t *cr, lp_rect r, float radius, const lp_shadow_layer *layers, int n);
void lp_draw_outer_shadows(cairo_t *cr, lp_rect r, float radius, const lp_shadow_layer *layers, int n);
/* Both, from one list: inset layers inside, others outside. */
void lp_draw_box_shadow(cairo_t *cr, lp_rect r, float radius, const lp_shadow_layer *layers, int n);

/* The brush tile (lp_texture.h) under mix-blend-mode: overlay at `opacity`. */
void lp_draw_brush(cairo_t *cr, lp_rect r, float radius, float opacity);

/* Surface.module.css .sheen: a 200%-wide band at sheen.angle, screen-blended,
 * slid by sheen_x (0..1, 0.5 = centred) and rotated by tilt degrees. */
void lp_draw_sheen(cairo_t *cr, lp_rect r, float radius, float sheen_x, float tilt_deg, float alpha);

/* A 1px line just outside an edge (the `0 1px 0 hairline` idiom). */
enum lp_edge { LP_EDGE_TOP = 1, LP_EDGE_RIGHT = 2, LP_EDGE_BOTTOM = 4, LP_EDGE_LEFT = 8 };
void lp_draw_hairline(cairo_t *cr, lp_rect r, int edges, lp_color c);

/* :focus-visible — a 3px ring outside the shape. */
void lp_draw_focus_ring(cairo_t *cr, lp_rect r, float radius, lp_color c, float width);

#endif
