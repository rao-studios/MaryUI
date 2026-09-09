/* The desktop on the scene: wallpaper per output, the menu bar and its
 * clock, dropdown menus, Spotlight (spotlight.c), and the routing of pointer
 * and keyboard input to the window manager (drags, resizes, focus) and to
 * chromes. */
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

#include "maryui/components/lp_menu.h"
#include "maryui/components/lp_menu_bar.h"
#include "maryui/components/lp_window.h"
#include "maryui/lp_geometry.h"
#include "maryui/lp_tokens.h"
#include "maryui/lp_wallpaper.h"
#include "chrome.h"
#include "window.h"

#define DOUBLE_CLICK_MS 400

static void format_clock(char *out, size_t n) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char day[8], hm[16];
    strftime(day, sizeof day, "%a", &tm);
    int hour = tm.tm_hour % 12;
    if (hour == 0) hour = 12;
    snprintf(hm, sizeof hm, "%d:%02d %s", hour, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
    snprintf(out, n, "%s %s", day, hm);
}

/* MARK: - The dropdown */

struct menu_host { struct mui_server *server; };

static void paint_menu(lp_ctx *ctx, struct mui_chrome *chrome, void *data) {
    struct mui_server *server = data;
    lp_desktop *d = &server->desktop;
    if (d->open_menu < 0) return;
    lp_menu_result result;
    lp_menu(ctx, LP_RECT(LP_MENU_SHADOW_EXTENT, LP_MENU_SHADOW_EXTENT, 0, 0), &d->menus[d->open_menu], d->menu_active, &result);
    if (ctx->pass == LP_PASS_EVENT) {
        if (result.hovered != d->menu_active) { d->menu_active = result.hovered; ctx->dirty = 1; }
        if (result.selected >= 0) {
            lp_desktop_select_menu_entry(d, result.selected);
        }
    }
}

static void close_menu_chrome(struct mui_server *server) {
    if (!server->menu) return;
    if (server->pointer_chrome == server->menu) server->pointer_chrome = NULL;
    mui_chrome_finish(server->menu);
    free(server->menu);
    server->menu = NULL;
    server->menu_shown_index = -1;
    server->menu_anim.active = 0;
}

