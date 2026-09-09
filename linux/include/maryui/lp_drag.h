/* A drag session: what is being dragged (files from a Finder window), where
 * it came from, and the ghost that follows the pointer. The desktop owns one
 * session (lp_desktop.drag); the host (the compositor) moves the ghost, finds
 * the chrome under the pointer and delivers lp_input.drag HOVER / LEAVE / DROP
 * events to it. Apps decide what a drop means. */
#ifndef MARYUI_LP_DRAG_H
#define MARYUI_LP_DRAG_H

#include "maryui/lp_files.h"
#include "maryui/lp_icons.h"
#include "maryui/lp_ui.h"

struct lp_desktop;

typedef struct lp_drag {
    int active;
    char source_window[12];
    char src_dir[LP_FILES_PATH_MAX];
    char (*names)[LP_FILES_NAME_MAX];
    int count;
    lp_icon icon;             /* the first item's kind */
    int folder;               /* the first item is a folder (the tile's tint) */
    char label[256];          /* "notes.txt" or "3 items" */
    int copy;                 /* Alt held, or the target is on another device: the ghost shows "+" */
} lp_drag;

/* Starts a session (ends any other); calls d->on_drag(d, 1). 0 when nothing was given. */
int lp_desktop_drag_begin(struct lp_desktop *d, const char *window_id, const char *src_dir, const char *const *names, int n, lp_icon icon, int folder);
/* Ends the session, frees the names, calls d->on_drag(d, 0). */
void lp_desktop_drag_end(struct lp_desktop *d);

/* The ghost: a 52px file tile with the label beneath, a count badge for several items,
 * a "+" badge when copying, at 80% opacity. */
#define LP_DRAG_GHOST_W 120
#define LP_DRAG_GHOST_H 84
#define LP_DRAG_HOTSPOT_X 60
#define LP_DRAG_HOTSPOT_Y 30
void lp_drag_ghost(lp_ctx *ctx, lp_rect r, const lp_drag *drag);

#endif
