/* Windows on the scene (window.h): the chrome, the client tree, and the
 * motion engine's per-window target plus Window.tsx's close and shade
 * animations, all applied as scene-node position, buffer scale/crop and opacity. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/components/lp_surface.h"
#include "maryui/components/lp_window.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_geometry.h"
#include "maryui/lp_tokens.h"
#include "window.h"

/* Window.tsx: the close animation runs motion.fast and the CLOSE follows 80 ms later; the shade transition is motion.slow. */
#define CLOSE_MS (LP_MOTION_FAST_MS + 80.0f)
#define SHADE_MS LP_MOTION_SLOW_MS

static int reduced_motion(const struct mui_window *win) { return win->server->settings->reduced_motion; }

static int chrome_height_for(const struct mui_window *win, enum lp_window_state state, float h) {
    return (int)(state == LP_WIN_SHADED ? LP_TITLE_HEIGHT : h) + win->margin_t + win->margin_b;
}

static void paint_window(lp_ctx *ctx, struct mui_chrome *chrome, void *data) {
    struct mui_window *win = data;
    /* While the shade animation runs, the tall frame is painted and the scene crops it. */
    enum lp_window_state state = win->shade_anim.active ? LP_WIN_NORMAL : win->state;
    lp_window_view view = {
        .title = NULL, .focused = win->focused, .state = state, .resizable = 1,
        .rect = LP_RECT(win->margin_l, win->margin_t, win->rect.w, win->rect.h),
    };
    int i = lp_wm_find(&win->server->desktop.wm, win->wm_id);
    const lp_window_record *rec = i >= 0 ? &win->server->desktop.wm.windows[i] : NULL;
    if (rec) { view.title = rec->title; view.resizable = rec->resizable; }
    ctx->active_window = win->focused;
    lp_title_bar_result bar;
    lp_rect body = lp_window_chrome(ctx, &view, &bar);
    win->body = body;
    if (ctx->pass == LP_PASS_DRAW) {
        /* The frame is opaque metal except its rounded corners: tell the scene, so the wallpaper under a window is never drawn. */
        float r = state == LP_WIN_ZOOMED ? 0 : LP_RADIUS_WINDOW;
        int fx = win->margin_l, fy = win->margin_t, fw = (int)win->rect.w, fh = (int)(state == LP_WIN_SHADED ? LP_TITLE_HEIGHT : win->rect.h);
        int cr = (int)ceilf(r);
        pixman_region32_t opaque;
        pixman_region32_init_rect(&opaque, fx + cr, fy, fw - 2 * cr, fh);
        if (fh > 2 * cr) {
            pixman_region32_union_rect(&opaque, &opaque, fx, fy + cr, cr, fh - 2 * cr);
            pixman_region32_union_rect(&opaque, &opaque, fx + fw - cr, fy + cr, cr, fh - 2 * cr);
        }
        mui_chrome_set_opaque_region(chrome, &opaque);
        pixman_region32_fini(&opaque);
    }
    if (win->kind == MUI_WINDOW_APP && win->instance && win->instance->app->paint && body.h > 0) {
        int draw = ctx->pass == LP_PASS_DRAW && ctx->cr;
        int paint_body = 1;
        /* A title-strip repaint (sheen, lights) does not run the app's paint.
         * This has to be a region test, not a bounding box: a strip repaint that
         * also damages the bottom lip for the corners would otherwise have a
         * bbox spanning the whole window and repaint the entire widget tree. */
        if (draw) paint_body = lp_clip_intersects(ctx->cr, body);
        if (paint_body) {
            if (draw) {
                cairo_save(ctx->cr);
                cairo_rectangle(ctx->cr, body.x, body.y, body.w, body.h);
                cairo_clip(ctx->cr);
                /* Surface body underneath every app. */
                lp_surface_paint(ctx->cr, body, (lp_surface_opts){ .variant = LP_VARIANT_BODY, .radius = 0 }, lp_surface_motion_of(ctx));
            }
            win->instance->app->paint(win->instance->state, ctx, body, &win->server->desktop);
            if (draw) cairo_restore(ctx->cr);
        }
    }
    if (ctx->pass == LP_PASS_EVENT && rec && !win->closing) {
        struct mui_server *server = win->server;
        lp_wm_action a = { .id = win->wm_id };
        if (bar.close) { mui_window_request_close(win); return; }
        if (bar.shade) { a.type = LP_WM_TOGGLE_SHADE; lp_desktop_dispatch(&server->desktop, &a); return; }
        if (bar.zoom || bar.double_click) { a.type = LP_WM_TOGGLE_ZOOM; lp_desktop_dispatch(&server->desktop, &a); return; }
        if (bar.drag_start && win->state != LP_WIN_ZOOMED) mui_window_begin_move(win, server->cursor->x, server->cursor->y);
    }
}

