/* Spotlight — the floating search bar that is also the application dock. A
 * platinum panel, `size.spotlight-width` wide, holding a pill-shaped
 * TextField; below it the dock (a row of app tiles) while the query is
 * empty, or the ranked results as ListRows. Mirrors web/src/components/Spotlight.
 * The model (open, query, selection, items) is lp_spotlight.h. */
#ifndef MARYUI_LP_SPOTLIGHT_PANEL_H
#define MARYUI_LP_SPOTLIGHT_PANEL_H

#include "maryui/lp_spotlight.h"
#include "maryui/lp_ui.h"

#define LP_SPOTLIGHT_PLACEHOLDER "Say “Hey Mary” or type something…"
/* The bar's TextField id: the host focuses it (ctx->focus) when the panel opens. */
#define LP_SPOTLIGHT_QUERY_ID lp_id_hash("spotlight.query")
#define LP_SPOTLIGHT_PAD LP_SPACE_2
#define LP_SPOTLIGHT_CELL_W 88
#define LP_SPOTLIGHT_CELL_H 84
/* The panel's outer shadow reaches this far around it (shadow.menu). */
#define LP_SPOTLIGHT_SHADOW_EXTENT 40

typedef struct lp_spotlight_view {
    lp_text_buffer *query;            /* edited in place by the bar */
    const lp_spotlight_item *items;   /* the results for the query (the dock when it is blank) */
    int count;
    int selection;
    const char *placeholder;          /* NULL: LP_SPOTLIGHT_PLACEHOLDER */
    float width;                      /* 0: size.spotlight-width */
    int focus_bar;                    /* the real panel: keep the bar focused (a preview leaves focus alone) */
} lp_spotlight_view;

typedef struct lp_spotlight_result {
    int hovered;         /* item under the pointer, else -1 */
    int activated;       /* item clicked this event, else -1 */
    int query_changed;
    lp_rect panel;       /* where the panel was painted */
} lp_spotlight_result;

/* Whether a query shows the dock (blank after trimming) rather than results. */
int lp_spotlight_query_is_blank(const char *query);
/* The panel's size for this view, and the tallest it ever gets (for sizing a buffer once). */
lp_size lp_spotlight_measure(const lp_spotlight_view *view);
lp_size lp_spotlight_max_size(float width);
/* Paints the panel with its top-left at (x, y); handles input in the EVENT pass. */
void lp_spotlight_panel(lp_ctx *ctx, float x, float y, const lp_spotlight_view *view, lp_spotlight_result *out);

#endif
