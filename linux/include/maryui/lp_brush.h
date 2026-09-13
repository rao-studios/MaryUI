/* The hand-painted highlight under a credited passage (Mary's BrushStroke.swift and
 * ContributionSpans.swift, PARITY D27): a seeded stroke, unique per line segment and
 * the same on every redraw, filled with the owner's colour at 0.20 alpha. The seed is
 * djb2 of the span's id XOR the line index; the RNG is splitmix64; the five colours
 * are the Paper page's, anchored on Mary's gold, chosen by the owner id's djb2. */
#ifndef MARYUI_LP_BRUSH_H
#define MARYUI_LP_BRUSH_H

#include <cairo.h>
#include <stdint.h>

#include "maryui/lp_types.h"

#define LP_BRUSH_ALPHA 0.20f
#define LP_BRUSH_INFLATE_W 12
#define LP_BRUSH_INFLATE_H 8
#define LP_BRUSH_FADE_MS 700.0
#define LP_BRUSH_STAGGER_MS 150.0
#define LP_BRUSH_PALETTE 5

/* djb2 over the bytes (StableHash.of). */
int64_t lp_brush_hash(const char *s);
/* The seed for a span's stroke on one of its lines. */
int64_t lp_brush_seed(const char *span_id, int line);
/* The owner's colour (paletteColor). */
lp_color lp_brush_color(const char *owner_id);
lp_color lp_brush_palette(int index);
/* Appends the stroke's closed path for `rect` to cr (the caller fills). */
void lp_brush_path(cairo_t *cr, lp_rect rect, int64_t seed);
/* The fade-in's opacity for a stroke that began at `since_ms`, index `idx` in reading order: easeIn over 0.7 s
 * after a 0.15 s stagger per index; 1 at once when `instant`. */
float lp_brush_opacity(double now_ms, double since_ms, int idx, int instant);

#endif
