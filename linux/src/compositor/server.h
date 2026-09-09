/* maryui-desktop: the Liquid Platinum desktop as a wlroots 0.17 compositor.
 *
 * The chrome (wallpaper, menu bar, window frames, menus, built-in apps) is
 * rendered by libmaryui with Cairo into wlr_buffers shown as scene nodes;
 * Wayland clients get server-side decorations. This header is the one shared
 * by every file in src/compositor/. */
#ifndef MUI_SERVER_H
#define MUI_SERVER_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <cairo.h>

#include "maryui/maryui.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_settings.h"
#include "anim.h"

/* A wlr_buffer whose pixels are a Cairo ARGB32 image surface. The pixman
 * renderer reads the pixels in place, so never draw into a buffer the scene
 * still holds (wlr_buffer.n_locks > 1). */
struct lp_cairo_buffer {
    struct wlr_buffer base;
    cairo_surface_t *surface;
};
struct lp_cairo_buffer *lp_cairo_buffer_create(int width, int height);
/* Wraps an existing ARGB32 image surface (takes its own reference). */
struct lp_cairo_buffer *lp_cairo_buffer_from_surface(cairo_surface_t *surface);

enum mui_cursor_shape {
    MUI_CURSOR_ARROW,
    MUI_CURSOR_NS,
    MUI_CURSOR_EW,
    MUI_CURSOR_NWSE,
    MUI_CURSOR_NESW,
    MUI_CURSOR_TEXT,
    MUI_CURSOR_COUNT,
};

struct mui_cursor_image {
    struct lp_cairo_buffer *buffer;
    int hotspot_x, hotspot_y;
};

struct mui_chrome;
struct mui_window;

enum mui_grab_kind { MUI_GRAB_NONE, MUI_GRAB_MOVE, MUI_GRAB_RESIZE, MUI_GRAB_DRAG };

/* MARYUI_DEBUG=frames: the frame handler's own time, bucketed <2, <4, <8, <16, 16+ ms. */
struct mui_frame_stats {
    unsigned buckets[5];
    unsigned frames;
    double total_ms, max_ms, report_ms;
    unsigned needs_frame, damaged;  /* frames where the backend asked for a commit, or the scene had changed */
    unsigned skipped;               /* backend-requested frames with nothing to show that were not committed */
    double commit_ms, commit_max_ms;  /* time inside wlr_scene_output_commit */
    double paint_ms, paint_max_ms; unsigned paints;  /* chrome repaints run from the frame handler (title strips during motion) */
    double tick_ms, tick_max_ms, transform_ms, transform_max_ms;  /* engine tick (steps + apply), and the scene setters alone */
    double pre_ms, mid_ms, post_ms, cpu_ms;  /* before the tick, between tick and commit, after the commit; thread CPU time of the whole handler */
    double anim_ms;  /* mui_windows_animate + mui_desktop_animate */
    double last_diag_ms;
    int motion_active;
};

struct mui_server {
    const lp_settings *settings;
    lp_branding branding;
    lp_desktop desktop;                  /* wm + apps + menus + keys */
    lp_motion_engine engine;             /* one loop per desktop; its wake schedules a frame (output.c) */
    int debug_frames;                    /* MARYUI_DEBUG=frames: wake/idle log + frame-time histogram */
    struct mui_frame_stats frame_stats;
    struct wl_list windows;              /* struct mui_window */
    struct {
        enum mui_grab_kind kind;
        struct mui_window *win;
        int handle;                      /* enum lp_resize_handle */
        double sx, sy;                   /* pointer at grab start (layout) */
        lp_rect start;
    } grab;
    struct mui_chrome *menu;             /* the open dropdown, or NULL */
    int menu_index;
    int menu_shown_index;                /* which menu the dropdown shows (-1: none); a change replays lp-menu-in */
    struct mui_tween menu_anim;          /* Menu.module.css lp-menu-in on the dropdown */
    int menu_x, menu_y;                  /* the dropdown's resting position */
    struct mui_chrome *spotlight;        /* Spotlight's panel while it is open (spotlight.c) */
    struct mui_tween spotlight_anim;     /* lp-spotlight-in: opacity 0→1, scale .96→1 about the centre */
    int spotlight_x, spotlight_y;        /* the panel chrome's resting position */
    lp_rect spotlight_panel;             /* the painted panel inside the chrome (chrome-local), for hit-testing */
    struct wl_event_source *spotlight_idle; /* a deferred sync after a WM change */
    struct wl_event_source *repaint_idle;   /* a deferred repaint of windows an app dirtied from inside an EVENT pass */
    struct mui_chrome *drag_ghost;          /* the drag session's ghost (drag.c), while one is active */
    struct mui_chrome *drag_source, *drag_target; /* the chrome the drag began in; the one under the pointer */
    int drag_ghost_copy;                    /* what the ghost last painted (the "+" badge) */
    struct mui_files *files;                /* directory watches (files.c) */
    struct {                             /* key repeat for keys the chromes consume (clients repeat on their own) */
        struct wl_event_source *timer;
        uint32_t keycode, keysym, mods;
        char utf8[8];
        int rate;
        int active;
    } key_repeat;
    int key_to_chrome;                   /* mui_desktop_key: the last press went to a chrome (repeatable) */
    uint32_t last_click_ms;
    double last_click_x, last_click_y;