/* Shows (or hides) the dropdown for the desktop's open menu below its trigger. */
static void sync_menu_chrome(struct mui_server *server) {
    lp_desktop *d = &server->desktop;
    if (d->open_menu < 0) {
        close_menu_chrome(server);
        if (server->menubar) { mui_chrome_damage_all(server->menubar); mui_chrome_repaint(server->menubar, mui_now_ms()); }
        return;
    }
    /* size from the model */
    cairo_surface_t *tmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(tmp);
    lp_size size = lp_menu_measure(cr, &d->menus[d->open_menu]);
    cairo_destroy(cr);
    cairo_surface_destroy(tmp);
    int w = (int)size.w + 2 * LP_MENU_SHADOW_EXTENT, h = (int)size.h + 2 * LP_MENU_SHADOW_EXTENT;
    if (!server->menu) {
        server->menu = calloc(1, sizeof(*server->menu));
        mui_chrome_init(server->menu, server, server->layer_menus, w, h, paint_menu, server);
    } else {
        mui_chrome_resize(server->menu, w, h);
    }
    int x, y;
    if (d->open_menu == LP_DESKTOP_MENU_POPUP) {
        /* The context menu: at its anchor in the owning window, kept on the desktop. */
        struct mui_window *owner = mui_window_find(server, d->popup_window);
        int tx = 0, ty = 0;
        if (owner) wlr_scene_node_coords(&owner->tree->node, &tx, &ty);
        x = tx + (int)d->popup_x - LP_MENU_SHADOW_EXTENT;
        y = ty + (int)d->popup_y - LP_MENU_SHADOW_EXTENT;
        if (x + w - LP_MENU_SHADOW_EXTENT > server->desktop_width) x = server->desktop_width - w + LP_MENU_SHADOW_EXTENT;
        if (y + h - LP_MENU_SHADOW_EXTENT > server->desktop_height) y = server->desktop_height - h + LP_MENU_SHADOW_EXTENT;
        if (y < (int)LP_SIZE_MENUBAR_HEIGHT - LP_MENU_SHADOW_EXTENT) y = (int)LP_SIZE_MENUBAR_HEIGHT - LP_MENU_SHADOW_EXTENT;
    } else {
        /* anchor: the trigger's left, 2px below the bar (MenuBar.tsx anchorFor) */
        lp_menu_bar_model model = { .count = LP_DESKTOP_MENU_COUNT, .open_index = d->open_menu, .clock = server->clock_text };
        for (int i = 0; i < LP_DESKTOP_MENU_COUNT; i++) model.labels[i] = i == 0 ? "" : d->menus[i].label;
        lp_menu_bar_result bar;
        lp_ctx probe = { 0 };
        probe.settings = server->settings;
        cairo_surface_t *ms = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        cairo_t *mcr = cairo_create(ms);
        lp_ctx_begin(&probe, LP_PASS_DRAW, mcr, NULL, LP_RECT(0, 0, server->desktop_width, LP_SIZE_MENUBAR_HEIGHT), 0);
        cairo_push_group(mcr);
        lp_menu_bar(&probe, LP_RECT(0, 0, server->desktop_width, LP_SIZE_MENUBAR_HEIGHT), &model, &bar);
        cairo_pattern_destroy(cairo_pop_group(mcr));
        lp_ctx_end(&probe);
        cairo_destroy(mcr);
        cairo_surface_destroy(ms);
        lp_rect t = bar.triggers[d->open_menu];
        x = (int)t.x - LP_MENU_SHADOW_EXTENT;
        y = (int)LP_SIZE_MENUBAR_HEIGHT + 2 - LP_MENU_SHADOW_EXTENT;
        if (x + w - LP_MENU_SHADOW_EXTENT > server->desktop_width) x = server->desktop_width - w + LP_MENU_SHADOW_EXTENT;
    }
    wlr_scene_node_set_position(&server->menu->node->node, x, y);
    server->menu_x = x;
    server->menu_y = y;
    if (server->menu_shown_index != d->open_menu) {
        server->menu_shown_index = d->open_menu;
        if (!server->settings->reduced_motion) {
            /* Menu.module.css lp-menu-in: opacity 0→1, translateY(-4px) scaleY(.96) → none, motion.fast ease-out. */
            mui_tween_start(&server->menu_anim, mui_now_ms(), LP_MOTION_FAST_MS, LP_MOTION_EASE_OUT);
            wlr_scene_buffer_set_opacity(server->menu->node, 0.0f);
            mui_server_schedule_frame(server);
        }
    }
    mui_chrome_damage_all(server->menu);
    mui_chrome_repaint(server->menu, mui_now_ms());
    if (server->menubar) { mui_chrome_damage_all(server->menubar); mui_chrome_repaint(server->menubar, mui_now_ms()); }
}

/* MARK: - The menu bar */

static void paint_menubar(lp_ctx *ctx, struct mui_chrome *chrome, void *data) {
    struct mui_server *server = data;
    lp_desktop *d = &server->desktop;
    if (d->menus[1].label == NULL) lp_desktop_build_menus(d);
    lp_menu_bar_model model = { .count = LP_DESKTOP_MENU_COUNT, .open_index = d->open_menu < LP_DESKTOP_MENU_COUNT ? d->open_menu : -1, .clock = server->clock_text, .status = NULL };
    for (int i = 0; i < LP_DESKTOP_MENU_COUNT; i++) model.labels[i] = i == 0 ? "" : d->menus[i].label;
    lp_menu_bar_result result;
    lp_menu_bar(ctx, LP_RECT(0, 0, chrome->width, LP_SIZE_MENUBAR_HEIGHT), &model, &result);
    if (ctx->pass == LP_PASS_EVENT) {
        if (result.pressed >= 0) {
            lp_desktop_toggle_menu(d, result.pressed);
            ctx->dirty = 1;
            server->menu_index = d->open_menu;
            sync_menu_chrome(server);
        } else if (d->open_menu >= 0 && d->open_menu < LP_DESKTOP_MENU_COUNT && result.hovered >= 0 && result.hovered != d->open_menu) {
            lp_desktop_toggle_menu(d, result.hovered);
            ctx->dirty = 1;
            sync_menu_chrome(server);
        }
    }
}

