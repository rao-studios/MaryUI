/* Drag and drop on the scene: while the desktop's drag session is active the
 * pointer is grabbed, a ghost chrome (lp_drag_ghost) follows it in the top
 * layer, and the app window under the pointer receives lp_input.drag HOVER /
 * LEAVE / DROP events; nothing is focused or raised by a hover. Esc cancels. */
#include <math.h>
#include <stdlib.h>

#include "maryui/lp_drag.h"
#include "chrome.h"
#include "window.h"

static bool reject(struct wlr_scene_buffer *buffer, double *sx, double *sy) { return false; }

static void paint_ghost(lp_ctx *ctx, struct mui_chrome *chrome, void *data) {
    struct mui_server *server = data;
    lp_drag_ghost(ctx, LP_RECT(0, 0, chrome->width, chrome->height), &server->desktop.drag);
}

static uint32_t mods(struct mui_server *server) {
    struct wlr_keyboard *kb = wlr_seat_get_keyboard(server->seat);
    return kb ? wlr_keyboard_get_modifiers(kb) : 0;
}

static void place_ghost(struct mui_server *server, double lx, double ly) {
    wlr_scene_node_set_position(&server->drag_ghost->node->node, (int)lx - LP_DRAG_HOTSPOT_X, (int)ly - LP_DRAG_HOTSPOT_Y);
}

static void repaint_ghost(struct mui_server *server) {
    mui_chrome_damage_all(server->drag_ghost);
    mui_chrome_repaint(server->drag_ghost, mui_now_ms());
}

void mui_drag_begin(struct mui_server *server) {
    if (server->drag_ghost) return;
    server->drag_ghost = calloc(1, sizeof(*server->drag_ghost));
    mui_chrome_init(server->drag_ghost, server, server->layer_spotlight, LP_DRAG_GHOST_W, LP_DRAG_GHOST_H, paint_ghost, server);
    server->drag_ghost->node->point_accepts_input = reject;
    server->desktop.drag.copy = (mods(server) & LP_MOD_ALT) != 0;
    server->drag_ghost_copy = server->desktop.drag.copy;
    place_ghost(server, server->cursor->x, server->cursor->y);
    repaint_ghost(server);
    server->grab.kind = MUI_GRAB_DRAG;
    server->grab.win = NULL;
    server->drag_source = NULL;
    server->drag_target = NULL;
    mui_cursor_set_shape(server, MUI_CURSOR_ARROW);
}

void mui_drag_finish(struct mui_server *server) {
    if (server->drag_ghost) {
        mui_chrome_finish(server->drag_ghost);
        free(server->drag_ghost);
        server->drag_ghost = NULL;
    }
    if (server->grab.kind == MUI_GRAB_DRAG) server->grab.kind = MUI_GRAB_NONE;
    server->drag_source = NULL;
    server->drag_target = NULL;
    mui_server_damage_all(server);
}

/* An app window's own chrome under the pointer (clients and the desktop's own chromes take no drops). */
static struct mui_chrome *target_at(struct mui_server *server, double lx, double ly, double *sx, double *sy) {
    struct mui_hit hit;
    mui_desktop_hit(server, lx, ly, &hit);
    if (!hit.chrome || !hit.win || hit.win->kind != MUI_WINDOW_APP || hit.chrome != &hit.win->chrome || hit.win->closing) return NULL;
    *sx = hit.sx;
    *sy = hit.sy;
    return hit.chrome;
}

static void send(struct mui_server *server, struct mui_chrome *chrome, double sx, double sy, int buttons, int kind, uint32_t time_msec) {
    lp_input in = { 0 };
    in.mx = (float)sx;
    in.my = (float)sy;
    in.buttons = buttons;
    in.mods = mods(server);
    in.drag = kind;
    in.time_ms = time_msec;
    mui_chrome_event(chrome, &in, mui_now_ms());
}

void mui_drag_motion(struct mui_server *server, double lx, double ly, uint32_t time_msec) {
    if (!server->drag_ghost) return;
    place_ghost(server, lx, ly);
    double sx = 0, sy = 0;
    struct mui_chrome *target = target_at(server, lx, ly, &sx, &sy);
    if (target != server->drag_target && server->drag_target) send(server, server->drag_target, NAN, NAN, 0, LP_DRAG_LEAVE, time_msec);
    server->drag_target = target;
    if (!target) server->desktop.drag.copy = (mods(server) & LP_MOD_ALT) != 0;
    else send(server, target, sx, sy, LP_BUTTON_LEFT, LP_DRAG_HOVER, time_msec);
    server->pointer_chrome = target;
    if (server->desktop.drag.copy != server->drag_ghost_copy) {
        server->drag_ghost_copy = server->desktop.drag.copy;
        repaint_ghost(server);
    }
    mui_cursor_set_shape(server, MUI_CURSOR_ARROW);
}

void mui_drag_drop(struct mui_server *server, double lx, double ly, uint32_t time_msec) {
    if (!server->drag_ghost) return;
    double sx = 0, sy = 0;
    struct mui_chrome *target = target_at(server, lx, ly, &sx, &sy);
    if (target != server->drag_target && server->drag_target) send(server, server->drag_target, NAN, NAN, 0, LP_DRAG_LEAVE, time_msec);
    server->drag_target = target;
    if (target) send(server, target, sx, sy, 0, LP_DRAG_DROP, time_msec);
    lp_desktop_drag_end(&server->desktop); /* on_drag(0): the ghost goes, the grab ends */
    server->pointer_chrome = target;
}

void mui_drag_cancel(struct mui_server *server) {
    if (!server->drag_ghost) return;
    if (server->drag_target) send(server, server->drag_target, NAN, NAN, 0, LP_DRAG_LEAVE, 0);
    server->drag_target = NULL;
    lp_desktop_drag_end(&server->desktop);
}

void mui_drag_mods_changed(struct mui_server *server) {
    if (!server->drag_ghost) return;
    mui_drag_motion(server, server->cursor->x, server->cursor->y, 0);
}

void mui_drag_window_gone(struct mui_server *server, struct mui_chrome *chrome) {
    if (server->drag_source == chrome) server->drag_source = NULL;
    if (server->drag_target == chrome) server->drag_target = NULL;
}