    struct wl_display *display;
    struct wlr_backend *backend;
    struct wlr_session *session;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_compositor *compositor;
    struct wlr_subcompositor *subcompositor;
    struct wlr_data_device_manager *data_device;
    struct wlr_xdg_shell *xdg_shell;
    struct wlr_xdg_decoration_manager_v1 *decoration_manager;
    struct wl_listener new_xdg_surface, new_toplevel_decoration;

    struct wlr_output_layout *output_layout;
    struct wl_list outputs; /* struct mui_output */
    struct wl_listener new_output;

    struct wlr_scene *scene;
    struct wlr_scene_output_layout *scene_layout;
    /* z.wallpaper 0 · z.windows 100 · z.menubar 600 · z.menus 700 · z.spotlight 800 (tokens.json) */
    struct wlr_scene_tree *layer_wallpaper, *layer_windows, *layer_menubar, *layer_menus, *layer_spotlight;
    int desktop_width, desktop_height;   /* the first output */

    struct wlr_seat *seat;
    struct wlr_cursor *cursor;
    struct mui_cursor_image cursors[MUI_CURSOR_COUNT];
    enum mui_cursor_shape cursor_shape;
    struct mui_chrome *pointer_chrome;   /* the chrome under the pointer, for leave events */

    struct mui_chrome *menubar;          /* the top strip, one per desktop (first output) */
    struct wl_event_source *clock_timer;
    char clock_text[32];
    struct wl_list keyboards; /* struct mui_keyboard */
    struct wl_listener new_input;
    struct wl_listener cursor_motion, cursor_motion_absolute, cursor_button, cursor_axis, cursor_frame;
    struct wl_listener request_set_cursor, request_set_selection;
};

struct mui_output {
    struct wl_list link;
    struct mui_server *server;
    struct wlr_output *wlr_output;
    struct wlr_scene_output *scene_output;
    struct wlr_scene_rect *background;
    struct wlr_scene_buffer *wallpaper;
    struct lp_cairo_buffer *wallpaper_buffer;
    int width, height;
    struct wl_event_source *frame_timer;  /* paces frames to the refresh rate (output.c request_frame) */
    double last_frame_ms;
    int frame_wanted;                     /* a frame was requested (motion, tweens); early flips re-arm the timer only then */
    double frame_due_ms;                  /* when the requested frame is due (motion: next refresh; ambient: 30 Hz) */
    double last_cursor_x, last_cursor_y;  /* pointer at the last commit, to notice hardware-cursor moves */
    struct wl_listener frame, request_state, destroy;
};

struct mui_keyboard {
    struct wl_list link;
    struct mui_server *server;
    struct wlr_keyboard *wlr_keyboard;
    struct wl_listener modifiers, key, destroy;
};

bool mui_server_init(struct mui_server *server);
void mui_server_finish(struct mui_server *server);

void mui_output_handle_new(struct wl_listener *listener, void *data);

void mui_input_init(struct mui_server *server);
void mui_input_finish(struct mui_server *server);
void mui_input_handle_new(struct wl_listener *listener, void *data);
/* Stops repeating the held key (focus moved, Spotlight closed). */
void mui_input_disarm_repeat(struct mui_server *server);

void mui_cursor_init(struct mui_server *server);
void mui_cursor_finish(struct mui_server *server);
void mui_cursor_set_shape(struct mui_server *server, enum mui_cursor_shape shape);