static int clock_tick(void *data) {
    struct mui_server *server = data;
    char text[32];
    format_clock(text, sizeof text);
    if (strcmp(text, server->clock_text) != 0 && server->menubar) {
        snprintf(server->clock_text, sizeof server->clock_text, "%s", text);
        mui_chrome_damage(server->menubar, LP_RECT(server->menubar->width - 200, 0, 200, LP_SIZE_MENUBAR_HEIGHT));
        mui_chrome_repaint(server->menubar, mui_now_ms());
    }
    time_t now = time(NULL);
    wl_event_source_timer_update(server->clock_timer, (int)((60 - now % 60) * 1000 + 50));
    return 0;
}

/* MARK: - Desktop model hooks */

static void desktop_changed(lp_desktop *d, uint64_t changed) {
    struct mui_server *server = d->host;
    mui_windows_sync(server);
    if (d->open_menu < 0 && server->menu) sync_menu_chrome(server);
    /* The window list is part of Spotlight's results; deferred, since this may run inside its EVENT pass. */
    if (server->spotlight || d->spotlight.open) mui_spotlight_request_sync(server);
}

static void desktop_settings(lp_desktop *d) {
    struct mui_server *server = d->host;
    mui_desktop_settings_changed(server);
}

static int desktop_request_close(lp_desktop *d, const char *window_id) {
    struct mui_server *server = d->host;
    struct mui_window *win = mui_window_find(server, window_id);
    if (!win) return 0;
    mui_window_request_close(win); /* clients are asked; built-in windows animate out, then CLOSE */
    return 1;
}

/* An app asked for a repaint from inside a model callback (often an EVENT pass of its own chrome):
 * damage now, repaint on the next loop turn. The chrome in the middle of its pass repaints itself. */
static void repaint_idle(void *data) {
    struct mui_server *server = data;
    server->repaint_idle = NULL;
    struct mui_window *win;
    wl_list_for_each(win, &server->windows, link) {
        if (win->chrome.ctx.dirty || pixman_region32_not_empty(&win->chrome.damage)) mui_chrome_repaint(&win->chrome, mui_now_ms());
    }
}

static void desktop_app_dirty(lp_desktop *d, const char *window_id) {
    struct mui_server *server = d->host;
    struct mui_window *win = mui_window_find(server, window_id);
    if (!win) return;
    mui_chrome_damage(&win->chrome, win->body);
    if (server->repaint_idle) return;
    struct wl_event_loop *loop = wl_display_get_event_loop(server->display);
    server->repaint_idle = wl_event_loop_add_idle(loop, repaint_idle, server);
}

static void desktop_on_drag(lp_desktop *d, int begin) {
    struct mui_server *server = d->host;
    if (begin) mui_drag_begin(server); else mui_drag_finish(server);
}

static void desktop_spawn(lp_desktop *d, const char *command) {
    pid_t pid = fork();
    if (pid == 0) {
        /* A fresh session so the child survives the compositor's signals; inherits WAYLAND_DISPLAY. */
        setsid();
        signal(SIGPIPE, SIG_DFL);
        execlp("/bin/sh", "/bin/sh", "-c", command, (char *)NULL);
        _exit(127);
    }
    if (pid < 0) wlr_log(WLR_ERROR, "spawn %s: fork failed", command);
    else wlr_log(WLR_INFO, "spawned %s (pid %d)", command, pid);
}

static int reap(int signo, void *data) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    return 0;
}

void mui_desktop_init(struct mui_server *server) {
    format_clock(server->clock_text, sizeof server->clock_text);
    struct wl_event_loop *loop = wl_display_get_event_loop(server->display);
    server->clock_timer = wl_event_loop_add_timer(loop, clock_tick, server);
    time_t now = time(NULL);
    wl_event_source_timer_update(server->clock_timer, (int)((60 - now % 60) * 1000 + 50));
    wl_event_loop_add_signal(loop, SIGCHLD, reap, server);
    server->desktop.on_change = desktop_changed;
    server->desktop.on_settings = desktop_settings;
    server->desktop.spawn = desktop_spawn;
    server->desktop.request_close = desktop_request_close;
    server->desktop.on_app_dirty = desktop_app_dirty;
    server->desktop.on_drag = desktop_on_drag;
    mui_files_init(server);
    server->desktop.open_menu = -1;
    server->menu_index = -1;
    lp_desktop_build_menus(&server->desktop);
}

