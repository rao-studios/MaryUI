/* TitleBar — the brushed grab handle of a window: traffic lights on the
 * left, an embossed centred title, the sliding sheen behind both. */
#ifndef MARYUI_LP_TITLE_BAR_H
#define MARYUI_LP_TITLE_BAR_H

#include "maryui/lp_ui.h"

typedef struct lp_title_bar_model {
    const char *title;
    int active, shaded, zoomed;
    lp_corners radii;      /* the window's live corner radii; the bottom pair is 0 unless shaded */
} lp_title_bar_model;

typedef struct lp_title_bar_result {
    int close, shade, zoom;
    int double_click;      /* on the bar, outside the lights */
    int drag_start;        /* press on the bar, outside the lights */
    int hovered;
} lp_title_bar_result;

void lp_title_bar(lp_ctx *ctx, lp_rect r, const lp_title_bar_model *model, lp_title_bar_result *out);

#endif
