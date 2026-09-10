/* A chrome: one Cairo-painted rectangle shown as a wlr_scene_buffer. The
 * paint callback runs in the EVENT pass on input and in the DRAW pass when
 * dirty; repaints go into a buffer the scene no longer holds and are
 * published with their damage, so only what changed is recomposited. */
#ifndef MUI_CHROME_H
#define MUI_CHROME_H

#include <pixman.h>

#include "maryui/lp_ui.h"
#include "server.h"

#define MUI_CHROME_BUFFERS 3

struct mui_chrome;
typedef void (*mui_chrome_paint_fn)(lp_ctx *ctx, struct mui_chrome *chrome, void *data);

struct mui_chrome {
    struct mui_server *server;
    struct wlr_scene_buffer *node;
    struct lp_cairo_buffer *buffers[MUI_CHROME_BUFFERS];
    int current;                 /* the buffer the scene shows, -1 before the first paint */
    int width, height;
    int opaque;                  /* the whole rectangle is opaque (lets the renderer blit) */
    pixman_region32_t opaque_region; /* buffer-local; used when set (mui_chrome_set_opaque_region) and the node is unscaled */
    int has_opaque_region;
    pixman_region32_t damage;    /* pending, in chrome coordinates */
    /* Buffer age: where each buffer differs from what is on screen. Repainting
     * into one only has to restore that much from the current buffer, instead of
     * copying the whole surface every time. */
    pixman_region32_t stale[MUI_CHROME_BUFFERS];
    mui_chrome_paint_fn paint;
    void *data;
    lp_ctx ctx;
    int pointer_inside;
    int painting;                /* a pass is running: a repaint asked for from inside it is left to the caller */
    int ambient;                 /* the last full paint of ambient_rect asked for another frame */
    lp_rect ambient_rect;
};

void mui_chrome_init(struct mui_chrome *chrome, struct mui_server *server, struct wlr_scene_tree *parent,
                     int width, int height, mui_chrome_paint_fn paint, void *data);
void mui_chrome_finish(struct mui_chrome *chrome);
void mui_chrome_resize(struct mui_chrome *chrome, int width, int height);
void mui_chrome_damage(struct mui_chrome *chrome, lp_rect r);
void mui_chrome_damage_all(struct mui_chrome *chrome);
/* Declares which pixels are opaque (the frame inside its shadow margin), so the scene skips what lies beneath. NULL clears. */
void mui_chrome_set_opaque_region(struct mui_chrome *chrome, const pixman_region32_t *region);
/* Re-applies (or, while the node is scaled, withdraws) the opaque region. */
void mui_chrome_apply_opaque(struct mui_chrome *chrome, int enabled);
/* Runs the EVENT pass with `in`; returns the cursor shape the chrome wants. Repaints if anything changed. */
enum lp_cursor_shape mui_chrome_event(struct mui_chrome *chrome, const lp_input *in, double now_ms);
/* The pointer left the chrome: clear hover state. */
void mui_chrome_pointer_leave(struct mui_chrome *chrome, double now_ms);
/* A key event for the chrome (the pointer position is kept from the last motion). */
void mui_chrome_key(struct mui_chrome *chrome, uint32_t keysym, uint32_t mods, const char *utf8, int pressed, double now_ms);
/* A wheel event over the chrome. */
void mui_chrome_scroll(struct mui_chrome *chrome, float dx, float dy, double now_ms);
/* Whether an ambient animation (progress glint, rolling bubble) wants another frame, and where. */
int mui_chrome_wants_frame(const struct mui_chrome *chrome);
lp_rect mui_chrome_ambient_rect(const struct mui_chrome *chrome);
/* Repaint the damaged region (or everything on the first paint) and publish it. */
void mui_chrome_repaint(struct mui_chrome *chrome, double now_ms);

/* A monotonic clock in milliseconds, the engine's and the chromes' time base. */
double mui_now_ms(void);

#endif