void mui_desktop_finish(struct mui_server *server) {
    if (server->clock_timer) wl_event_source_remove(server->clock_timer);
    server->clock_timer = NULL;
    if (server->repaint_idle) wl_event_source_remove(server->repaint_idle);
    server->repaint_idle = NULL;
    mui_drag_finish(server);
    mui_files_finish(server);
    close_menu_chrome(server);
    mui_spotlight_finish(server);
    struct mui_window *win, *tmp;
    wl_list_for_each_safe(win, tmp, &server->windows, link) mui_window_destroy(win);
    if (server->menubar) {
        mui_chrome_finish(server->menubar);
        free(server->menubar);
        server->menubar = NULL;
    }
}

void mui_desktop_settings_changed(struct mui_server *server) {
    struct mui_window *win;
    wl_list_for_each(win, &server->windows, link) { mui_chrome_damage_all(&win->chrome); mui_chrome_repaint(&win->chrome, mui_now_ms()); }
    if (server->menubar) { mui_chrome_damage_all(server->menubar); mui_chrome_repaint(server->menubar, mui_now_ms()); }
    if (server->menu) { mui_chrome_damage_all(server->menu); mui_chrome_repaint(server->menu, mui_now_ms()); }
    if (server->spotlight) { mui_chrome_damage_all(server->spotlight); mui_chrome_repaint(server->spotlight, mui_now_ms()); }
}

void mui_desktop_output_ready(struct mui_output *output) {
    struct mui_server *server = output->server;
    int w = output->width, h = output->height;
    if (w <= 0 || h <= 0) return;

    double t0 = mui_now_ms();
    cairo_surface_t *wp = lp_wallpaper_cached(w, h);
    cairo_surface_t *argb = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(argb);
    cairo_set_source_surface(cr, wp, 0, 0);
    cairo_paint(cr);
    lp_wallpaper_vignette(cr, w, h);
    cairo_destroy(cr);
    cairo_surface_destroy(wp);
    if (output->wallpaper_buffer) wlr_buffer_drop(&output->wallpaper_buffer->base);
    output->wallpaper_buffer = lp_cairo_buffer_from_surface(argb);
    cairo_surface_destroy(argb);
    if (!output->wallpaper) output->wallpaper = wlr_scene_buffer_create(server->layer_wallpaper, NULL);
    wlr_scene_buffer_set_buffer(output->wallpaper, &output->wallpaper_buffer->base);
    struct wlr_box box;
    wlr_output_layout_get_box(server->output_layout, output->wlr_output, &box);
    wlr_scene_node_set_position(&output->wallpaper->node, box.x, box.y);
    pixman_region32_t opaque;
    pixman_region32_init_rect(&opaque, 0, 0, w, h);
    wlr_scene_buffer_set_opaque_region(output->wallpaper, &opaque);
    pixman_region32_fini(&opaque);
    wlr_log(WLR_INFO, "wallpaper %dx%d ready in %.0f ms", w, h, mui_now_ms() - t0);

    /* The first output is the desktop: menu bar across it, bounds below the bar. */
    if (server->menubar && server->desktop_width && box.x != 0) return;
    server->desktop_width = w;
    server->desktop_height = h;
    if (!server->menubar) {
        server->menubar = calloc(1, sizeof(*server->menubar));
        mui_chrome_init(server->menubar, server, server->layer_menubar, w, (int)LP_SIZE_MENUBAR_HEIGHT + LP_MENU_BAR_SHADOW_EXTENT, paint_menubar, server);
        wlr_scene_node_set_position(&server->menubar->node->node, box.x, box.y);
    } else {
        mui_chrome_resize(server->menubar, w, (int)LP_SIZE_MENUBAR_HEIGHT + LP_MENU_BAR_SHADOW_EXTENT);
    }
    mui_chrome_repaint(server->menubar, mui_now_ms());
    lp_wm_action a = { .type = LP_WM_SET_BOUNDS, .bounds = LP_RECT(box.x, box.y + LP_SIZE_MENUBAR_HEIGHT, w, h - LP_SIZE_MENUBAR_HEIGHT) };
    lp_desktop_dispatch(&server->desktop, &a);
    if (server->desktop.wm.count == 0) {
        lp_desktop_open_app(&server->desktop, "finder");
        lp_desktop_open_app(&server->desktop, "gallery");
    }
}

/* MARK: - Pointer routing */

static int cmp_z_desc(const void *a, const void *b) {
    const struct mui_window *const *wa = a, *const *wb = b;
    return (*wb)->z - (*wa)->z;
}

