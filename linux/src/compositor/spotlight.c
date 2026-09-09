/* Spotlight on the scene: one fixed-size chrome in the top layer that follows
 * desktop.spotlight (open, query, selection), appears with lp-spotlight-in,
 * and is hit only inside the painted panel (its shadow falls through). */
#include <math.h>
#include <stdlib.h>

#include "maryui/components/lp_spotlight_panel.h"
#include "maryui/lp_tokens.h"
#include "chrome.h"
#include "window.h"

#define EXTENT LP_SPOTLIGHT_SHADOW_EXTENT

static void paint_spotlight(lp_ctx *ctx, struct mui_chrome *chrome, void *data) {
    struct mui_server *server = data;
    lp_desktop *d = &server->desktop;
    if (!d->spotlight.open) return;
    lp_spotlight_item results[LP_SPOTLIGHT_MAX_RESULTS];
    int n = lp_desktop_spotlight_results(d, results, LP_SPOTLIGHT_MAX_RESULTS);
    if (d->spotlight.selection >= n) d->spotlight.selection = n > 0 ? n - 1 : 0;
    lp_spotlight_view view = { .query = &d->spotlight.query, .items = results, .count = n, .selection = d->spotlight.selection, .focus_bar = 1 };
    lp_spotlight_result res;
    lp_spotlight_panel(ctx, EXTENT, EXTENT, &view, &res);
    server->spotlight_panel = res.panel;
    if (ctx->pass == LP_PASS_EVENT) {
        if (res.query_changed) { d->spotlight.selection = 0; ctx->dirty = 1; }
        if (res.hovered >= 0 && res.hovered != d->spotlight.selection) { d->spotlight.selection = res.hovered; ctx->dirty = 1; }
        if (res.activated >= 0) {
            lp_desktop_spotlight_activate(d, res.activated); /* opens, focuses or spawns; closes Spotlight */
            mui_spotlight_request_sync(server);
            ctx->dirty = 1;
        }
    }
}

static void close_chrome(struct mui_server *server) {
    if (!server->spotlight) return;
    if (server->pointer_chrome == server->spotlight) server->pointer_chrome = NULL;
    mui_chrome_finish(server->spotlight);
    free(server->spotlight);
    server->spotlight = NULL;
    server->spotlight_anim.active = 0;
    mui_input_disarm_repeat(server);
}

void mui_spotlight_sync(struct mui_server *server) {
    lp_desktop *d = &server->desktop;
    if (!d->spotlight.open) {
        close_chrome(server);
        return;
    }
    double now = mui_now_ms();
    if (!server->spotlight) {
        /* One buffer the size of the tallest panel: typing never reallocates. */
        lp_size max = lp_spotlight_max_size(0);
        int w = (int)max.w + 2 * EXTENT, h = (int)max.h + 2 * EXTENT;
        server->spotlight = calloc(1, sizeof(*server->spotlight));
        mui_chrome_init(server->spotlight, server, server->layer_spotlight, w, h, paint_spotlight, server);
        server->spotlight->ctx.focus = LP_SPOTLIGHT_QUERY_ID;
        int x = (server->desktop_width - (int)max.w) / 2 - EXTENT;
        int y = (int)lroundf(server->desktop_height * LP_SPOTLIGHT_Y_FRACTION - LP_SIZE_SPOTLIGHT_BAR_HEIGHT / 2 - LP_SPOTLIGHT_PAD) - EXTENT;
        wlr_scene_node_set_position(&server->spotlight->node->node, x, y);
        server->spotlight_x = x;
        server->spotlight_y = y;
        server->spotlight_panel = LP_RECT(EXTENT, EXTENT, max.w, max.h);
        /* The focused built-in window gives up its caret: one caret on screen. */
        const lp_window_record *focused = lp_wm_focused(&d->wm);
        struct mui_window *win = focused ? mui_window_find(server, focused->id) : NULL;
        if (win && win->kind == MUI_WINDOW_APP && win->chrome.ctx.focus) {
            win->chrome.ctx.focus = 0;
            mui_chrome_damage_all(&win->chrome);
            mui_chrome_repaint(&win->chrome, now);
        }
        mui_input_disarm_repeat(server);
        if (!server->settings->reduced_motion) {
            /* Spotlight.module.css lp-spotlight-in: opacity 0→1, scale(.96)→1, motion.fast ease-out. */
            mui_tween_start(&server->spotlight_anim, now, LP_MOTION_FAST_MS, LP_MOTION_EASE_OUT);
            wlr_scene_buffer_set_opacity(server->spotlight->node, 0.0f);
            mui_server_schedule_frame(server);
        }
    }
    mui_chrome_damage_all(server->spotlight);
    mui_chrome_repaint(server->spotlight, now);
}

static void idle_sync(void *data) {
    struct mui_server *server = data;
    server->spotlight_idle = NULL;
    mui_spotlight_sync(server);
}

void mui_spotlight_request_sync(struct mui_server *server) {
    if (server->spotlight_idle) return;
    struct wl_event_loop *loop = wl_display_get_event_loop(server->display);
    server->spotlight_idle = wl_event_loop_add_idle(loop, idle_sync, server);
}

int mui_spotlight_hit(struct mui_server *server, double lx, double ly, struct mui_hit *hit) {
    if (!server->spotlight) return 0;
    int nx, ny;
    wlr_scene_node_coords(&server->spotlight->node->node, &nx, &ny);
    double sx = lx - nx, sy = ly - ny;
    lp_rect p = server->spotlight_panel;
    if (sx < p.x || sy < p.y || sx >= p.x + p.w || sy >= p.y + p.h) return 0;
    hit->chrome = server->spotlight;
    hit->sx = sx;
    hit->sy = sy;
    return 1;
}

int mui_spotlight_animate(struct mui_server *server, double now_ms) {
    if (!server->spotlight || !server->spotlight_anim.active) return 0;
    struct wlr_scene_buffer *node = server->spotlight->node;
    int w = server->spotlight->width, h = server->spotlight->height;
    float p = mui_tween_progress(&server->spotlight_anim, now_ms);
    if (!server->spotlight_anim.active) {
        wlr_scene_buffer_set_opacity(node, 1.0f);
        wlr_scene_buffer_set_dest_size(node, 0, 0);
        wlr_scene_node_set_position(&node->node, server->spotlight_x, server->spotlight_y);
        mui_server_damage_all(server);
        return 0;
    }
    float s = 0.96f + 0.04f * p;
    int dw = (int)lroundf(w * s), dh = (int)lroundf(h * s);
    wlr_scene_buffer_set_opacity(node, p);
    wlr_scene_buffer_set_dest_size(node, dw, dh);
    wlr_scene_node_set_position(&node->node, server->spotlight_x + (w - dw) / 2, server->spotlight_y + (h - dh) / 2);
    return 1;
}

void mui_spotlight_finish(struct mui_server *server) {
    if (server->spotlight_idle) { wl_event_source_remove(server->spotlight_idle); server->spotlight_idle = NULL; }
    close_chrome(server);
}
