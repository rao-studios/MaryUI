/* Reopening windows at login (Linux only, PARITY D34). The desktop writes the windows it has open to
 * $XDG_CONFIG_HOME/maryui/session.conf whenever they change — each app window in stacking order with its place,
 * its size, whether it is zoomed or shaded, and the folder or document its surface shows — and, when System
 * Settings › General › Reopen at login is on, opens them again the next time it starts. Off, the desktop starts
 * empty. Client (Wayland) windows and internal apps are not kept: nothing here could start them again. */
#ifndef MARYUI_LP_SESSION_H
#define MARYUI_LP_SESSION_H

#include <stddef.h>

struct lp_desktop;

#define LP_SESSION_FILE "session.conf"

/* Writes the open app windows, lowest first (atomically). How many were written, or -errno. */
int lp_session_save(struct lp_desktop *d);
/* Opens the windows session.conf lists, lowest first, so the last is in front: a document that is gone opens its
 * app empty, an app that is gone is skipped, as is a line that does not parse. How many opened; 0 with no file. */
int lp_session_restore(struct lp_desktop *d);
/* The folder or document a window shows, from its app's surface; 0 (and "") when it shows none. */
int lp_session_document_path(struct lp_desktop *d, const char *window_id, char *out, size_t n);

#endif
