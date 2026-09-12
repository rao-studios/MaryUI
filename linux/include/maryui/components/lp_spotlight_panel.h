/* Spotlight — the floating search bar that is also the application dock, and
 * (folded in the same way the dock was) the app's commands. A platinum panel,
 * `size.spotlight-width` wide, holding a pill-shaped TextField; below it the
 * dock (a row of app tiles) while the query is empty, or the ranked results as
 * ListRows. Under the dock ride the commands: a "Searching <app>" line, a row
 * of pills (File / Edit / View …), and the open pill's entries inline in this
 * same panel instead of a second floating one. There is no menu bar.
 * Mirrors web/src/components/Spotlight. The model (open, query, selection,
 * items) is lp_spotlight.h; the menus are lp_menus.h, built by lp_desktop. */
#ifndef MARYUI_LP_SPOTLIGHT_PANEL_H
#define MARYUI_LP_SPOTLIGHT_PANEL_H

#include "maryui/lp_menus.h"
#include "maryui/lp_spotlight.h"
#include "maryui/lp_ui.h"

#define LP_SPOTLIGHT_PLACEHOLDER "Say “Hey Mary” or type something…"
/* The bar's TextField id: the host focuses it (ctx->focus) when the panel opens. */
#define LP_SPOTLIGHT_QUERY_ID lp_id_hash("spotlight.query")
#define LP_SPOTLIGHT_PAD LP_SPACE_2
#define LP_SPOTLIGHT_CELL_W 88
#define LP_SPOTLIGHT_CELL_H 84
/* The commands section: the "Searching <app>" line, and the row of pills. */
#define LP_SPOTLIGHT_CMD_HEADER_H 24
#define LP_SPOTLIGHT_PILL_H 24
#define LP_SPOTLIGHT_PILL_GAP 4
#define LP_SPOTLIGHT_MAX_PILLS 8
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
    /* The app commands, as pills under the dock. NULL: no commands section. */
    const lp_menu_model *menus;
    int menu_count;
    int open_menu;                    /* the expanded pill, else -1 */
    int menu_active;                  /* the open menu's highlighted entry, else -1 */
    /* "Searching <name>": the focused window's app. NULL: no context line. */
    const char *context_name;
    lp_icon context_icon;
    /* The panel never grows past this (0: unbounded). An open menu absorbs the
     * whole reduction and clips; the bar, the dock and the pills never do. */
    float max_h;
} lp_spotlight_view;

typedef struct lp_spotlight_result {
    int hovered;         /* item under the pointer, else -1 */
    int activated;       /* item clicked this event, else -1 */
    int query_changed;
    int menu_pressed;    /* command pill pressed this event, else -1 */
    int menu_hovered;    /* command pill under the pointer, else -1 */
    int entry_hovered;   /* entry of the open menu under the pointer, else -1 */
    int entry_selected;  /* entry chosen this event, else -1 */
    lp_rect panel;       /* where the panel was painted */
} lp_spotlight_result;

/* Whether a query shows the dock (blank after trimming) rather than results. */
int lp_spotlight_query_is_blank(const char *query);
/* The panel's size for this view. */
lp_size lp_spotlight_measure(const lp_spotlight_view *view);
/*
 * The tallest the panel gets with no command menu open — what a host allocates
 * once so that typing never reallocates. `view.open_menu` and `view.max_h` are
 * ignored: opening a pill is a deliberate click, and the host resizes for it.
 */
lp_size lp_spotlight_max_size(const lp_spotlight_view *view);
/* Paints the panel with its top-left at (x, y); handles input in the EVENT pass. */
void lp_spotlight_panel(lp_ctx *ctx, float x, float y, const lp_spotlight_view *view, lp_spotlight_result *out);

#endif