/* MARK: - Grabs */

void mui_window_begin_move(struct mui_window *win, double lx, double ly) {
    struct mui_server *server = win->server;
    server->grab.kind = MUI_GRAB_MOVE;
    server->grab.win = win;
    server->grab.start = win->rect;
    server->grab.sx = lx;
    server->grab.sy = ly;
    lp_window_motion_begin_drag(&win->motion, (float)(lx - win->rect.x), (float)(ly - win->rect.y));
    lp_motion_engine_wake(&server->engine, mui_now_ms());
}

void mui_window_begin_resize(struct mui_window *win, int handle, double lx, double ly) {
    struct mui_server *server = win->server;
    server->grab.kind = MUI_GRAB_RESIZE;
    server->grab.win = win;
    server->grab.handle = handle;
    server->grab.start = win->rect;
    server->grab.sx = lx;
    server->grab.sy = ly;
    lp_window_motion_begin_resize(&win->motion);
    lp_motion_engine_wake(&server->engine, mui_now_ms());
}

/* MARK: - Close */

void mui_window_request_close(struct mui_window *win) {
    if (mui_xdg_request_close(win)) return;
    if (win->closing) return;
    struct mui_server *server = win->server;
    if (reduced_motion(win)) {
        lp_wm_action a = { .type = LP_WM_CLOSE, .id = win->wm_id };
        lp_desktop_dispatch(&server->desktop, &a);
        return;
    }
    win->closing = 1;
    mui_tween_start(&win->close_anim, mui_now_ms(), LP_MOTION_FAST_MS, MUI_EASE_IN);
    if (server->grab.win == win) { server->grab.kind = MUI_GRAB_NONE; server->grab.win = NULL; }
    if (server->pointer_chrome == &win->chrome) server->pointer_chrome = NULL;
    mui_server_schedule_frame(server);
}

/* MARK: - The transform */

/* The web's `transform` on the chrome element: translate, then scale about an
 * origin given in window-rect coordinates. The chrome buffer carries the shadow
 * margin, so the origin shifts by it; the client tree follows in xdg.c. */
static void apply_transform(struct mui_window *win, float tx, float ty, float sx, float sy, float ox, float oy, float opacity) {
    struct mui_chrome *ch = &win->chrome;
    if (!ch->node) return;
    double t_start = win->server->debug_frames ? mui_now_ms() : 0;
    float cx = ox + win->margin_l, cy = oy + win->margin_t;
    float nx = win->rect.x - win->margin_l + tx + cx * (1 - sx);
    float ny = win->rect.y - win->margin_t + ty + cy * (1 - sy);
    wlr_scene_node_set_position(&win->tree->node, (int)lroundf(nx), (int)lroundf(ny));
    int w = ch->width, h = ch->height;
    int crop = win->visual_h > 0;
    if (crop) {
        h = win->margin_t + (int)lroundf(win->visual_h);
        if (h > ch->height) h = ch->height;
        struct wlr_fbox src = { 0, 0, w, h };
        wlr_scene_buffer_set_source_box(ch->node, &src);
    } else {
        wlr_scene_buffer_set_source_box(ch->node, NULL);
    }
    int scaled = crop || fabsf(sx - 1) > 1e-4f || fabsf(sy - 1) > 1e-4f;
    int dw = 0, dh = 0;
    if (scaled) {
        dw = (int)lroundf(w * sx);
        dh = (int)lroundf(h * sy);
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
    }
    /* Hand the scene only what changed: each setter re-walks the scene graph. */
    if (!win->applied.valid || dw != win->applied.dw || dh != win->applied.dh) wlr_scene_buffer_set_dest_size(ch->node, dw, dh);
    /* Pixman's bilinear path is slow at 1280×800; nearest is fine for a 3.5 % jelly or a 300 ms flight. */
    if (!win->applied.valid || scaled != win->applied.filter_nearest) wlr_scene_buffer_set_filter_mode(ch->node, scaled ? WLR_SCALE_FILTER_NEAREST : WLR_SCALE_FILTER_BILINEAR);
    if (!win->applied.valid || opacity != win->applied.opacity) wlr_scene_buffer_set_opacity(ch->node, opacity);
    int opaque_on = !scaled && opacity >= 1.0f;
    if (!win->applied.valid || opaque_on != win->applied.opaque_on) mui_chrome_apply_opaque(ch, opaque_on);
    win->applied.dw = dw;
    win->applied.dh = dh;
    win->applied.opacity = opacity;
    win->applied.filter_nearest = scaled;
    win->applied.opaque_on = opaque_on;
    win->applied.valid = 1;
    if (win->server->debug_frames) {
        double ms = mui_now_ms() - t_start;
        struct mui_frame_stats *s = &win->server->frame_stats;
        s->transform_ms += ms;
        if (ms > s->transform_max_ms) s->transform_max_ms = ms;
    }
    if (win->was_scaled && !scaled) mui_server_damage_all(win->server);
    win->was_scaled = scaled;
    if (win->kind == MUI_WINDOW_XDG) mui_xdg_client_transform(win, sx, sy, win->visual_h);
}

