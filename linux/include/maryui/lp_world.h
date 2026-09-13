/* The world the desktop publishes to Mary (PARITY D28): what is on screen, as each application
 * says it itself — no accessibility tree, no pixels. Every app with a `surface` hook contributes
 * one place per app (its front window): the window's title, the document it shows and a window
 * of its text, the elements it offers and the one that has focus, the selection. The compositor
 * sends the whole world to maryd whenever the focus or the windows change (coalesced over 50 ms),
 * every LP_WORLD_POLL_S (or the app's own surface_poll_s) while windows are open, and whenever
 * maryd asks (`world.request`, at the start of a turn). A text selection in the front window is
 * a handoff of its own (`selection{…}`; `selection.clear{…}` when it goes), and maryd can ask one
 * app for its surface directly (`app.state{call_id, app}` → `app.state.result`).
 *
 *   desktop → maryd   world{capturedAt, focus, places[{place, capturedAt, surface{application{name, id},
 *                     activeWindow{title}, windowCount, minimizedCount, elements[{identity, ordinal, role, kind,
 *                     label, focused, enabled}], focused, pageNotYetRead, document{name, path, text, total, lower,
 *                     upper}}}], windows[{id, app, title, minimized}]}
 *                     selection{applicationID, place, text, document, lower, upper, total, editable, capturedAt}
 *                     selection.clear{applicationID}
 *                     app.state.result{call_id, ok, surface | error}
 *
 * Linux only; without json-c nothing is published. */
#ifndef MARYUI_LP_WORLD_H
#define MARYUI_LP_WORLD_H

#include <stddef.h>
#include <stdint.h>

#include "maryui/lp_files.h"

struct lp_desktop;
struct lp_source;

#define LP_WORLD_POLL_S 15
#define LP_WORLD_COALESCE_MS 50
#define LP_SURFACE_ELEMENTS 120
#define LP_SURFACE_LABEL_MAX 121
#define LP_SURFACE_TEXT_WINDOW 1200     /* code points of a document around its selection or caret */

typedef struct lp_app_surface_element {
    char role[24];                  /* "button", "textarea", "list", "row" … */
    char kind[24];                  /* the humanized word: "button" */
    char label[LP_SURFACE_LABEL_MAX];
    int focused, enabled;
} lp_app_surface_element;

/* What an app says about its front window. The strings the app allocates (text, selection) are freed by
 * the publisher with free(3). */
typedef struct lp_app_surface {
    char window_title[128];         /* "" : the window manager's title */
    char document_name[LP_FILES_NAME_MAX];
    char document_path[LP_FILES_PATH_MAX];
    char *document_text;            /* a window of the document's text, or NULL */
    int document_total;             /* code points in the whole document (0: unknown) */
    int document_lower, document_upper;   /* the window's bounds in code points */
    char *selection_text;           /* the selected text, or NULL */
    int selection_lower, selection_upper; /* code points into the document */
    int selection_editable;
    lp_app_surface_element elements[LP_SURFACE_ELEMENTS];
    int element_count;
    int focused;                    /* index into elements, or -1 */
    int page_not_yet_read;
} lp_app_surface;

/* Adds an element; the label is clipped. Returns its index, or -1 when full. */
int lp_app_surface_add(lp_app_surface *s, const char *role, const char *kind, const char *label, int focused, int enabled);

typedef struct lp_world {
    struct lp_source *coalesce;     /* the 50 ms timer after a change */
    struct lp_source *poll;
    int poll_s;
    char selection_app[32];         /* the selection last published, so a repeat is not sent twice */
    char *selection_text;
    int selection_lower, selection_upper;
    int published;                  /* worlds sent (for tests) */
} lp_world;

void lp_world_init(lp_world *w);
void lp_world_free(struct lp_desktop *d);
/* The world as JSON text (heap; the caller frees), or NULL without json-c. `now_ms` stamps capturedAt. */
char *lp_desktop_world_json(struct lp_desktop *d, int64_t now_ms);
/* Sends the world (and the selection handoff, when it changed) to maryd now. 0, -ENOTCONN, -ENOSYS. */
int lp_desktop_publish_world(struct lp_desktop *d);
/* The focus or the windows changed: publish after LP_WORLD_COALESCE_MS (at once without an event loop). */
void lp_desktop_world_changed(struct lp_desktop *d);
/* maryd asked (world.request). */
void lp_desktop_on_world_request(struct lp_desktop *d);
/* maryd asked one app for its surface (app.state{call_id, app}); the answer goes back on the socket. */
void lp_desktop_on_app_state(struct lp_desktop *d, const char *call_id, const char *app_id);
/* One app's surface as JSON text (its front window; heap), or NULL when it has none. */
char *lp_desktop_app_surface_json(struct lp_desktop *d, const char *app_id, int64_t now_ms);

#endif
