/* The object tier: an icon drawn as a lit object rather than diagrammed as an
 * outline. Geometry is lp_objects.h (generated from objects.json, no colour in
 * it); every pass is the shared recipe under `object` in lp_tokens.h, so the
 * whole set regrades from the tokens the way the wallpaper does from `molten`.
 * Mirrors web/src/components/ObjectIcon.
 *
 * Nothing here animates or reads a clock: an idle desktop still schedules no
 * frames. */
#ifndef MARYUI_LP_OBJECT_ICON_H
#define MARYUI_LP_OBJECT_ICON_H

#include <cairo.h>

#include "maryui/lp_icons.h"
#include "maryui/lp_objects.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_types.h"

/*
 * What a name renders as at a size, from tiers.ts: a stroked glyph at or below
 * object.tier-glyph-max, the flat body alone to object.tier-plain-max, the full
 * lit recipe above. A function of the LOGICAL size only — never of the device
 * pixel ratio, or the same icon would be a different drawing on two monitors.
 */
enum lp_icon_tier { LP_TIER_GLYPH, LP_TIER_PLAIN, LP_TIER_LIT };
enum lp_icon_tier lp_icon_tier(float size);

/* LP_OBJ_COUNT when there is no object of that name. */
lp_object lp_object_by_name(const char *name);

/*
 * Draws the object to fill `box` (square; the smaller side wins). Below the
 * glyph tier it draws the object's declared glyph fallback through
 * lp_icon_draw instead, so a caller can hand any size to this and get the
 * right drawing. `settings` may be NULL: the defaults answer.
 */
void lp_object_icon_draw(cairo_t *cr, lp_object obj, lp_rect box, const lp_settings *settings);

/*
 * Icon.tsx's `variant="object"`: the object of this mark's name where one
 * exists and the size earns it, else the stroked glyph in `color`. The one
 * place that decision is made, so the Finder's tiles and Spotlight's dock
 * cannot drift apart.
 */
void lp_icon_paint_object(cairo_t *cr, lp_icon icon, lp_rect box, float stroke_width, lp_color color,
                          const lp_settings *settings);

#endif
