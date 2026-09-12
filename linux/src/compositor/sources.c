/* Event sources for the built-in apps: lp_desktop.add_fd / add_timer as
 * wl_event_loop sources, so a terminal's pty, a player's frame eventfd or a
 * monitor's tick runs on the compositor thread like every other event. The
 * WL_EVENT_* bits are LP_SOURCE_*'s, so masks pass straight through. */
#include <stdlib.h>

#include "maryui/lp_desktop.h"
#include "chrome.h"

struct lp_source {
    struct wl_event_source *source;
    lp_source_fn fn;
    void *data;
};

/* The app may remove its own source from inside fn: nothing touches `s` after the call. */
static int on_fd(int fd, uint32_t mask, void *data) {
    struct lp_source *s = data;
    return s->fn(fd, mask, s->data);
}

static int on_timer(void *data) {
    struct lp_source *s = data;
    return s->fn(-1, 0, s->data);
}

static struct wl_event_loop *loop_of(lp_desktop *d) {
    struct mui_server *server = d->host;
    return wl_display_get_event_loop(server->display);
}

static lp_source *add_fd(lp_desktop *d, int fd, uint32_t mask, lp_source_fn fn, void *data) {
    struct lp_source *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->fn = fn;
    s->data = data;
    s->source = wl_event_loop_add_fd(loop_of(d), fd, mask, on_fd, s);
    if (!s->source) {
        wlr_log(WLR_ERROR, "sources: cannot watch fd %d", fd);
        free(s);
        return NULL;
    }
    return s;
}

static lp_source *add_timer(lp_desktop *d, lp_source_fn fn, void *data) {
    struct lp_source *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->fn = fn;
    s->data = data;
    s->source = wl_event_loop_add_timer(loop_of(d), on_timer, s);
    if (!s->source) {
        wlr_log(WLR_ERROR, "sources: cannot create a timer");
        free(s);
        return NULL;
    }
    return s;
}

static void update_timer(lp_desktop *d, lp_source *s, int ms) {
    wl_event_source_timer_update(s->source, ms);
}

static void remove_source(lp_desktop *d, lp_source *s) {
    wl_event_source_remove(s->source);
    free(s);
}

void mui_sources_init(struct mui_server *server) {
    server->desktop.add_fd = add_fd;
    server->desktop.add_timer = add_timer;
    server->desktop.update_timer = update_timer;
    server->desktop.remove_source = remove_source;
}