void mui_window_apply_motion(struct mui_window *win) {
    lp_window_motion_out *o = &win->motion.out;
    lp_ctx *ctx = &win->chrome.ctx;
    /* The corners live at both ends of the frame, so they damage more than the
     * title strip. lp_window_motion only republishes them once one has actually
     * moved a quarter-pixel, which is what keeps this from firing every frame. */
    int corners_moved = fabsf(ctx->corners.tl - o->corners.tl) > 0.01f || fabsf(ctx->corners.tr - o->corners.tr) > 0.01f ||
                        fabsf(ctx->corners.br - o->corners.br) > 0.01f || fabsf(ctx->corners.bl - o->corners.bl) > 0.01f;
    if (corners_moved || fabsf(ctx->sheen_x - o->sheen_x) > 0.0005f || fabsf(ctx->tilt - o->tilt_deg) > 0.005f ||
        fabsf(ctx->vx - o->vx) > 0.005f || fabsf(ctx->vx_lag - o->vx_lag) > 0.005f || fabsf(ctx->speed - o->speed) > 0.005f ||
        fabsf(ctx->slosh_deg - o->slosh_deg) > 0.01f || fabsf(ctx->slosh_y - o->slosh_y_px) > 0.01f ||
        fabsf(ctx->slosh_x - o->slosh_x_px) > 0.01f || fabsf(ctx->grain_x - o->grain_x) > 0.05f ||
        fabsf(ctx->grain_y - o->grain_y) > 0.05f) {
        ctx->sheen_x = o->sheen_x;
        ctx->tilt = o->tilt_deg;
        ctx->vx = o->vx;
        ctx->vx_lag = o->vx_lag;
        ctx->speed = o->speed;
        ctx->slosh_deg = o->slosh_deg;
        ctx->slosh_y = o->slosh_y_px;
        ctx->slosh_x = o->slosh_x_px;
        ctx->grain_x = o->grain_x;
        ctx->grain_y = o->grain_y;
        ctx->corners = o->corners;
        ctx->radius_k = o->radius_k;
        /* The title bar carries the sheen band and the lights; body surfaces keep a static sheen (PARITY.md). */
        mui_chrome_damage(&win->chrome, LP_RECT(win->margin_l, win->margin_t, win->rect.w, LP_TITLE_HEIGHT));
        if (corners_moved && win->state != LP_WIN_SHADED) {
            float lip = fmaxf(o->corners.bl, o->corners.br) + 1;
            mui_chrome_damage(&win->chrome, LP_RECT(win->margin_l, win->margin_t + win->rect.h - lip, win->rect.w, lip));
        }
        double t0 = mui_now_ms();
        mui_chrome_repaint(&win->chrome, t0);
        if (win->server->debug_frames) {
            double ms = mui_now_ms() - t0;
            struct mui_frame_stats *s = &win->server->frame_stats;
            s->paint_ms += ms;
            s->paints++;
            if (ms > s->paint_max_ms) s->paint_max_ms = ms;
        }
    }
    /* Only the title strip repaints while a window moves (PARITY.md D4), so the
     * body keeps whatever grain offset it last painted with. Once the motion
     * settles, repaint the whole chrome so the brushed skin lines up again
     * across the seam between the title bar and the body. */
    if (!o->identity) {
        win->was_moving = 1;
    } else if (win->was_moving) {
        win->was_moving = 0;
        mui_chrome_damage_all(&win->chrome);
        mui_chrome_repaint(&win->chrome, mui_now_ms());
    }

    if (win->closing) return; /* mui_windows_animate owns the transform */
    float sx = o->sx, sy = o->sy;
    if (win->kind == MUI_WINDOW_XDG) { sx = o->fly_sx; sy = o->fly_sy; } /* clients keep their pixels: no jelly (PARITY.md) */
    apply_transform(win, o->tx, o->ty, sx, sy, o->origin_x, o->origin_y, 1.0f);
}

