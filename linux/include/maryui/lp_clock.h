/* The ambient clock (Linux only, PARITY D13): the time, top right on the wallpaper, on a brushed
 * platinum capsule sized to its text — dark embossed ink on metal, readable over any wallpaper. The
 * compositor (src/compositor/desktop.c) and lp-render --clock paint it through the same call. */
#ifndef MARYUI_LP_CLOCK_H
#define MARYUI_LP_CLOCK_H

#include <cairo.h>

#include "maryui/lp_settings.h"
#include "maryui/lp_types.h"

#define LP_CLOCK_H 22.0f
#define LP_CLOCK_PAD 12.0f

/* Where the capsule sits for `text`: right-aligned in `box`, centred on its height. */
lp_rect lp_clock_capsule(cairo_t *cr, lp_rect box, const char *text);
/* Paints the clock into `box` (the chrome's rectangle, in its own coordinates). world_x/world_y is where
 * the box's origin sits on the desktop, so the capsule's grain is cut from the desktop's one sheet of
 * metal — pass the frame's position, never add the box's own origin on top (see the brushed-metal note). */
void lp_clock_paint(cairo_t *cr, lp_rect box, const char *text, float world_x, float world_y, const lp_settings *settings);

#endif