/* The desktop surface: wallpaper for an output, the menu bar and its clock. */
void mui_desktop_output_ready(struct mui_output *output);
void mui_desktop_init(struct mui_server *server);
void mui_desktop_finish(struct mui_server *server);
/* Pointer events routed to chromes and grabs (layout coordinates). Returns 1 when consumed. */
int mui_desktop_pointer_event(struct mui_server *server, double lx, double ly, int buttons, int pressed, int released, uint32_t time_msec);
/* Keyboard shortcuts, menu navigation, then the focused built-in window. Returns 1 when consumed. */
int mui_desktop_key(struct mui_server *server, uint32_t keysym, uint32_t modifiers, const char *utf8, int pressed);
/* A wheel event at the pointer; returns 1 when a chrome consumed it. */
int mui_desktop_scroll(struct mui_server *server, double lx, double ly, float dx, float dy);
/* Repaints chromes whose last paint asked for another frame. */
void mui_desktop_ambient_tick(struct mui_server *server);
/* Repaints everything that depends on the settings (accent, wallpaper). */
void mui_desktop_settings_changed(struct mui_server *server);
/* What is under a layout point: a chrome (sx/sy chrome-local), a client surface (surface-local), or nothing.
 * Shadow margins are not hit; the search falls through to what lies beneath them. */
struct mui_hit {
    struct mui_chrome *chrome;
    struct mui_window *win;
    struct wlr_surface *surface;
    double sx, sy;
};
void mui_desktop_hit(struct mui_server *server, double lx, double ly, struct mui_hit *hit);
/* Advances the desktop's own animations (the dropdown's lp-menu-in, Spotlight's appear); non-zero while one runs. */
int mui_desktop_animate(struct mui_server *server, double now_ms);

/* Spotlight (spotlight.c): the panel chrome follows desktop.spotlight. */
void mui_spotlight_sync(struct mui_server *server);
/* Syncs on the next loop turn (safe from inside a chrome's EVENT pass). */
void mui_spotlight_request_sync(struct mui_server *server);
int mui_spotlight_hit(struct mui_server *server, double lx, double ly, struct mui_hit *hit);
int mui_spotlight_animate(struct mui_server *server, double now_ms);
void mui_spotlight_finish(struct mui_server *server);
/* Drag and drop (drag.c): the ghost chrome and the grab that routes HOVER / LEAVE / DROP to app windows. */
void mui_drag_begin(struct mui_server *server);
void mui_drag_finish(struct mui_server *server);
void mui_drag_motion(struct mui_server *server, double lx, double ly, uint32_t time_msec);
void mui_drag_drop(struct mui_server *server, double lx, double ly, uint32_t time_msec);
void mui_drag_cancel(struct mui_server *server);
void mui_drag_mods_changed(struct mui_server *server);
void mui_drag_window_gone(struct mui_server *server, struct mui_chrome *chrome);
/* Directory watching (files.c): lp_desktop.watch over inotify. */
void mui_files_init(struct mui_server *server);
void mui_files_finish(struct mui_server *server);
/* Schedules a frame on every output; the motion engine's wake callback (output.c). */
void mui_server_schedule_frame(struct mui_server *server);
/* Damages every output whole (after scaled/cropped nodes return to normal). */
void mui_server_damage_all(struct mui_server *server);
void mui_engine_wake(void *user);

/* xdg-shell clients as windows (xdg.c). */
void mui_xdg_init(struct mui_server *server);
struct mui_window;
/* Called from mui_windows_sync for client windows: size, activation, shading. */
void mui_xdg_window_synced(struct mui_window *win, int focused, int rect_changed);
/* Asks the client to close; returns 1 when the window is a client's. */
int mui_xdg_request_close(struct mui_window *win);
/* The window a client surface (or one of its popups) belongs to, if any. */
struct mui_window *mui_xdg_window_of_surface(struct mui_server *server, struct wlr_surface *surface);
/* Positions, clips and shows/hides the client tree for the chrome's scale and crop (window.c's transform). */
void mui_xdg_client_transform(struct mui_window *win, float sx, float sy, float visual_h);
/* Focus the keyboard on a client window's surface (or clear it). */
void mui_xdg_keyboard_focus(struct mui_server *server, struct mui_window *win);

#endif