static int window_step(lp_motion_target *t, float dt, double now_ms) {
    struct mui_window *win = t->user;
    struct mui_server *server = win->server;
    lp_rect live = win->rect;
    if (win->state == LP_WIN_SHADED) live.h = LP_TITLE_HEIGHT;
    int active = lp_window_motion_step(&win->motion, dt, now_ms, live, (float)server->desktop_width, &lp_motion_live,
                                       server->settings->reduced_motion);
    mui_window_apply_motion(win);
    return active;
}

void mui_window_place(struct mui_window *win, lp_rect rect) {
    win->rect = rect;
    mui_window_apply_motion(win);
}

/* MARK: - Lifecycle */

struct mui_window *mui_window_find(struct mui_server *server, const char *wm_id) {
    struct mui_window *win;
    wl_list_for_each(win, &server->windows, link) {
        if (strcmp(win->wm_id, wm_id) == 0) return win;
    }
    return NULL;
}

static struct mui_window *window_create(struct mui_server *server, const lp_window_record *rec) {
    struct mui_window *win = calloc(1, sizeof(*win));
    win->server = server;
    snprintf(win->wm_id, sizeof win->wm_id, "%s", rec->id);
    win->kind = MUI_WINDOW_APP;
    win->instance = lp_desktop_instance(&server->desktop, rec->id);
    lp_window_shadow_margins(&win->margin_l, &win->margin_t, &win->margin_r, &win->margin_b);
    win->tree = wlr_scene_tree_create(server->layer_windows);
    win->rect = rec->rect;
    win->state = rec->state;
    mui_chrome_init(&win->chrome, server, win->tree, (int)rec->rect.w + win->margin_l + win->margin_r,
                    chrome_height_for(win, rec->state, rec->rect.h), paint_window, win);
    win->chrome.node->node.data = &win->chrome;
    win->chrome.ctx.active_window = 1;
    lp_window_motion_init(&win->motion);
    win->target.step = window_step;
    win->target.user = win;
    lp_motion_engine_add(&server->engine, &win->target);
    wl_list_insert(&server->windows, &win->link);
    return win;
}

void mui_window_destroy(struct mui_window *win) {
    struct mui_server *server = win->server;
    lp_motion_engine_remove(&server->engine, &win->target);
    if (win->client) win->client->win = NULL;
    if (server->pointer_chrome == &win->chrome) server->pointer_chrome = NULL;
    if (server->grab.win == win) server->grab.kind = MUI_GRAB_NONE, server->grab.win = NULL;
    mui_drag_window_gone(server, &win->chrome);
    mui_chrome_finish(&win->chrome);
    wlr_scene_node_destroy(&win->tree->node);
    wl_list_remove(&win->link);
    free(win);
}

static int cmp_z(const void *a, const void *b) {
    const struct mui_window *const *wa = a, *const *wb = b;
    return (*wa)->z - (*wb)->z;
}

