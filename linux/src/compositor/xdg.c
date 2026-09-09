/* xdg-shell clients as windows. Every toplevel gets a Liquid Platinum frame
 * with server-side decorations (always: a client asking for CSD is answered
 * SERVER_SIDE), its surface tree sits at the frame's body, the WM's rect
 * drives the client's size and the client's committed geometry drives the
 * frame (terminals snap to cells). Moves, resizes, zoom, shade, title and
 * close flow both ways. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/edges.h>

#include "maryui/lp_geometry.h"
#include "window.h"

static lp_rect body_rect_of(struct mui_window *win) {
    return LP_RECT(win->rect.x, win->rect.y + LP_TITLE_HEIGHT, win->rect.w, win->rect.h - LP_TITLE_HEIGHT);
}

/* The client tree follows the chrome: position, clip and visibility come from the window's transform. */
static void place_client(struct mui_xdg_client *c) {
    if (c->win) mui_window_apply_motion(c->win);
}

static void configure_size(struct mui_xdg_client *c) {
    if (!c->win) return;
    lp_rect body = body_rect_of(c->win);
    int w = (int)body.w, h = (int)body.h;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (c->geometry.width != w || c->geometry.height != h) wlr_xdg_toplevel_set_size(c->toplevel, w, h);
}

void mui_xdg_window_synced(struct mui_window *win, int focused, int rect_changed) {
    struct mui_xdg_client *c = win->client;
    if (!c) return;
    wlr_xdg_toplevel_set_activated(c->toplevel, focused);
    if (rect_changed) configure_size(c);
    place_client(c);
}

int mui_xdg_request_close(struct mui_window *win) {
    if (!win || win->kind != MUI_WINDOW_XDG || !win->client) return 0;
    wlr_xdg_toplevel_send_close(win->client->toplevel);
    return 1;
}

void mui_xdg_keyboard_focus(struct mui_server *server, struct mui_window *win) {
    struct wlr_seat *seat = server->seat;
    if (!win || win->kind != MUI_WINDOW_XDG || !win->client) {
        if (seat->keyboard_state.focused_surface) wlr_seat_keyboard_notify_clear_focus(seat);
        return;
    }
    struct wlr_surface *surface = win->client->toplevel->base->surface;
    if (seat->keyboard_state.focused_surface == surface) return;
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
    if (keyboard) wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
    else wlr_seat_keyboard_notify_enter(seat, surface, NULL, 0, NULL);
}

struct mui_window *mui_xdg_window_of_surface(struct mui_server *server, struct wlr_surface *surface) {
    struct wlr_surface *root = wlr_surface_get_root_surface(surface);
    struct wlr_xdg_surface *xdg = wlr_xdg_surface_try_from_wlr_surface(root);
    if (!xdg) return NULL;
    if (xdg->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) return xdg->data ? ((struct mui_xdg_client *)xdg->data)->win : NULL;
    if (xdg->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        /* a popup: its parent chain leads to the toplevel */
        struct wlr_xdg_popup *popup = xdg->popup;
        while (popup && popup->parent) {
            struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(popup->parent);
            if (!parent) break;
            if (parent->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) return parent->data ? ((struct mui_xdg_client *)parent->data)->win : NULL;
            popup = parent->popup;
        }
    }
    return NULL;
}

/* wlr_scene_xdg_surface_create's tree has its origin at the window geometry's
 * corner, so it sits at the body's origin — scaled with the chrome during a
 * flight, cropped during a shade. Client pixels are never scaled (PARITY.md):
 * during a flight the frame scales and the content is clipped to fit. */
