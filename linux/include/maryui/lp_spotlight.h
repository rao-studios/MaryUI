/* Spotlight's model (web/src/desktop/spotlight.ts): open/closed, the query,
 * the selection, and the pure functions that turn the desktop's apps and
 * windows into items and filter them. The panel that paints it is
 * components/lp_spotlight_panel.h; the compositor hosts it (spotlight.c). */
#ifndef MARYUI_LP_SPOTLIGHT_H
#define MARYUI_LP_SPOTLIGHT_H

#include "maryui/components/lp_controls.h"
#include "maryui/lp_icons.h"

struct lp_desktop;

enum lp_spotlight_kind { LP_SPOT_APP, LP_SPOT_WINDOW, LP_SPOT_COMMAND };

typedef struct lp_spotlight_item {
    enum lp_spotlight_kind kind;
    char id[32];          /* app id, window id, or command id */
    char title[128];
    char also[64];        /* another name the query matches: an app's aka ("System Settings") */
    char subtitle[64];    /* "Application", "Window · Finder", "Command" */
    lp_icon icon;
    const char *object;       /* the object-tier mark, when the glyph's name is not it */
    int index;            /* app index (APP), window index (WINDOW) */
    int running;          /* an app with a window open (the dock's dot) */
    int dock;             /* pinned: shown for a blank query (lp_app.dock, or the Terminal command) */
} lp_spotlight_item;

typedef struct lp_spotlight {
    int open;
    lp_text_buffer query;
    int selection;        /* index into the results */
} lp_spotlight;

#define LP_SPOTLIGHT_MAX_ITEMS 80
#define LP_SPOTLIGHT_MAX_RESULTS 8
/* The bar's centre sits at this fraction of the desktop height. */
#define LP_SPOTLIGHT_Y_FRACTION 0.38f

void lp_spotlight_init(lp_spotlight *s);
void lp_spotlight_open(lp_spotlight *s);     /* empties the query, selection 0 */
void lp_spotlight_close(lp_spotlight *s);
void lp_spotlight_toggle(lp_spotlight *s);
void lp_spotlight_set_query(lp_spotlight *s, const char *query);   /* selection 0 */
/* Moves the selection by delta among count results, wrapping. */
void lp_spotlight_move(lp_spotlight *s, int delta, int count);

/* Every launchable thing: the registered apps (hidden ones too), the Terminal
 * command while no terminal app is registered, then the open windows. Returns the count. */
int lp_spotlight_items(const struct lp_desktop *d, lp_spotlight_item *out, int max);
/* The dock (an empty query: the pinned items) or the ranked matches over every item:
 * title prefix, then a word prefix, then a substring; stable; at most max. */
int lp_spotlight_results(const lp_spotlight_item *items, int n, const char *query, lp_spotlight_item *out, int max);

#endif
