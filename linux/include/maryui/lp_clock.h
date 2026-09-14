/* The ambient clock (Linux only, PARITY D13): the time, top right on the wallpaper, its letters cut from brushed
 * platinum — the desktop's one sheet of metal showing through the glyphs, over a dark keyline and a soft shadow so
 * the pale metal reads on any wallpaper. No plate behind it. The compositor (src/compositor/desktop.c) and
 * lp-render --clock paint it through the same call. */
#ifndef MARYUI_LP_CLOCK_H
#define MARYUI_LP_CLOCK_H

#include <cairo.h>
#include <stddef.h>
#include <time.h>

#include "maryui/lp_settings.h"
#include "maryui/lp_types.h"

#define LP_CLOCK_SIZE 15.0f     /* text.lg: large and bold enough for the grain to show inside a letter */
#define LP_CLOCK_INSET 6.0f     /* from the box's right edge, room for the keyline and the shadow */

/* The clock's words for a moment: the weekday, the month and the day, then the time — "Mon Sep 14 6:21 AM", or
 * "Mon Sep 14 06:21" in 24-hour time (System Settings › General). English abbreviations, as the C locale gives. */
void lp_clock_format(const struct tm *tm, int hours24, char *out, size_t n);
/* Where the time's line sits for `text`: right-aligned LP_CLOCK_INSET inside `box`, centred on its height. */
lp_rect lp_clock_bounds(cairo_t *cr, lp_rect box, const char *text);
/* Paints the clock into `box` (the chrome's rectangle, in its own coordinates). world_x/world_y is where the box's
 * origin sits on the desktop, so the letters' grain is cut from the desktop's one sheet of metal — pass the frame's
 * position, never add the box's own origin on top (see the brushed-metal note). */
void lp_clock_paint(cairo_t *cr, lp_rect box, const char *text, float world_x, float world_y, const lp_settings *settings);

#endif
