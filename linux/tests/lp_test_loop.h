/* A poll(2) event loop standing in for the compositor's wl_event_loop, so
 * apps that ask lp_desktop for fd and timer sources (a terminal's pty, a
 * player's eventfd, a monitor's tick) can be driven headlessly. Install it on
 * a desktop, then run it for a bounded time or until a condition holds. */
#ifndef LP_TEST_LOOP_H
#define LP_TEST_LOOP_H

#include <poll.h>
#include <string.h>
#include <time.h>

#include "maryui/lp_desktop.h"

#define LP_TEST_LOOP_MAX 32

struct lp_source {
    int used;
    int fd;              /* -1 for a timer */
    uint32_t mask;
    double deadline;     /* a timer's, in ms; < 0 while disarmed */
    lp_source_fn fn;
    void *data;
};

static struct lp_source lp_test_sources[LP_TEST_LOOP_MAX];

static double lp_test_now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e3 + (double)t.tv_nsec / 1e6;
}

static lp_source *lp_test_slot(int fd, lp_source_fn fn, void *data) {
    for (int i = 0; i < LP_TEST_LOOP_MAX; i++) {
        lp_source *s = &lp_test_sources[i];
        if (s->used) continue;
        memset(s, 0, sizeof *s);
        s->used = 1;
        s->fd = fd;
        s->deadline = -1;
        s->fn = fn;
        s->data = data;
        return s;
    }
    return NULL;
}

static lp_source *lp_test_add_fd(lp_desktop *d, int fd, uint32_t mask, lp_source_fn fn, void *data) {
    lp_source *s = lp_test_slot(fd, fn, data);
    if (s) s->mask = mask;
    return s;
}

static lp_source *lp_test_add_timer(lp_desktop *d, lp_source_fn fn, void *data) {
    return lp_test_slot(-1, fn, data);
}

static void lp_test_update_timer(lp_desktop *d, lp_source *s, int ms) {
    s->deadline = ms > 0 ? lp_test_now_ms() + ms : -1;
}

static void lp_test_remove_source(lp_desktop *d, lp_source *s) {
    s->used = 0;
}

static void lp_test_loop_install(lp_desktop *d) {
    memset(lp_test_sources, 0, sizeof lp_test_sources);
    d->add_fd = lp_test_add_fd;
    d->add_timer = lp_test_add_timer;
    d->update_timer = lp_test_update_timer;
    d->remove_source = lp_test_remove_source;
}

/* Dispatches ready fds and due timers for up to `ms`, or until `until()` returns 1. Returns the callbacks run. */
static int lp_test_loop_run(int ms, int (*until)(void)) {
    double end = lp_test_now_ms() + ms;
    int fired = 0;
    for (;;) {
        if (until && until()) break;
        double now = lp_test_now_ms();
        if (now >= end) break;
        struct pollfd fds[LP_TEST_LOOP_MAX];
        lp_source *who[LP_TEST_LOOP_MAX];
        int n = 0;
        double next = end;
        for (int i = 0; i < LP_TEST_LOOP_MAX; i++) {
            lp_source *s = &lp_test_sources[i];
            if (!s->used) continue;
            if (s->fd >= 0) {
                fds[n].fd = s->fd;
                fds[n].events = (short)(((s->mask & LP_SOURCE_READABLE) ? POLLIN : 0) | ((s->mask & LP_SOURCE_WRITABLE) ? POLLOUT : 0));
                fds[n].revents = 0;
                who[n++] = s;
            } else if (s->deadline >= 0 && s->deadline < next) {
                next = s->deadline;
            }
        }
        int wait = (int)(next - now + 0.5);
        if (poll(fds, (nfds_t)n, wait < 0 ? 0 : wait) > 0) {
            for (int i = 0; i < n; i++) {
                if (!fds[i].revents || !who[i]->used) continue;
                uint32_t mask = ((fds[i].revents & POLLIN) ? LP_SOURCE_READABLE : 0) | ((fds[i].revents & POLLOUT) ? LP_SOURCE_WRITABLE : 0)
                              | ((fds[i].revents & POLLHUP) ? LP_SOURCE_HANGUP : 0) | ((fds[i].revents & POLLERR) ? LP_SOURCE_ERROR : 0);
                who[i]->fn(who[i]->fd, mask, who[i]->data);
                fired++;
            }
        }
        now = lp_test_now_ms();
        for (int i = 0; i < LP_TEST_LOOP_MAX; i++) {
            lp_source *s = &lp_test_sources[i];
            if (!s->used || s->fd >= 0 || s->deadline < 0 || now < s->deadline) continue;
            s->deadline = -1;   /* one-shot, like wl_event_source_timer_update */
            s->fn(-1, 0, s->data);
            fired++;
        }
    }
    return fired;
}

#endif
