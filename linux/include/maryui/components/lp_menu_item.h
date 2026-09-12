/* MenuItem — one row of a menu: check column, label, shortcut; `separator`
 * draws the hairline instead. The highlight is the accent gradient. Mirrors
 * web/src/components/MenuItem, which the Spotlight panel now renders inline.
 *
 * `lp_menu_list` paints the rows and nothing else, so the caller owns the
 * container: inside Spotlight that is the panel's `.cmdDropdown` rule, and for
 * the Finder's context menu it is `lp_menu_popup` below — the one floating
 * dropdown left on this side, which the web has no counterpart for. */
#ifndef MARYUI_LP_MENU_ITEM_H
#define MARYUI_LP_MENU_ITEM_H

#include "maryui/lp_menus.h"
#include "maryui/lp_ui.h"

#define LP_MENU_MIN_WIDTH 200
/* The shadow extent around a floating menu chrome (shadow.menu). */
#define LP_MENU_SHADOW_EXTENT 40

typedef struct lp_menu_list_result {
    int selected;     /* entry index chosen this event, else -1 */
    int hovered;      /* entry under the pointer, else -1 */
} lp_menu_list_result;

/* size.control-height, or a separator's 1 + 2·space.1. */
float lp_menu_item_height(const lp_menu_entry *entry);
/* The rows' natural size: space.1 above and below plus the heights, and the
 * widest row (floor LP_MENU_MIN_WIDTH). `cr` may be NULL — lp_text_measure
 * keeps a scratch context, so an EVENT pass measures exactly what DRAW will. */
lp_size lp_menu_list_measure(cairo_t *cr, const lp_menu_model *model);
/* Paints and hit-tests the rows inside `r`, which is already the content box.
 * `active` is the keyboard/hover highlight. No background: see the header note. */
void lp_menu_list(lp_ctx *ctx, lp_rect r, const lp_menu_model *model, int active, lp_menu_list_result *out);
/* The floating panel around a list: shadow.menu, surface.menu at radius.md with
 * emboss-raised. Returns the panel's size. */
lp_size lp_menu_popup(lp_ctx *ctx, float x, float y, const lp_menu_model *model, int active, lp_menu_list_result *out);

#endif
