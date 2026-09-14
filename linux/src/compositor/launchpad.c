/* The Launchpad on the scene (Linux, PARITY D32): one full-screen chrome in the top layer that
 * follows desktop.launchpad, over the wallpaper softened once per screen size, appearing with a
 * short scale-and-fade like Spotlight. Opened from Spotlight's All Applications pill; Esc, a click
 * on the scrim or launching an app closes it. */
#include <math.h>
#include <stdlib.h>

#include "maryui/lp_launchpad.h"
#include "maryui/lp_tokens.h"
#include "maryui/lp_wallpaper.h"
#include "chrome.h"
#include "window.h"

static cairo_surface_t *backdrop_for(struct mui_server *server, int w, int h) {
    if (server->launchpad_backdrop && server->launchpad_backdrop_w == w && server->launchpad_backdrop_h == h) return server->launchpad_backdrop;
    if (server->launchpad_backdrop) cairo_surface_destroy(server->launchpad_backdrop);
    cairo_surface_t *wp = lp_wallpaper_for(w, h, server->settings);
    server->launchpad_backdrop = lp_launchpad_backdrop(wp, w, h);
    server->launchpad_backdrop_w = w;
    server->launchpad_backdrop_h = h;
    cairo_surface_destroy(wp);
    return server->launchpad_backdrop;
}

static void paint_launchpad(lp_ctx *ctx, struct mui_chrome *chrome, void *data) {
    struct mui_server *server = data;
    lp_desktop *d = &server->desktop;
    if (!d->launchpad.open) return;
    lp_spotlight_item items[LP_SPOTLIGHT_MAX_ITEMS];
    int n = lp_desktop_launchpad_items(d, items, LP_SPOTLIGHT_MAX_ITEMS);
    if (d->launchpad.selection >= n) d->launchpad.selection = n > 0 ? n - 1 : 0;
    lp_launchpad_view view = {
        .query = &d->launchpad.query, .items = items, .count = n, .selection = d->launchpad.selection, .page = d->launchpad.page,
        .width = (float)chrome->width, .height = (float)chrome->height, .backdrop = server->launchpad_backdrop, .focus_field = 1,
    };
    lp_launchpad_result res;
    lp_launchpad_panel(ctx, &view, &res);
    if (ctx->pass != LP_PASS_EVENT) return;
    if (res.query_changed) { d->launchpad.selection = 0; d->launchpad.page = 0; ctx->dirty = 1; }
    if (res.hovered >= 0 && res.hovered != d->launchpad.selection) { d->launchpad.selection = res.hovered; ctx->dirty = 1; }
    if (res.page_pressed >= 0) { d->launchpad.page = res.page_pressed; ctx->dirty = 1; }
    if (res.activated >= 0) {
        lp_desktop_launchpad_activate(d, res.activated);   /* opens the app, closes the grid */
        mui_launchpad_request_sync(server);                /* CLOSES: deferred, never from here */
        ctx->dirty = 1;
    } else if (res.dismissed) {
        lp_desktop_launchpad_close(d);
        mui_launchpad_request_sync(server);
        ctx->dirty = 1;
    }
}

static void close_chrome(struct mui_server *server) {
    if (!server->launchpad) return;
    if (server->pointer_chrome == server->launchpad) server->pointer_chrome = NULL;
    mui_chrome_finish(server->launchpad);
    free(server->launchpad);
    server->launchpad = NULL;
    server->launchpad_anim.active = 0;
    mui_input_disarm_repeat(server);
}

static void settle(struct mui_server *server) {
    if (!server->launchpad) return;
    server->launchpad_anim.active = 0;
    wlr_scene_buffer_set_opacity(server->launchpad->node, 1.0f);
    wlr_scene_buffer_set_dest_size(server->launchpad->node, 0, 0);
    wlr_scene_node_set_position(&server->launchpad->node->node, 0, 0);
    mui_server_damage_all(server);
}

