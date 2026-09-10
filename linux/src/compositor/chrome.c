#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "chrome.h"
#include "maryui/lp_tokens.h"

double mui_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

void mui_chrome_init(struct mui_chrome *chrome, struct mui_server *server, struct wlr_scene_tree *parent,
                     int width, int height, mui_chrome_paint_fn paint, void *data) {
    memset(chrome, 0, sizeof *chrome);
    chrome->server = server;
    chrome->width = width;
    chrome->height = height;
    chrome->paint = paint;
    chrome->data = data;
    chrome->current = -1;
    pixman_region32_init(&chrome->damage);
    pixman_region32_init(&chrome->opaque_region);
    for (int i = 0; i < MUI_CHROME_BUFFERS; i++) pixman_region32_init(&chrome->stale[i]);
    chrome->node = wlr_scene_buffer_create(parent, NULL);
    chrome->node->node.data = chrome;
    chrome->ctx.settings = server->settings;
    chrome->ctx.active_window = 1;
    chrome->ctx.sheen_x = 0.5f;
    /* A window that never moves still has corners: start them at rest. */
    chrome->ctx.corners = lp_rest_corners(LP_RADIUS_WINDOW);
    chrome->ctx.dirty = 1;
    mui_chrome_damage_all(chrome);
}

void mui_chrome_finish(struct mui_chrome *chrome) {
    for (int i = 0; i < MUI_CHROME_BUFFERS; i++) {
        if (chrome->buffers[i]) wlr_buffer_drop(&chrome->buffers[i]->base);
        chrome->buffers[i] = NULL;
    }
    if (chrome->node) wlr_scene_node_destroy(&chrome->node->node);
    chrome->node = NULL;
    pixman_region32_fini(&chrome->damage);
    pixman_region32_fini(&chrome->opaque_region);
    for (int i = 0; i < MUI_CHROME_BUFFERS; i++) pixman_region32_fini(&chrome->stale[i]);
}

void mui_chrome_resize(struct mui_chrome *chrome, int width, int height) {
    if (width == chrome->width && height == chrome->height) return;
    chrome->width = width;
    chrome->height = height;
    for (int i = 0; i < MUI_CHROME_BUFFERS; i++) {
        if (chrome->buffers[i]) wlr_buffer_drop(&chrome->buffers[i]->base);
        chrome->buffers[i] = NULL;
        pixman_region32_clear(&chrome->stale[i]);
    }
    chrome->current = -1;
    chrome->ctx.dirty = 1;
    mui_chrome_damage_all(chrome);
}

void mui_chrome_damage(struct mui_chrome *chrome, lp_rect r) {
    int x0 = (int)floorf(r.x), y0 = (int)floorf(r.y), x1 = (int)ceilf(r.x + r.w), y1 = (int)ceilf(r.y + r.h);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > chrome->width) x1 = chrome->width;
    if (y1 > chrome->height) y1 = chrome->height;
    if (x1 <= x0 || y1 <= y0) return;
    pixman_region32_union_rect(&chrome->damage, &chrome->damage, x0, y0, x1 - x0, y1 - y0);
    chrome->ctx.dirty = 1;
}

void mui_chrome_damage_all(struct mui_chrome *chrome) {
    pixman_region32_union_rect(&chrome->damage, &chrome->damage, 0, 0, chrome->width, chrome->height);
    chrome->ctx.dirty = 1;
}

/* A buffer nobody but us holds: the scene locks the one it shows (and the
 * renderer may keep another one briefly), so three rotate comfortably. */
static int free_buffer(struct mui_chrome *chrome) {
    for (int i = 0; i < MUI_CHROME_BUFFERS; i++) {
        if (i == chrome->current) continue;
        struct lp_cairo_buffer *b = chrome->buffers[i];
        if (!b) {
            chrome->buffers[i] = lp_cairo_buffer_create(chrome->width, chrome->height);
            /* Brand new: it holds nothing that matches the screen. */
            pixman_region32_union_rect(&chrome->stale[i], &chrome->stale[i], 0, 0, chrome->width, chrome->height);
            return chrome->buffers[i] ? i : -1;
        }
        if (b->base.n_locks == 0) return i;
    }
    /* Everything is locked: allocate a fresh one in the first non-current slot and drop the old. */
    int i = chrome->current == 0 ? 1 : 0;
    wlr_buffer_drop(&chrome->buffers[i]->base);
    chrome->buffers[i] = lp_cairo_buffer_create(chrome->width, chrome->height);
    pixman_region32_union_rect(&chrome->stale[i], &chrome->stale[i], 0, 0, chrome->width, chrome->height);
    return chrome->buffers[i] ? i : -1;
}