void mui_xdg_client_transform(struct mui_window *win, float sx, float sy, float visual_h) {
    struct mui_xdg_client *c = win->client;
    if (!c || !c->scene_tree) return;
    float bx = win->margin_l * sx, by = (win->margin_t + LP_TITLE_HEIGHT) * sy;
    wlr_scene_node_set_position(&c->scene_tree->node, (int)lroundf(bx), (int)lroundf(by));
    float body_h = (visual_h > 0 ? visual_h : win->rect.h) - LP_TITLE_HEIGHT;
    int cw = (int)lroundf(win->rect.w * sx), ch = (int)lroundf(body_h * sy);
    int shaded = win->state == LP_WIN_SHADED && visual_h <= 0;
    int visible = !shaded && ch > 0;
    wlr_scene_node_set_enabled(&c->scene_tree->node, visible);
    if (!c->surface_tree) return;
    int clip = visible && (visual_h > 0 || fabsf(sx - 1) > 1e-4f || fabsf(sy - 1) > 1e-4f);
    if (clip) {
        struct wlr_box box = { c->geometry.x, c->geometry.y, cw > 0 ? cw : 1, ch > 0 ? ch : 1 };
        wlr_scene_subsurface_tree_set_clip(&c->surface_tree->node, &box);
    } else {
        wlr_scene_subsurface_tree_set_clip(&c->surface_tree->node, NULL);
    }
}

/* MARK: - Listeners */

static void client_commit(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, commit);
    struct wlr_xdg_surface *xdg = c->toplevel->base;
    if (xdg->initial_commit) {
        /* Let the client pick its size; we frame whatever it draws. (wm_capabilities needs xdg-shell v5; we offer v3.) */
        wlr_xdg_toplevel_set_size(c->toplevel, 0, 0);
        if (c->decoration) wlr_xdg_toplevel_decoration_v1_set_mode(c->decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        return;
    }
    struct wlr_box geo;
    wlr_xdg_surface_get_geometry(xdg, &geo);
    int changed = geo.width != c->geometry.width || geo.height != c->geometry.height || geo.x != c->geometry.x || geo.y != c->geometry.y;
    c->geometry = geo;
    if (!c->mapped || !c->win || !changed) return;
    struct mui_server *server = c->server;
    place_client(c);
    /* The frame follows the content the client actually committed (cell-snapped terminals). */
    if (server->grab.kind == MUI_GRAB_NONE || server->grab.win != c->win) {
        lp_rect body = body_rect_of(c->win);
        if ((int)body.w != geo.width || (int)body.h != geo.height) {
            lp_wm_action a = { .type = LP_WM_RESIZE, .id = c->win->wm_id,
                .rect = LP_RECT(c->win->rect.x, c->win->rect.y, geo.width, geo.height + LP_TITLE_HEIGHT) };
            lp_desktop_dispatch(&server->desktop, &a);
        }
    }
}

static void client_map(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, map);
    struct mui_server *server = c->server;
    c->mapped = 1;
    wlr_xdg_surface_get_geometry(c->toplevel->base, &c->geometry);
    const char *app_id = c->toplevel->app_id && *c->toplevel->app_id ? c->toplevel->app_id : "wayland";
    const char *title = c->toplevel->title && *c->toplevel->title ? c->toplevel->title : app_id;
    lp_wm_action a = { .type = LP_WM_OPEN, .spec = lp_open_spec_default(app_id, title) };
    a.spec.rect.w = c->geometry.width > 0 ? c->geometry.width : 640;
    a.spec.rect.h = (c->geometry.height > 0 ? c->geometry.height : 400) + LP_TITLE_HEIGHT;
    int min_w = c->toplevel->current.min_width, min_h = c->toplevel->current.min_height;
    a.spec.min_size = (lp_size){ min_w > 120 ? min_w : 120, (min_h > 60 ? min_h : 60) + LP_TITLE_HEIGHT };
    lp_desktop *d = &server->desktop;
    int before = d->wm.count;
    lp_desktop_dispatch(d, &a);
    if (d->wm.count != before + 1) { wlr_log(WLR_ERROR, "xdg: could not open a window for %s", app_id); return; }
    const lp_window_record *rec = &d->wm.windows[d->wm.count - 1];
    struct mui_window *win = mui_window_find(server, rec->id);
    if (!win) return;
    win->kind = MUI_WINDOW_XDG;
    win->client = c;
    c->win = win;
    c->scene_tree = wlr_scene_xdg_surface_create(win->tree, c->toplevel->base);
    /* Its subsurface tree, for clipping during flights and shades (popups are siblings, left unclipped). */
    c->surface_tree = NULL;
    struct wlr_scene_node *child;
    wl_list_for_each(child, &c->scene_tree->children, link) {
        if (child->type == WLR_SCENE_NODE_TREE) { c->surface_tree = wlr_scene_tree_from_node(child); break; }
    }
    c->toplevel->base->data = c;
    place_client(c);
    /* The WM may have constrained the rect: tell the client. */
    configure_size(c);
    wlr_xdg_toplevel_set_activated(c->toplevel, 1);
    mui_xdg_keyboard_focus(server, win);
    mui_chrome_damage_all(&win->chrome);
    mui_chrome_repaint(&win->chrome, mui_now_ms());
    wlr_log(WLR_INFO, "xdg: %s mapped as %s (%dx%d)", app_id, rec->id, c->geometry.width, c->geometry.height);
}

