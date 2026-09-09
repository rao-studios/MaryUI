/* MenuBar — the brushed strip across the top: the Rao monogram trigger, the
 * app menus, and status on the right (the clock). Click opens a menu; while
 * one is open, hovering another switches to it. Mirrors web/src/components/MenuBar. */
#ifndef MARYUI_LP_MENU_BAR_H
#define MARYUI_LP_MENU_BAR_H

#include "maryui/lp_settings.h"
#include "maryui/lp_ui.h"

#define LP_MENU_BAR_MAX 8

typedef struct lp_menu_bar_model {
    const char *labels[LP_MENU_BAR_MAX]; /* labels[0] is the monogram trigger; its label is unused */
    int count;
    int open_index;                      /* -1 when no menu is open */
    const char *clock;                   /* "Mon 5:42 PM" */
    const char *status;                  /* optional text before the clock */
} lp_menu_bar_model;

typedef struct lp_menu_bar_result {
    int pressed;        /* trigger pressed this event, else -1 */
    int hovered;        /* trigger under the pointer, else -1 */
    lp_rect triggers[LP_MENU_BAR_MAX];
} lp_menu_bar_result;

/* The bar's outer shadow reaches this far below LP_SIZE_MENUBAR_HEIGHT. */
#define LP_MENU_BAR_SHADOW_EXTENT 10

void lp_menu_bar(lp_ctx *ctx, lp_rect r, const lp_menu_bar_model *model, lp_menu_bar_result *out);

#endif
