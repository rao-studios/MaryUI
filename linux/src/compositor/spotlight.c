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

/* The view for the current model, minus the two fields only the host knows. */
static lp_spotlight_view view_of(struct mui_server *server, const lp_spotlight_item *items, int count) {
    lp_spotlight_view v = lp_desktop_spotlight_view(&server->desktop, items, count);
    v.focus_bar = 1;
    /* An open menu grows the panel downward; keep it on the desktop. */
    float room = (float)server->desktop_height - (float)(server->spotlight_y + EXTENT) - LP_SPACE_2;
    v.max_h = room > 0 ? room : 0;
    return v;
}

static void paint_spotlight(lp_ctx *ctx, struct mui_chrome *chrome, void *data) {
    struct mui_server *server = data;
    lp_desktop *d = &server->desktop;
    if (!d->spotlight.open) return;
    lp_spotlight_item results[LP_SPOTLIGHT_MAX_RESULTS];
    int n = lp_desktop_spotlight_results(d, results, LP_SPOTLIGHT_MAX_RESULTS);
    if (d->spotlight.selection >= n) d->spotlight.selection = n > 0 ? n - 1 : 0;
    lp_spotlight_view view = view_of(server, results, n);
    lp_spotlight_result res;
    lp_spotlight_panel(ctx, EXTENT, EXTENT, &view, &res);
    server->spotlight_panel = res.panel;
    if (ctx->pass != LP_PASS_EVENT) return;

    if (res.query_changed) {
        lp_desktop_spotlight_query_changed(d);  /* selection 0; a typed query drops the pill */
        mui_spotlight_resize(server);           /* the commands section may have gone with it */
        ctx->dirty = 1;
    }
    if (res.hovered >= 0 && res.hovered != d->spotlight.selection) { d->spotlight.selection = res.hovered; ctx->dirty = 1; }

    /* The command pills. Press opens, as a pull-down menu does. */
    if (res.menu_pressed >= 0) {
        lp_desktop_toggle_menu(d, res.menu_pressed);
        mui_spotlight_resize(server);
        ctx->dirty = 1;
    } else if (d->open_menu >= 0 && d->open_menu < LP_DESKTOP_MENU_COUNT &&
               res.menu_hovered >= 0 && res.menu_hovered != d->open_menu) {
        lp_desktop_toggle_menu(d, res.menu_hovered);   /* hover switches while one is open */
        mui_spotlight_resize(server);
        ctx->dirty = 1;
    }

    /* The open menu's entries. */
    if (res.entry_hovered != d->menu_active) { d->menu_active = res.entry_hovered; ctx->dirty = 1; }
    if (res.entry_selected >= 0) {
        lp_desktop_select_menu_entry(d, res.entry_selected);  /* runs it, closes the menu */
        lp_spotlight_close(&d->spotlight);                    /* picking a command dismisses the panel */
        mui_spotlight_request_sync(server);                   /* CLOSES: deferred, never from here */
        ctx->dirty = 1;
    }
    if (res.activated >= 0) {
        lp_desktop_spotlight_activate(d, res.activated); /* opens, focuses or spawns; closes Spotlight */
        mui_spotlight_request_sync(server);
        ctx->dirty = 1;
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

/* The resting state lp-spotlight-in ends in. A resize has to get here first:
 * the tween scales the node from the buffer's size, so changing that size
 * under it makes the scale and the position jump. */
static void spotlight_settle(struct mui_server *server) {
    if (!server->spotlight) return;
    server->spotlight_anim.active = 0;
    wlr_scene_buffer_set_opacity(server->spotlight->node, 1.0f);
    wlr_scene_buffer_set_dest_size(server->spotlight->node, 0, 0);
    wlr_scene_node_set_position(&server->spotlight->node->node, server->spotlight_x, server->spotlight_y);
    mui_server_damage_all(server);
}

/*
 * Sizes the open panel for the current model — a command pill opened, closed or
 * switched, or a query hid the commands. Safe to call from inside the panel's
 * own EVENT pass, because it never destroys the chrome; mui_spotlight_sync
 * does, so that one must stay deferred (see mui_spotlight_request_sync).
 * The resize is synchronous on purpose: mui_chrome_event repaints as soon as
 * the pass returns, and a deferred resize would paint one frame of an open
 * menu clipped to the old, too-short buffer.
 */
void mui_spotlight_resize(struct mui_server *server) {
    if (!server->spotlight || !server->desktop.spotlight.open) return;
    lp_spotlight_item results[LP_SPOTLIGHT_MAX_RESULTS];
    int n = lp_desktop_spotlight_results(&server->desktop, results, LP_SPOTLIGHT_MAX_RESULTS);
    lp_spotlight_view v = view_of(server, results, n);
    lp_size want = lp_spotlight_measure(&v), base = lp_spotlight_max_size(&v);
    int h = (int)(want.h > base.h ? want.h : base.h) + 2 * EXTENT;
    if (h == server->spotlight->height) return;
    spotlight_settle(server);
    mui_chrome_resize(server->spotlight, server->spotlight->width, h);
}

void mui_spotlight_sync(struct mui_server *server) {
    lp_desktop *d = &server->desktop;
    if (!d->spotlight.open) {
        close_chrome(server);
        return;
    }
    double now = mui_now_ms();
    if (!server->spotlight) {
        /* One buffer the size of the tallest panel with no pill open: typing
         * never reallocates. Opening a pill is a click, and resizes. */
        lp_spotlight_item results[LP_SPOTLIGHT_MAX_RESULTS];
        int n = lp_desktop_spotlight_results(d, results, LP_SPOTLIGHT_MAX_RESULTS);
        lp_spotlight_view v = lp_desktop_spotlight_view(d, results, n);
        lp_size max = lp_spotlight_max_size(&v);
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
        spotlight_settle(server);
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