void mui_windows_sync(struct mui_server *server) {
    lp_desktop *d = &server->desktop;
    double now = mui_now_ms();
    int reduced = server->settings->reduced_motion;
    /* Drop windows the WM no longer has. */
    struct mui_window *win, *tmp;
    wl_list_for_each_safe(win, tmp, &server->windows, link) {
        if (lp_wm_find(&d->wm, win->wm_id) < 0) mui_window_destroy(win);
    }
    /* Create or update the rest. */
    struct mui_window *sorted[LP_WM_MAX_WINDOWS];
    int n = 0;
    for (int i = 0; i < d->wm.count; i++) {
        const lp_window_record *rec = &d->wm.windows[i];
        win = mui_window_find(server, rec->id);
        int fresh = win == NULL;
        if (!win) win = window_create(server, rec);
        /* Instances compact on close (lp_desktop.c sync_instances): never keep a stale pointer. */
        if (win->kind == MUI_WINDOW_APP) win->instance = lp_desktop_instance(d, rec->id);
        int focused = d->wm.focused == i;
        lp_rect prev = win->rect;
        enum lp_window_state prev_state = win->state;
        int rect_changed = (int)rec->rect.w != (int)prev.w || (int)rec->rect.h != (int)prev.h;
        int state_changed = prev_state != rec->state;
        int focus_changed = win->focused != focused;
        /* Only repaint what actually changed. This used to include
          * `|| grab.kind == MUI_GRAB_NONE`, which is the normal state, so every
          * model change repainted every window. */
        int repaint = rect_changed || state_changed || focus_changed;
        if (focus_changed) {
            /* Blur drops the widget focus (the caret) and stops a repeating key. */
            if (!focused) win->chrome.ctx.focus = 0;
            mui_input_disarm_repeat(server);
        }
        win->focused = focused;
        win->state = rec->state;
        win->z = rec->z;

        int shade_toggle = !fresh && state_changed && (prev_state == LP_WIN_SHADED || rec->state == LP_WIN_SHADED);
        int zoom_toggle = !fresh && state_changed && (prev_state == LP_WIN_ZOOMED || rec->state == LP_WIN_ZOOMED);
        if (shade_toggle && !reduced) {
            /* Window.module.css: `transition: height var(--lp-motion-slow) var(--lp-motion-ease-out)`. */
            float from = win->visual_h > 0 ? win->visual_h : (prev_state == LP_WIN_SHADED ? LP_TITLE_HEIGHT : prev.h);
            float to = rec->state == LP_WIN_SHADED ? LP_TITLE_HEIGHT : rec->rect.h;
            win->shade_from_h = from;
            win->shade_to_h = to;
            win->visual_h = from;
            mui_tween_start(&win->shade_anim, now, SHADE_MS, LP_MOTION_EASE_OUT);
            /* The tall frame is what gets cropped: make sure the chrome holds it. */
            mui_chrome_resize(&win->chrome, (int)rec->rect.w + win->margin_l + win->margin_r, chrome_height_for(win, LP_WIN_NORMAL, rec->rect.h));
            mui_server_schedule_frame(server);
        } else if ((rect_changed || state_changed) && !win->shade_anim.active) {
            mui_chrome_resize(&win->chrome, (int)rec->rect.w + win->margin_l + win->margin_r, chrome_height_for(win, rec->state, rec->rect.h));
        }
        /* Window.tsx: zooming records the live rect and flies from it (FLIP). */
        if (zoom_toggle && !lp_rects_equal(prev, rec->rect)) lp_window_motion_fly_from(&win->motion, prev, rec->rect, reduced);
        mui_window_place(win, rec->rect);
        if (repaint) mui_chrome_damage_all(&win->chrome);
        mui_chrome_repaint(&win->chrome, now);
        if (win->kind == MUI_WINDOW_XDG) mui_xdg_window_synced(win, focused, rect_changed || state_changed);
        if (focus_changed && focused) mui_xdg_keyboard_focus(server, win);
        if (n < LP_WM_MAX_WINDOWS) sorted[n++] = win;
    }
    qsort(sorted, (size_t)n, sizeof sorted[0], cmp_z);
    for (int i = 0; i < n; i++) wlr_scene_node_raise_to_top(&sorted[i]->tree->node);
    /* Window.tsx nudges the engine on every rect/state change so the sheen re-settles. */
    lp_motion_engine_wake(&server->engine, now);
}

/* MARK: - Animations */

static void finish_shade(struct mui_window *win) {
    win->visual_h = 0;
    mui_chrome_resize(&win->chrome, (int)win->rect.w + win->margin_l + win->margin_r, chrome_height_for(win, win->state, win->rect.h));
    mui_chrome_damage_all(&win->chrome);
    mui_chrome_repaint(&win->chrome, mui_now_ms());
    mui_window_apply_motion(win);
}

int mui_windows_animate(struct mui_server *server, double now_ms) {
    int active = 0;
    struct mui_window *win, *tmp;
    wl_list_for_each_safe(win, tmp, &server->windows, link) {
        if (win->closing) {
            /* @keyframes lp-window-close { to { transform: scale(0.96); opacity: 0 } } over motion.fast, ease-in, held until CLOSE. */
            float t = (float)((now_ms - win->close_anim.start_ms) / LP_MOTION_FAST_MS);
            if (t > 1) t = 1;
            if (t < 0) t = 0;
            float p = lp_cubic_bezier_eval(MUI_EASE_IN, t);
            float h = win->state == LP_WIN_SHADED ? LP_TITLE_HEIGHT : win->rect.h;
            apply_transform(win, 0, 0, 1 - 0.04f * p, 1 - 0.04f * p, win->rect.w / 2, h / 2, 1 - p);
            if (now_ms - win->close_anim.start_ms >= CLOSE_MS) {
                lp_wm_action a = { .type = LP_WM_CLOSE, .id = win->wm_id };
                lp_desktop_dispatch(&server->desktop, &a); /* on_change → sync destroys the window */
            } else {
                active = 1;
            }
            continue;
        }
        if (win->shade_anim.active) {
            float p = mui_tween_progress(&win->shade_anim, now_ms);
            if (!win->shade_anim.active) {
                finish_shade(win);
            } else {
                win->visual_h = win->shade_from_h + (win->shade_to_h - win->shade_from_h) * p;
                mui_window_apply_motion(win);
                active = 1;
            }
        }
    }
    return active;
}