static void client_unmap(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, unmap);
    c->mapped = 0;
    if (c->scene_tree) { wlr_scene_node_destroy(&c->scene_tree->node); c->scene_tree = NULL; }
    c->surface_tree = NULL;
    if (c->win) {
        struct mui_window *win = c->win;
        c->win = NULL;
        win->client = NULL;
        lp_wm_action a = { .type = LP_WM_CLOSE, .id = win->wm_id };
        lp_desktop_dispatch(&c->server->desktop, &a);
    }
}

static void client_destroy(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, destroy);
    wl_list_remove(&c->map.link);
    wl_list_remove(&c->unmap.link);
    wl_list_remove(&c->commit.link);
    wl_list_remove(&c->destroy.link);
    wl_list_remove(&c->request_move.link);
    wl_list_remove(&c->request_resize.link);
    wl_list_remove(&c->request_maximize.link);
    wl_list_remove(&c->request_minimize.link);
    wl_list_remove(&c->set_title.link);
    wl_list_remove(&c->new_popup.link);
    if (c->decoration) { wl_list_remove(&c->decoration_request_mode.link); wl_list_remove(&c->decoration_destroy.link); }
    if (c->win) c->win->client = NULL;
    free(c);
}

static void client_request_move(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, request_move);
    struct mui_server *server = c->server;
    if (!c->win || c->win->state == LP_WIN_ZOOMED) return;
    mui_window_begin_move(c->win, server->cursor->x, server->cursor->y);
}

static int handle_for_edges(uint32_t edges) {
    int n = edges & WLR_EDGE_TOP, s = edges & WLR_EDGE_BOTTOM, w = edges & WLR_EDGE_LEFT, e = edges & WLR_EDGE_RIGHT;
    if (n && w) return LP_HANDLE_NW;
    if (n && e) return LP_HANDLE_NE;
    if (s && w) return LP_HANDLE_SW;
    if (s && e) return LP_HANDLE_SE;
    if (n) return LP_HANDLE_N;
    if (s) return LP_HANDLE_S;
    if (w) return LP_HANDLE_W;
    return LP_HANDLE_E;
}

static void client_request_resize(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, request_resize);
    struct wlr_xdg_toplevel_resize_event *event = data;
    struct mui_server *server = c->server;
    if (!c->win || c->win->state != LP_WIN_NORMAL) return;
    mui_window_begin_resize(c->win, handle_for_edges(event->edges), server->cursor->x, server->cursor->y);
}

static void client_request_maximize(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, request_maximize);
    if (c->win) {
        lp_wm_action a = { .type = LP_WM_TOGGLE_ZOOM, .id = c->win->wm_id };
        lp_desktop_dispatch(&c->server->desktop, &a);
    }
    wlr_xdg_surface_schedule_configure(c->toplevel->base);
}

static void client_request_minimize(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, request_minimize);
    if (c->win) {
        lp_wm_action a = { .type = LP_WM_TOGGLE_SHADE, .id = c->win->wm_id };
        lp_desktop_dispatch(&c->server->desktop, &a);
    }
    wlr_xdg_surface_schedule_configure(c->toplevel->base);
}

static void client_set_title(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, set_title);
    if (!c->win || !c->toplevel->title) return;
    lp_wm_action a = { .type = LP_WM_SET_TITLE, .id = c->win->wm_id, .title = c->toplevel->title };
    lp_desktop_dispatch(&c->server->desktop, &a);
}