/* Window shadows and the dropdown's shadow are not hit (like CSS box-shadow):
 * a point in a margin falls through to whatever lies beneath. */
void mui_desktop_hit(struct mui_server *server, double lx, double ly, struct mui_hit *hit) {
    memset(hit, 0, sizeof *hit);
    double nx, ny;
    /* A client surface on top (popups can reach outside their window) is what it seems. */
    struct wlr_scene_node *top = wlr_scene_node_at(&server->scene->tree.node, lx, ly, &nx, &ny);
    if (top && top->type == WLR_SCENE_NODE_BUFFER) {
        struct wlr_scene_surface *ss = wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(top));
        if (ss) {
            hit->surface = ss->surface;
            hit->win = mui_xdg_window_of_surface(server, ss->surface);
            hit->sx = nx;
            hit->sy = ny;
            return;
        }
    }
    /* Spotlight's panel (its shadow falls through). */
    if (mui_spotlight_hit(server, lx, ly, hit)) return;
    /* The dropdown, inside its shadow margin. */
    if (server->menu) {
        int mx, my;
        wlr_scene_node_coords(&server->menu->node->node, &mx, &my);
        double sx = lx - mx, sy = ly - my;
        if (sx >= LP_MENU_SHADOW_EXTENT && sy >= LP_MENU_SHADOW_EXTENT &&
            sx < server->menu->width - LP_MENU_SHADOW_EXTENT && sy < server->menu->height - LP_MENU_SHADOW_EXTENT) {
            hit->chrome = server->menu;
            hit->sx = sx;
            hit->sy = sy;
            return;
        }
    }
    /* The menu bar, above its shadow. */
    if (server->menubar) {
        int mx, my;
        wlr_scene_node_coords(&server->menubar->node->node, &mx, &my);
        double sx = lx - mx, sy = ly - my;
        if (sx >= 0 && sy >= 0 && sx < server->menubar->width && sy < LP_SIZE_MENUBAR_HEIGHT) {
            hit->chrome = server->menubar;
            hit->sx = sx;
            hit->sy = sy;
            return;
        }
    }
    /* Windows, front to back, by their rect plus the resize grip that pokes out of it. */
    struct mui_window *sorted[LP_WM_MAX_WINDOWS];
    int n = 0;
    struct mui_window *win;
    wl_list_for_each(win, &server->windows, link) if (n < LP_WM_MAX_WINDOWS && !win->closing) sorted[n++] = win;
    qsort(sorted, (size_t)n, sizeof sorted[0], cmp_z_desc);
    float grip = LP_SIZE_RESIZE_GRIP / 2;
    for (int i = 0; i < n; i++) {
        win = sorted[i];
        lp_rect r = win->rect;
        if (win->state == LP_WIN_SHADED) r.h = LP_TITLE_HEIGHT;
        if (win->visual_h > 0) r.h = win->visual_h;
        if (lx < r.x - grip || ly < r.y - grip || lx >= r.x + r.w + grip || ly >= r.y + r.h + grip) continue;
        int tx, ty;
        wlr_scene_node_coords(&win->tree->node, &tx, &ty);
        struct wlr_scene_node *node = wlr_scene_node_at(&win->tree->node, lx - tx, ly - ty, &nx, &ny);
        hit->win = win;
        if (node && node->type == WLR_SCENE_NODE_BUFFER) {
            struct wlr_scene_surface *ss = wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node));
            if (ss) {
                hit->surface = ss->surface;
                hit->sx = nx;
                hit->sy = ny;
                return;
            }
        }
        hit->chrome = &win->chrome;
        hit->sx = lx - tx;
        hit->sy = ly - ty;
        return;
    }
}

static enum mui_cursor_shape to_cursor(enum lp_cursor_shape s) {
    switch (s) {
    case LP_CURSOR_NS: return MUI_CURSOR_NS;
    case LP_CURSOR_EW: return MUI_CURSOR_EW;
    case LP_CURSOR_NWSE: return MUI_CURSOR_NWSE;
    case LP_CURSOR_NESW: return MUI_CURSOR_NESW;
    case LP_CURSOR_TEXT: return MUI_CURSOR_TEXT;
    default: return MUI_CURSOR_ARROW;
    }
}

static uint32_t seat_mods(struct mui_server *server) {
    struct wlr_keyboard *kb = wlr_seat_get_keyboard(server->seat);
    return kb ? wlr_keyboard_get_modifiers(kb) : 0; /* WLR_MODIFIER_* are LP_MOD_* */
}

