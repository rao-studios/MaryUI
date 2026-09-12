/*
 * Which tier a mark is drawn at, and the dispatch to it — tiers.ts plus the
 * branch Icon.tsx makes around it. The passes themselves are
 * src/draw/lp_object_icon.c, the way ObjectIcon.tsx sits under Icon.tsx.
 */

#include "maryui/lp_icon.h"
#include "maryui/lp_object_icon.h"
#include "maryui/lp_tokens.h"

enum lp_icon_tier lp_icon_tier(float size) {
    if (size <= LP_OBJECT_TIER_GLYPH_MAX) return LP_TIER_GLYPH;
    if (size <= LP_OBJECT_TIER_PLAIN_MAX) return LP_TIER_PLAIN;
    return LP_TIER_LIT;
}

void lp_icon_paint_object(cairo_t *cr, lp_icon icon, lp_rect box, float stroke_width, lp_color color,
                          const lp_settings *settings) {
    float size = box.w < box.h ? box.w : box.h;
    /* An object of the mark's own name is its lit tier; anything else, or a
     * size that has not earned the lighting, stays the stroked glyph. */
    lp_object obj = icon < LP_ICON_COUNT ? lp_object_by_name(LP_ICON_NAMES[icon]) : LP_OBJ_COUNT;
    if (obj < LP_OBJ_COUNT && lp_icon_tier(size) != LP_TIER_GLYPH) {
        lp_object_icon_draw(cr, obj, box, settings);
        return;
    }
    lp_icon_draw(cr, icon, box.x, box.y, size, stroke_width, color);
}
