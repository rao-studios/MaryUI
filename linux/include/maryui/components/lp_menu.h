/* Menu + MenuItem — a dropdown panel of rows: check column, label, shortcut.
 * The highlight is the accent gradient. Near-opaque platinum; blur, not
 * refraction (≈ the web's backdrop blur is not reproduced by wlr_scene). */
#ifndef MARYUI_LP_MENU_H
#define MARYUI_LP_MENU_H

#include "maryui/lp_ui.h"

#define LP_MENU_MAX_ENTRIES 24
#define LP_MENU_MIN_WIDTH 200

typedef struct lp_menu_entry {
    int separator;
    char label[64];
    const char *shortcut;
    int checked, disabled;
    int command;      /* the desktop's command id */
    int arg;
} lp_menu_entry;

typedef struct lp_menu_model {
    const char *id;
    const char *label;
    lp_menu_entry entries[LP_MENU_MAX_ENTRIES];
    int count;
} lp_menu_model;

typedef struct lp_menu_result {
    int selected;     /* entry index chosen this event, else -1 */
    int hovered;      /* entry under the pointer, else -1 */
    int height;
    int width;
} lp_menu_result;

/* The panel's size for a model (needs a cairo context to measure text). */
lp_size lp_menu_measure(cairo_t *cr, const lp_menu_model *model);
/* Paints the panel at r's top-left; `active` is the keyboard/hover highlight. */
void lp_menu(lp_ctx *ctx, lp_rect r, const lp_menu_model *model, int active, lp_menu_result *out);
/* The shadow extent around a menu chrome. */
#define LP_MENU_SHADOW_EXTENT 40

#endif
