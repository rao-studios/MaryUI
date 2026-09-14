/* Launchpad — every application on a grid over the desktop (Linux, PARITY D32): the wallpaper
 * blurred behind a platinum scrim, a round search field, tiles of the apps' marks with their names,
 * paged when more than fit. Opened from Spotlight's All Applications pill; Esc, a click on the scrim
 * or launching closes it. The model is lp_desktop.launchpad (open, query, selection, page); the
 * compositor hosts it in src/compositor/launchpad.c and lp-render draws it with --launchpad. Linux-only
 * UI, so it lives beside the graph and the pane kit rather than among the mirrored components. */
#ifndef MARYUI_LP_LAUNCHPAD_H
#define MARYUI_LP_LAUNCHPAD_H

#include <cairo.h>

#include "maryui/components/lp_controls.h"
#include "maryui/lp_spotlight.h"
#include "maryui/lp_ui.h"

#define LP_LAUNCHPAD_QUERY_ID lp_id_hash("launchpad.query")
#define LP_LAUNCHPAD_CELL_W 128.0f
#define LP_LAUNCHPAD_CELL_H 120.0f
#define LP_LAUNCHPAD_MARK 64.0f
#define LP_LAUNCHPAD_MAX_COLUMNS 7
#define LP_LAUNCHPAD_SEARCH_W 320.0f
#define LP_LAUNCHPAD_SEARCH_Y 48.0f
#define LP_LAUNCHPAD_SEARCH_H 32.0f
#define LP_LAUNCHPAD_GRID_TOP (LP_LAUNCHPAD_SEARCH_Y + LP_LAUNCHPAD_SEARCH_H + 40.0f)
#define LP_LAUNCHPAD_DOTS_H 40.0f
#define LP_LAUNCHPAD_MARGIN 40.0f

/* The grid's columns for a screen this wide, so the model's arrow keys and the panel agree. */
static inline int lp_launchpad_columns(float width) {
    int c = (int)((width - 2 * LP_LAUNCHPAD_MARGIN) / LP_LAUNCHPAD_CELL_W);
    return c < 1 ? 1 : c > LP_LAUNCHPAD_MAX_COLUMNS ? LP_LAUNCHPAD_MAX_COLUMNS : c;
}
/* The rows a page holds on a screen this tall. */
static inline int lp_launchpad_rows(float height) {
    int r = (int)((height - LP_LAUNCHPAD_GRID_TOP - LP_LAUNCHPAD_DOTS_H) / LP_LAUNCHPAD_CELL_H);
    return r < 1 ? 1 : r;
}

typedef struct lp_launchpad_view {
    lp_text_buffer *query;            /* edited in place by the field */
    const lp_spotlight_item *items;   /* the apps to show (lp_desktop_launchpad_items) */
    int count;
    int selection;
    int page;
    float width, height;              /* the screen */
    cairo_surface_t *backdrop;        /* the wallpaper blurred to width×height (lp_launchpad_backdrop), or NULL for the scrim alone */
    int focus_field;                  /* keep the field focused (the real host does; a render leaves focus alone) */
} lp_launchpad_view;

typedef struct lp_launchpad_result {
    int hovered;         /* item under the pointer, else -1 */
    int activated;       /* item clicked this event, else -1 */
    int query_changed;
    int dismissed;       /* the scrim was clicked */
    int page_pressed;    /* a page dot was clicked, else -1 */
    int columns, rows, pages;
} lp_launchpad_result;

/* The wallpaper softened for the scrim: scaled down by four, blurred, scaled back up. The caller owns it. */
cairo_surface_t *lp_launchpad_backdrop(cairo_surface_t *wallpaper, int w, int h);
/* Paints the grid over (0, 0, width, height); handles input in the EVENT pass. */
void lp_launchpad_panel(lp_ctx *ctx, const lp_launchpad_view *v, lp_launchpad_result *out);

#endif
