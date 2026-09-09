/* The layout components: ScrollArea, Toolbar, Sidebar, ListRow. Each mirrors
 * web/src/components/<Name>. Rects are in the chrome's coordinates. */
#ifndef MARYUI_LP_LAYOUT_COMPONENTS_H
#define MARYUI_LP_LAYOUT_COMPONENTS_H

#include "maryui/lp_icons.h"
#include "maryui/lp_ui.h"

/* ScrollArea — a viewport over taller/wider content with thin platinum scrollbars.
 * begin() clips to the viewport and returns the content's origin rect (scrolled);
 * end() draws the scrollbar and restores the clip. Wheel input scrolls in the EVENT pass. */
typedef struct lp_scroll_state { float x, y; } lp_scroll_state;
lp_rect lp_scroll_begin(lp_ctx *ctx, lp_id id, lp_rect viewport, lp_size content, lp_scroll_state *state);
void lp_scroll_end(lp_ctx *ctx);

/* Toolbar — the brushed 40px strip under a title bar. Cuts itself off the top of *area; returns its inner rect. */
#define LP_TOOLBAR_H 40
lp_rect lp_toolbar(lp_ctx *ctx, lp_rect *area);

/* Sidebar — a 180px source list. Cuts itself off the left of *area; returns the padded content rect. */
#define LP_SIDEBAR_W 180
lp_rect lp_sidebar(lp_ctx *ctx, lp_rect *area);
/* A section heading; advances *cursor (a rect whose y moves down). */
void lp_sidebar_section(lp_ctx *ctx, lp_rect *cursor, const char *title);
/* A 24px selectable row; returns clicked; advances *cursor. */
int lp_sidebar_item(lp_ctx *ctx, lp_id id, lp_rect *cursor, lp_icon icon, const char *label, int selected);

/* ListRow — one 22px line: icon, name, trailing columns; rows alternate tint. */
#define LP_LIST_ROW_H 22
#define LP_LIST_HEADER_H 20
/* Column x positions/widths for a row `width` wide with n trailing columns (grid minmax(160px,2fr) repeat(minmax(90px,1fr))). */
void lp_list_columns(float width, int n, float *xs, float *ws);
/* The header row; sort_col (or -1) shows a ▲/▼ after its label. Returns the column clicked this event, else -1. */
int lp_list_header(lp_ctx *ctx, lp_id id, lp_rect r, const char *const *columns, int n, int sort_col, int descending);
/* Returns 1 on click, 2 on double-click. */
int lp_list_row(lp_ctx *ctx, lp_id id, lp_rect r, lp_icon icon, const char *name, const char *const *columns, int n, int selected, int even);

#endif