int mui_desktop_pointer_event(struct mui_server *server, double lx, double ly, int buttons, int pressed, int released, uint32_t time_msec) {
    lp_desktop *d = &server->desktop;
    double now = mui_now_ms();

    /* A drag session owns the pointer: the ghost follows, app windows get HOVER / DROP, a right press cancels. */
    if (server->grab.kind == MUI_GRAB_DRAG) {
        if (released & LP_BUTTON_LEFT) mui_drag_drop(server, lx, ly, time_msec);
        else if (pressed & LP_BUTTON_RIGHT) mui_drag_cancel(server);
        else mui_drag_motion(server, lx, ly, time_msec);
        return 1;
    }
    /* Grabs first: they own the pointer until release. */
    if (server->grab.kind != MUI_GRAB_NONE && server->grab.win) {
        struct mui_window *win = server->grab.win;
        float dx = (float)(lx - server->grab.sx), dy = (float)(ly - server->grab.sy);
        if (server->grab.kind == MUI_GRAB_MOVE) {
            /* Window.tsx onMove: the clamped delta goes to the motion target, which translates the chrome each frame. */
            lp_rect next = server->grab.start;
            next.x += dx;
            next.y += dy;
            next = lp_clamp_to_bounds(next, d->wm.bounds, 0, LP_TITLE_HEIGHT);
            lp_window_motion_move_drag(&win->motion, next.x - server->grab.start.x, next.y - server->grab.start.y, now, (float)lx, (float)ly);
            lp_motion_engine_wake(&server->engine, now);
            if (released & LP_BUTTON_LEFT) {
                /* onEnd: commit the layout, zero the delta, then tell the store — no flicker on release. */
                server->grab.kind = MUI_GRAB_NONE;
                server->grab.win = NULL;
                mui_window_place(win, next);
                lp_window_motion_end_drag(&win->motion);
                mui_window_apply_motion(win);
                lp_wm_action a = { .type = LP_WM_MOVE, .id = win->wm_id, .x = next.x, .y = next.y };
                lp_desktop_dispatch(d, &a);
                mui_windows_sync(server);
            }
        } else {
            int i = lp_wm_find(&d->wm, win->wm_id);
            lp_size min = i >= 0 ? d->wm.windows[i].min_size : LP_DEFAULT_MIN_SIZE;
            lp_rect next = lp_resize_from_handle(server->grab.start, (enum lp_resize_handle)server->grab.handle, dx, dy, min, &d->wm.bounds);
            if ((int)next.w != (int)win->rect.w || (int)next.h != (int)win->rect.h) {
                mui_chrome_resize(&win->chrome, (int)next.w + win->margin_l + win->margin_r, (int)next.h + win->margin_t + win->margin_b);
            }
            mui_window_place(win, next);
            mui_chrome_repaint(&win->chrome, now);
            lp_motion_engine_wake(&server->engine, now); /* the sheen follows the new width */
            if (released & LP_BUTTON_LEFT) {
                server->grab.kind = MUI_GRAB_NONE;
                server->grab.win = NULL;
                lp_window_motion_end_resize(&win->motion);
                lp_wm_action a = { .type = LP_WM_RESIZE, .id = win->wm_id, .rect = next };
                lp_desktop_dispatch(d, &a);
                mui_windows_sync(server);
            }
        }
        return 1;
    }

    struct mui_hit hit;
    mui_desktop_hit(server, lx, ly, &hit);
    struct mui_chrome *chrome = hit.chrome;
    struct mui_window *win = hit.win;
    double sx = hit.sx, sy = hit.sy;

    /* A press outside the open menu (and off the menu bar) closes it. */
    if ((pressed & (LP_BUTTON_LEFT | LP_BUTTON_RIGHT)) && d->open_menu >= 0 && chrome != server->menu && chrome != server->menubar) {
        lp_desktop_close_menu(d);
        sync_menu_chrome(server);
    }
    /* A press outside Spotlight's panel closes it; the press carries on to what is beneath. */
    if ((pressed & LP_BUTTON_LEFT) && d->spotlight.open && chrome != server->spotlight) {
        lp_spotlight_close(&d->spotlight);
        mui_spotlight_sync(server);
    }

    if (server->pointer_chrome && server->pointer_chrome != chrome) {
        mui_chrome_pointer_leave(server->pointer_chrome, now);
        server->pointer_chrome = NULL;
    }
    if (!chrome) {
        if (hit.surface) {
            /* A client surface: pointer focus and motion; a press focuses its window. */
            if ((pressed & (LP_BUTTON_LEFT | LP_BUTTON_RIGHT)) && win && !win->focused) {
                lp_wm_action a = { .type = LP_WM_FOCUS, .id = win->wm_id };
                lp_desktop_dispatch(d, &a);
            }
            if (server->seat->pointer_state.focused_surface != hit.surface) {
                wlr_seat_pointer_notify_enter(server->seat, hit.surface, sx, sy);
                mui_cursor_set_shape(server, MUI_CURSOR_ARROW);
            }
            wlr_seat_pointer_notify_motion(server->seat, time_msec, sx, sy);
            return 0; /* buttons and axes go to the seat */
        }
        if (server->seat->pointer_state.focused_surface) wlr_seat_pointer_clear_focus(server->seat);
        mui_cursor_set_shape(server, MUI_CURSOR_ARROW);
        return 0;
    }
    if (server->seat->pointer_state.focused_surface) {
        wlr_seat_pointer_clear_focus(server->seat);
        mui_cursor_set_shape(server, server->cursor_shape == MUI_CURSOR_COUNT ? MUI_CURSOR_ARROW : server->cursor_shape);
    }
    server->pointer_chrome = chrome;

    /* Windows: resize handles and focus before the chrome sees the event. */
    if (win) {
        int i = lp_wm_find(&d->wm, win->wm_id);
        const lp_window_record *rec = i >= 0 ? &d->wm.windows[i] : NULL;
        lp_rect local = LP_RECT(win->margin_l, win->margin_t, win->rect.w, win->state == LP_WIN_SHADED ? LP_TITLE_HEIGHT : win->rect.h);
        if (rec && rec->resizable && rec->state == LP_WIN_NORMAL) {
            lp_rect handles[LP_HANDLE_COUNT];
            lp_window_handles(local, handles);
            for (int h = 0; h < LP_HANDLE_COUNT; h++) {
                if (lp_rect_contains(handles[h], (float)sx, (float)sy)) {
                    mui_cursor_set_shape(server, to_cursor(lp_window_handle_cursor((enum lp_resize_handle)h)));
                    if (pressed & LP_BUTTON_LEFT) {
                        if (!win->focused) { lp_wm_action a = { .type = LP_WM_FOCUS, .id = win->wm_id }; lp_desktop_dispatch(d, &a); }
                        mui_window_begin_resize(win, h, lx, ly);
                    }
                    return 1;
                }
            }
        }
        /* Outside the window itself (in the shadow margin): nothing. */
        if (!lp_rect_contains(local, (float)sx, (float)sy)) {
            mui_cursor_set_shape(server, MUI_CURSOR_ARROW);
            return 0;
        }
        if ((pressed & (LP_BUTTON_LEFT | LP_BUTTON_RIGHT)) && !win->focused) {
            lp_wm_action a = { .type = LP_WM_FOCUS, .id = win->wm_id };
            lp_desktop_dispatch(d, &a);
        }
    }

    lp_input in = { 0 };
    in.mx = (float)sx;
    in.my = (float)sy;
    in.buttons = buttons;
    in.pressed = pressed;
    in.released = released;
    in.mods = seat_mods(server);
    in.time_ms = time_msec;
    if (pressed & LP_BUTTON_LEFT) {
        if (time_msec - server->last_click_ms < DOUBLE_CLICK_MS && fabs(lx - server->last_click_x) < 4 && fabs(ly - server->last_click_y) < 4) {
            in.double_click = 1;
            server->last_click_ms = 0;
        } else {
            server->last_click_ms = time_msec;
            server->last_click_x = lx;
            server->last_click_y = ly;
        }
    }
    enum lp_cursor_shape shape = mui_chrome_event(chrome, &in, now);
    mui_cursor_set_shape(server, to_cursor(shape));
    /* The pass began a drag: the chrome it started in loses its press (the release never reaches it) and its hover. */
    if (server->grab.kind == MUI_GRAB_DRAG && !server->drag_source) {
        server->drag_source = chrome;
        chrome->ctx.active = 0;
        mui_chrome_pointer_leave(chrome, now);
        server->pointer_chrome = NULL;
        mui_cursor_set_shape(server, MUI_CURSOR_ARROW);
        return 1;
    }
    /* The pass may have opened a context menu (or closed the dropdown): bring the chrome in step. */
    if ((d->open_menu >= 0) != (server->menu != NULL) || (server->menu && d->open_menu != server->menu_shown_index)) sync_menu_chrome(server);
    return 1;
}