void mui_chrome_repaint(struct mui_chrome *chrome, double now_ms) {
    if (!chrome->ctx.dirty && !pixman_region32_not_empty(&chrome->damage)) return;
    if (chrome->width <= 0 || chrome->height <= 0) return;
    /* Re-entered from inside a pass (an app dispatched an action, and the host synced its windows):
     * the damage stays pending and the pass that is running repaints when it ends. */
    if (chrome->painting) return;
    chrome->painting = 1;
    int next = free_buffer(chrome);
    if (next < 0) { chrome->painting = 0; return; }  /* or this chrome never paints again */
    struct lp_cairo_buffer *target = chrome->buffers[next];
    int full = chrome->current < 0 || !chrome->buffers[chrome->current];
    if (full) pixman_region32_union_rect(&chrome->damage, &chrome->damage, 0, 0, chrome->width, chrome->height);

    cairo_t *cr = cairo_create(target->surface);
    if (!full) {
        /* Bring this buffer up to what is on screen — but only where it is
         * behind, and not where we are about to repaint anyway. Copying the
         * whole surface here cost megabytes a frame. */
        pixman_region32_t restore;
        pixman_region32_init(&restore);
        pixman_region32_subtract(&restore, &chrome->stale[next], &chrome->damage);
        if (pixman_region32_not_empty(&restore)) {
            int rn;
            pixman_box32_t *rb = pixman_region32_rectangles(&restore, &rn);
            cairo_save(cr);
            for (int i = 0; i < rn; i++) cairo_rectangle(cr, rb[i].x1, rb[i].y1, rb[i].x2 - rb[i].x1, rb[i].y2 - rb[i].y1);
            cairo_clip(cr);
            cairo_set_source_surface(cr, chrome->buffers[chrome->current]->surface, 0, 0);
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_paint(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            cairo_restore(cr);
        }
        pixman_region32_fini(&restore);
    }
    int nrects;
    pixman_box32_t *boxes = pixman_region32_rectangles(&chrome->damage, &nrects);
    for (int i = 0; i < nrects; i++) cairo_rectangle(cr, boxes[i].x1, boxes[i].y1, boxes[i].x2 - boxes[i].x1, boxes[i].y2 - boxes[i].y1);
    cairo_clip(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    lp_ctx_begin(&chrome->ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, chrome->width, chrome->height), now_ms);
    chrome->ctx.in.mx = chrome->pointer_inside ? chrome->ctx.in.mx : NAN;
    chrome->paint(&chrome->ctx, chrome, chrome->data);
    lp_ctx_end(&chrome->ctx);
    cairo_destroy(cr);
    cairo_surface_flush(target->surface);

    wlr_scene_buffer_set_buffer_with_damage(chrome->node, &target->base, &chrome->damage);
    mui_chrome_apply_opaque(chrome, 1);
    /* Ambient state: a request in this pass wins; a repaint that covered the old
     * rect without a request means the animation is gone; a strip repaint elsewhere keeps it. */
    if (chrome->ctx.wants_frame) {
        chrome->ambient = 1;
        chrome->ambient_rect = chrome->ctx.wants_frame_rect;
    } else if (chrome->ambient) {
        lp_rect a = chrome->ambient_rect;
        pixman_box32_t box = { (int)floorf(a.x), (int)floorf(a.y), (int)ceilf(a.x + a.w), (int)ceilf(a.y + a.h) };
        if (pixman_region32_contains_rectangle(&chrome->damage, &box) == PIXMAN_REGION_IN) chrome->ambient = 0;
    }
    /* This buffer now matches the screen; every other one is behind by what we
     * just painted. */
    for (int i = 0; i < MUI_CHROME_BUFFERS; i++) {
        if (i == next) pixman_region32_clear(&chrome->stale[i]);
        else pixman_region32_union(&chrome->stale[i], &chrome->stale[i], &chrome->damage);
    }
    chrome->current = next;
    pixman_region32_clear(&chrome->damage);
    chrome->ctx.dirty = 0;
    chrome->painting = 0;
    /* The scene has new pixels: ask for a frame. Without this the picture waits for whatever
     * else wakes the output next (a pointer motion), because output.c paces frames itself. */
    mui_server_schedule_frame(chrome->server);
}

enum lp_cursor_shape mui_chrome_event(struct mui_chrome *chrome, const lp_input *in, double now_ms) {
    lp_input copy = *in;
    lp_ctx_begin(&chrome->ctx, LP_PASS_EVENT, NULL, &copy, LP_RECT(0, 0, chrome->width, chrome->height), now_ms);
    if (!in->keysym && !in->scroll_x && !in->scroll_y) chrome->pointer_inside = !isnan(in->mx);
    chrome->painting = 1;
    chrome->paint(&chrome->ctx, chrome, chrome->data);
    chrome->painting = 0;
    enum lp_cursor_shape cursor = chrome->ctx.cursor;
    lp_ctx_end(&chrome->ctx);
    /* Keep the pointer position for the DRAW pass (hover looks). */
    chrome->ctx.in.mx = in->mx;
    chrome->ctx.in.my = in->my;
    chrome->ctx.in.buttons = in->buttons;
    chrome->ctx.in.mods = in->mods;
    if (chrome->ctx.dirty) mui_chrome_damage_all(chrome);
    else if (chrome->ctx.has_damage) mui_chrome_damage(chrome, chrome->ctx.damage); /* lp_damage: a marquee, a hover ring */
    /* Damage may also have come from a sync the pass triggered; repaint() returns at once when there is none. */
    mui_chrome_repaint(chrome, now_ms);
    return cursor;
}

void mui_chrome_key(struct mui_chrome *chrome, uint32_t keysym, uint32_t mods, const char *utf8, int pressed, double now_ms) {
    lp_input in = { 0 };
    in.mx = chrome->pointer_inside ? chrome->ctx.in.mx : NAN;
    in.my = chrome->ctx.in.my;
    in.buttons = chrome->ctx.in.buttons;
    in.keysym = keysym;
    in.mods = mods;
    in.key_pressed = pressed;
    if (utf8) snprintf(in.utf8, sizeof in.utf8, "%s", utf8);
    mui_chrome_event(chrome, &in, now_ms);
}

void mui_chrome_scroll(struct mui_chrome *chrome, float dx, float dy, double now_ms) {
    lp_input in = { 0 };
    in.mx = chrome->ctx.in.mx;
    in.my = chrome->ctx.in.my;
    in.buttons = chrome->ctx.in.buttons;
    in.scroll_x = dx;
    in.scroll_y = dy;
    mui_chrome_event(chrome, &in, now_ms);
}

int mui_chrome_wants_frame(const struct mui_chrome *chrome) { return chrome->ambient; }
lp_rect mui_chrome_ambient_rect(const struct mui_chrome *chrome) { return chrome->ambient_rect; }

void mui_chrome_pointer_leave(struct mui_chrome *chrome, double now_ms) {
    if (!chrome->pointer_inside && !chrome->ctx.hot) return;
    lp_input in = { 0 };
    in.mx = in.my = NAN;
    mui_chrome_event(chrome, &in, now_ms);
    chrome->pointer_inside = 0;
}

void mui_chrome_set_opaque_region(struct mui_chrome *chrome, const pixman_region32_t *region) {
    if (region) {
        pixman_region32_copy(&chrome->opaque_region, region);
        chrome->has_opaque_region = 1;
    } else {
        pixman_region32_clear(&chrome->opaque_region);
        chrome->has_opaque_region = 0;
    }
}

void mui_chrome_apply_opaque(struct mui_chrome *chrome, int enabled) {
    if (!chrome->node) return;
    pixman_region32_t region;
    pixman_region32_init(&region);
    if (enabled && chrome->opaque) pixman_region32_init_rect(&region, 0, 0, chrome->width, chrome->height);
    else if (enabled && chrome->has_opaque_region) pixman_region32_copy(&region, &chrome->opaque_region);
    wlr_scene_buffer_set_opaque_region(chrome->node, &region);
    pixman_region32_fini(&region);
}
