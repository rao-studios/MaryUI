/* A window on screen: the window manager's record made visible as a scene
 * subtree — a chrome buffer (shadow + frame + title bar + built-in body) and,
 * for Wayland clients, the client's surface tree at the body rect. The motion
 * engine's per-window target (lp_window_motion) lives here too: its output
 * becomes the subtree's position and the chrome buffer's scale, the way the
 * web writes a `transform` on the chrome element. */
#ifndef MUI_WINDOW_H
#define MUI_WINDOW_H

#include "maryui/lp_desktop.h"
#include "maryui/lp_motion.h"
#include "anim.h"
#include "chrome.h"

enum mui_window_kind { MUI_WINDOW_APP, MUI_WINDOW_XDG };

struct mui_window {
    struct wl_list link;
    struct mui_server *server;
    char wm_id[12];
    enum mui_window_kind kind;
    struct wlr_scene_tree *tree;      /* at rect - margin, offset by the motion transform */
    struct mui_chrome chrome;         /* the frame, including the shadow margin */
    int margin_l, margin_t, margin_r, margin_b;
    lp_rect rect;                     /* the rect the window is laid out at: the WM's, or the live rect during a resize */
    enum lp_window_state state;
    int focused;
    int z;
    lp_app_instance *instance;        /* built-in app, or NULL */
    lp_rect body;                     /* chrome-local body rect from the last paint */
    struct mui_xdg_client *client;    /* xdg.c, for MUI_WINDOW_XDG */

    /* Motion (web/src/lib/motionEngine.ts WindowMotion) and the keyframe animations of Window.tsx. */
    lp_window_motion motion;          /* motion.out is what the scene shows */
    lp_motion_target target;
    int was_moving;              /* to repaint the body once when the motion settles */
    struct mui_tween close_anim;      /* lp-window-close: scale .96 + fade over motion.fast, CLOSE after CLOSE_MS */
    int closing;
    struct mui_tween shade_anim;      /* the height transition: the chrome is cropped from shade_from_h to shade_to_h */
    float shade_from_h, shade_to_h;
    float visual_h;                   /* the cropped window height while the shade animation runs, else 0 */
    int was_scaled;                   /* the last transform scaled or cropped the chrome */
    struct { int dw, dh; float opacity; int filter_nearest, opaque_on, valid; } applied;  /* last values handed to the scene, to skip no-op setters */
};

/* xdg.c's per-client state, owned by the client's lifetime, not the window's. */
struct mui_xdg_client {
    struct mui_server *server;
    struct mui_window *win;                 /* NULL until mapped / after the window closed */
    struct wlr_xdg_toplevel *toplevel;
    struct wlr_xdg_toplevel_decoration_v1 *decoration;
    struct wlr_scene_tree *scene_tree;      /* wlr_scene_xdg_surface_create's tree, under win->tree */
    struct wlr_scene_tree *surface_tree;    /* its subsurface tree (clipped during flights and shades) */
    struct wlr_box geometry;
    int mapped;
    struct wl_listener map, unmap, commit, destroy, request_move, request_resize, request_maximize, request_minimize,
        set_title, new_popup, decoration_request_mode, decoration_destroy;
};

/* Closes through the client when there is one, else animates the chrome out and closes through the WM. */
void mui_window_request_close(struct mui_window *win);

struct mui_window *mui_window_find(struct mui_server *server, const char *wm_id);
/* Brings the scene in step with the WM state after a dispatch (starts flights and shade animations). */
void mui_windows_sync(struct mui_server *server);
/* Sets the laid-out rect (during resizes, without a WM commit) and re-applies the transform. */
void mui_window_place(struct mui_window *win, lp_rect rect);
/* Scene ← motion.out: the --lp-* variables into the title bar, the transform onto the subtree. */
void mui_window_apply_motion(struct mui_window *win);
/* Starts a move grab with the pointer at layout (lx, ly): the title bar was pressed, or the client asked. */
void mui_window_begin_move(struct mui_window *win, double lx, double ly);
/* Starts a resize grab from a handle. */
void mui_window_begin_resize(struct mui_window *win, int handle, double lx, double ly);
/* Advances the close and shade animations; non-zero while any is running. */
int mui_windows_animate(struct mui_server *server, double now_ms);
void mui_window_destroy(struct mui_window *win);

#endif