int mui_desktop_key(struct mui_server *server, uint32_t keysym, uint32_t modifiers, const char *utf8, int pressed) {
    lp_desktop *d = &server->desktop;
    if (server->grab.kind == MUI_GRAB_DRAG) {
        if (pressed && keysym == XKB_KEY_Escape) mui_drag_cancel(server);
        return 1; /* the modifiers still reach the seat: Alt toggles the copy badge */
    }
    int had_menu = d->open_menu;
    int was_open = d->spotlight.open;
    server->key_to_chrome = 0;
    int handled = pressed ? lp_desktop_key(d, keysym, modifiers) : 0;
    if (was_open || d->spotlight.open) {
        /* Spotlight owns the keyboard while it is up: Esc/↑/↓/Enter were the desktop's, the rest edits the
         * query in the bar. Presses never reach clients; releases pass (a release without its press is inert). */
        if (!pressed) { if (!d->spotlight.open) mui_spotlight_sync(server); return 0; }
        if (!handled && d->spotlight.open && server->spotlight) {
            mui_chrome_key(server->spotlight, keysym, modifiers, utf8, pressed, mui_now_ms());
            server->key_to_chrome = 1;
        }
        mui_spotlight_sync(server);
        if (had_menu >= 0 || d->open_menu >= 0) sync_menu_chrome(server);
        return 1;
    }
    if (handled && (had_menu >= 0 || d->open_menu >= 0)) {
        sync_menu_chrome(server);
        return 1;
    }
    if (handled) return 1;
    /* The focused built-in window gets the key (text fields, sliders). */
    const lp_window_record *focused = lp_wm_focused(&d->wm);
    if (!focused) return 0;
    struct mui_window *win = mui_window_find(server, focused->id);
    if (!win || win->kind != MUI_WINDOW_APP) return 0;
    mui_chrome_key(&win->chrome, keysym, modifiers, utf8, pressed, mui_now_ms());
    server->key_to_chrome = pressed;
    return 1;
}

