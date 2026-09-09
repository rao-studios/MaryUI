/* Monogram — the Rao mark: a serif R and its mirror image sharing a stem.
 * `flat` is a single colour (the menu bar); `platinum` fills the mark with
 * the metal ramp, the brushed grain and a fixed specular for hero use. Set
 * in the display font, like SvgDefs.tsx, until the mark is traced. */
#ifndef MARYUI_LP_MONOGRAM_H
#define MARYUI_LP_MONOGRAM_H

#include "maryui/lp_ui.h"

enum lp_monogram_variant { LP_MONOGRAM_FLAT, LP_MONOGRAM_PLATINUM };

void lp_monogram_paint(cairo_t *cr, lp_rect box, enum lp_monogram_variant variant, lp_color color);
void lp_monogram(lp_ctx *ctx, lp_rect box, enum lp_monogram_variant variant, lp_color color);

#endif