void mui_launchpad_sync(struct mui_server *server) {
    lp_desktop *d = &server->desktop;
    if (!d->launchpad.open) {
        close_chrome(server);
        return;
    }
    double now = mui_now_ms();
    int w = server->desktop_width, h = server->desktop_height;
    if (w <= 0 || h <= 0) return;
    if (!server->launchpad) {
        backdrop_for(server, w, h);
        server->launchpad = calloc(1, sizeof(*server->launchpad));
        mui_chrome_init(server->launchpad, server, server->layer_spotlight, w, h, paint_launchpad, server);
        server->launchpad->ctx.focus = LP_LAUNCHPAD_QUERY_ID;
        server->launchpad->opaque = 1;
        wlr_scene_node_set_position(&server->launchpad->node->node, 0, 0);
        /* the focused built-in window gives up its caret: one caret on screen */
        const lp_window_record *focused = lp_wm_focused(&d->wm);
        struct mui_window *win = focused ? mui_window_find(server, focused->id) : NULL;
        if (win && win->kind == MUI_WINDOW_APP && win->chrome.ctx.focus) {
            win->chrome.ctx.focus = 0;
            mui_chrome_damage_all(&win->chrome);
            mui_chrome_repaint(&win->chrome, now);
        }
        mui_input_disarm_repeat(server);
        if (!server->settings->reduced_motion) {
            /* like lp-spotlight-in, from a little larger: opacity 0→1, scale 1.06→1 about the centre */
            mui_tween_start(&server->launchpad_anim, now, LP_MOTION_NORMAL_MS, LP_MOTION_EASE_OUT);
            wlr_scene_buffer_set_opacity(server->launchpad->node, 0.0f);
            mui_server_schedule_frame(server);
        }
    }
    mui_chrome_damage_all(server->launchpad);
    mui_chrome_repaint(server->launchpad, now);
}

static void idle_sync(void *data) {
    struct mui_server *server = data;
    server->launchpad_idle = NULL;
    mui_launchpad_sync(server);
}

void mui_launchpad_request_sync(struct mui_server *server) {
    if (server->launchpad_idle) return;
    struct wl_event_loop *loop = wl_display_get_event_loop(server->display);
    server->launchpad_idle = wl_event_loop_add_idle(loop, idle_sync, server);
}

int mui_launchpad_hit(struct mui_server *server, double lx, double ly, struct mui_hit *hit) {
    if (!server->launchpad) return 0;
    int nx, ny;
    wlr_scene_node_coords(&server->launchpad->node->node, &nx, &ny);
    hit->chrome = server->launchpad;
    hit->sx = lx - nx;
    hit->sy = ly - ny;
    return 1;   /* the whole screen while it is up */
}

int mui_launchpad_animate(struct mui_server *server, double now_ms) {
    if (!server->launchpad || !server->launchpad_anim.active) return 0;
    struct wlr_scene_buffer *node = server->launchpad->node;
    int w = server->launchpad->width, h = server->launchpad->height;
    float p = mui_tween_progress(&server->launchpad_anim, now_ms);
    if (!server->launchpad_anim.active) {
        settle(server);
        return 0;
    }
    float s = 1.06f - 0.06f * p;
    int dw = (int)lroundf(w * s), dh = (int)lroundf(h * s);
    wlr_scene_buffer_set_opacity(node, p);
    wlr_scene_buffer_set_dest_size(node, dw, dh);
    wlr_scene_node_set_position(&node->node, (w - dw) / 2, (h - dh) / 2);
    return 1;
}

/* The wallpaper changed (settings) or the screen did: the softened copy is stale. */
void mui_launchpad_invalidate(struct mui_server *server) {
    if (server->launchpad_backdrop) cairo_surface_destroy(server->launchpad_backdrop);
    server->launchpad_backdrop = NULL;
    server->launchpad_backdrop_w = server->launchpad_backdrop_h = 0;
    if (server->desktop.launchpad.open) { lp_desktop_launchpad_close(&server->desktop); mui_launchpad_sync(server); }
}

void mui_launchpad_finish(struct mui_server *server) {
    if (server->launchpad_idle) { wl_event_source_remove(server->launchpad_idle); server->launchpad_idle = NULL; }
    close_chrome(server);
    if (server->launchpad_backdrop) cairo_surface_destroy(server->launchpad_backdrop);
    server->launchpad_backdrop = NULL;
}
