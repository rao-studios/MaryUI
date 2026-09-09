/* The icon set from icons.json (lp_icons.h), stroked like Icon.tsx: 24×24
 * viewBox, round caps and joins, stroke width 1.7 at size 24. */
#ifndef MARYUI_LP_ICON_H
#define MARYUI_LP_ICON_H

#include <cairo.h>

#include "maryui/lp_icons.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_types.h"

/* Draws the icon scaled to fit `size` px at (x, y). stroke_width is in viewBox units (LP_ICON_STROKE by default). */
void lp_icon_draw(cairo_t *cr, lp_icon icon, float x, float y, float size, float stroke_width, lp_color color);

/* The Finder's file tile: a raised platinum well (accent-tinted for folders) with the kind's icon inside. */
void lp_file_icon_paint(cairo_t *cr, lp_rect r, lp_icon icon, int folder, lp_accent accent);

/* Looks an icon up by its JSON name ("chevronLeft"); LP_ICON_COUNT when unknown. */
lp_icon lp_icon_by_name(const char *name);

#endif