static void popup_new(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, new_popup);
    struct wlr_xdg_popup *popup = data;
    if (!c->scene_tree) return;
    struct wlr_scene_tree *tree = wlr_scene_xdg_surface_create(c->scene_tree, popup->base);
    popup->base->data = tree;
    /* Keep the popup on the desktop. */
    if (c->win) {
        struct mui_server *server = c->server;
        struct wlr_box box = { .x = -(c->win->rect.x + c->geometry.x), .y = -(c->win->rect.y + LP_TITLE_HEIGHT + c->geometry.y),
            .width = server->desktop_width, .height = server->desktop_height };
        wlr_xdg_popup_unconstrain_from_box(popup, &box);
    }
}

static void decoration_request_mode(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, decoration_request_mode);
    wlr_xdg_toplevel_decoration_v1_set_mode(c->decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void decoration_destroy(struct wl_listener *listener, void *data) {
    struct mui_xdg_client *c = wl_container_of(listener, c, decoration_destroy);
    wl_list_remove(&c->decoration_request_mode.link);
    wl_list_remove(&c->decoration_destroy.link);
    c->decoration = NULL;
}

static void new_toplevel_decoration(struct wl_listener *listener, void *data) {
    struct mui_server *server = wl_container_of(listener, server, new_toplevel_decoration);
    struct wlr_xdg_toplevel_decoration_v1 *decoration = data;
    struct mui_xdg_client *c = decoration->toplevel->base->data;
    if (!c) return;
    c->decoration = decoration;
    c->decoration_request_mode.notify = decoration_request_mode;
    wl_signal_add(&decoration->events.request_mode, &c->decoration_request_mode);
    c->decoration_destroy.notify = decoration_destroy;
    wl_signal_add(&decoration->events.destroy, &c->decoration_destroy);
    wlr_xdg_toplevel_decoration_v1_set_mode(decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void new_xdg_surface(struct wl_listener *listener, void *data) {
    struct mui_server *server = wl_container_of(listener, server, new_xdg_surface);
    struct wlr_xdg_surface *xdg = data;
    if (xdg->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) return; /* popups arrive through their parent's new_popup */
    struct mui_xdg_client *c = calloc(1, sizeof(*c));
    c->server = server;
    c->toplevel = xdg->toplevel;
    xdg->data = c;
    c->map.notify = client_map;
    wl_signal_add(&xdg->surface->events.map, &c->map);
    c->unmap.notify = client_unmap;
    wl_signal_add(&xdg->surface->events.unmap, &c->unmap);
    c->commit.notify = client_commit;
    wl_signal_add(&xdg->surface->events.commit, &c->commit);
    c->destroy.notify = client_destroy;
    wl_signal_add(&xdg->events.destroy, &c->destroy);
    c->request_move.notify = client_request_move;
    wl_signal_add(&xdg->toplevel->events.request_move, &c->request_move);
    c->request_resize.notify = client_request_resize;
    wl_signal_add(&xdg->toplevel->events.request_resize, &c->request_resize);
    c->request_maximize.notify = client_request_maximize;
    wl_signal_add(&xdg->toplevel->events.request_maximize, &c->request_maximize);
    c->request_minimize.notify = client_request_minimize;
    wl_signal_add(&xdg->toplevel->events.request_minimize, &c->request_minimize);
    c->set_title.notify = client_set_title;
    wl_signal_add(&xdg->toplevel->events.set_title, &c->set_title);
    c->new_popup.notify = popup_new;
    wl_signal_add(&xdg->events.new_popup, &c->new_popup);
}

void mui_xdg_init(struct mui_server *server) {
    server->new_xdg_surface.notify = new_xdg_surface;
    wl_signal_add(&server->xdg_shell->events.new_surface, &server->new_xdg_surface);
    server->new_toplevel_decoration.notify = new_toplevel_decoration;
    wl_signal_add(&server->decoration_manager->events.new_toplevel_decoration, &server->new_toplevel_decoration);
}