int mui_desktop_scroll(struct mui_server *server, double lx, double ly, float dx, float dy) {
    struct mui_hit hit;
    mui_desktop_hit(server, lx, ly, &hit);
    if (!hit.chrome) return 0;
    hit.chrome->ctx.in.mx = (float)hit.sx;
    hit.chrome->ctx.in.my = (float)hit.sy;
    mui_chrome_scroll(hit.chrome, dx, dy, mui_now_ms());
    return 1;
}

/* MARK: - Animations */

static int animate_menu(struct mui_server *server, double now_ms) {
    if (!server->menu || !server->menu_anim.active) return 0;
    struct wlr_scene_buffer *node = server->menu->node;
    float p = mui_tween_progress(&server->menu_anim, now_ms);
    if (!server->menu_anim.active) {
        wlr_scene_buffer_set_opacity(node, 1.0f);
        wlr_scene_buffer_set_dest_size(node, 0, 0);
        wlr_scene_node_set_position(&node->node, server->menu_x, server->menu_y);
        mui_server_damage_all(server);
        return 0;
    }
    wlr_scene_buffer_set_opacity(node, p);
    wlr_scene_buffer_set_dest_size(node, server->menu->width, (int)lroundf(server->menu->height * (0.96f + 0.04f * p)));
    wlr_scene_node_set_position(&node->node, server->menu_x, server->menu_y - (int)lroundf(4 * (1 - p)));
    return 1;
}

int mui_desktop_animate(struct mui_server *server, double now_ms) {
    int active = animate_menu(server, now_ms);
    active |= mui_spotlight_animate(server, now_ms);
    return active;
}

void mui_desktop_ambient_tick(struct mui_server *server) {
    struct mui_window *win;
    wl_list_for_each(win, &server->windows, link) {
        if (mui_chrome_wants_frame(&win->chrome)) { mui_chrome_damage(&win->chrome, mui_chrome_ambient_rect(&win->chrome)); mui_chrome_repaint(&win->chrome, mui_now_ms()); }
    }
}
