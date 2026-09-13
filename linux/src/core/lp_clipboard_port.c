/* The host's clipboard through a virtio console port (lp_clipboard_port.h). */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "maryui/components/lp_text_area.h"
#include "maryui/lp_clipboard_port.h"
#include "maryui/lp_desktop.h"

void lp_clipboard_port_init(lp_clipboard_port *p) {
    memset(p, 0, sizeof *p);
    p->fd = -1;
}

static void reset_message(lp_clipboard_port *p) {
    free(p->text);
    p->text = NULL;
    p->header_len = p->want = p->have = p->skip = 0;
}

int lp_clipboard_port_feed(lp_clipboard_port *p, const char *bytes, size_t n) {
    int taken = 0;
    while (n) {
        if (p->skip) {
            size_t k = n < p->skip ? n : p->skip;
            p->skip -= k;
            bytes += k;
            n -= k;
            continue;
        }
        if (!p->text) {
            size_t k = 4 - p->header_len < n ? 4 - p->header_len : n;
            memcpy(p->header + p->header_len, bytes, k);
            p->header_len += k;
            bytes += k;
            n -= k;
            if (p->header_len < 4) break;
            p->header_len = 0;
            uint32_t len = p->header[0] | p->header[1] << 8 | p->header[2] << 16 | (uint32_t)p->header[3] << 24;
            if (len == 0) continue;
            if (len > LP_CLIPBOARD_FRAME_MAX || !(p->text = malloc(len))) {
                p->skip = len;
                continue;
            }
            p->want = len;
            p->have = 0;
            continue;
        }
        size_t k = p->want - p->have < n ? p->want - p->have : n;
        memcpy(p->text + p->have, bytes, k);
        p->have += k;
        bytes += k;
        n -= k;
        if (p->have == p->want) {
            lp_text_clipboard_set(lp_text_clipboard_shared(), p->text, (int)p->want);
            free(p->text);
            p->text = NULL;
            p->messages++;
            taken++;
        }
    }
    return taken;
}

static int on_retry(int fd, uint32_t mask, void *data);

static void shut(lp_clipboard_port *p) {
    if (p->source) lp_desktop_remove_source(p->desk, p->source);
    p->source = NULL;
    if (p->fd >= 0) close(p->fd);
    p->fd = -1;
    reset_message(p);
}

/* The host's side went away (or never came): look again later. */
static void reopen_later(lp_clipboard_port *p) {
    shut(p);
    if (!p->retry) p->retry = lp_desktop_add_timer(p->desk, 0, on_retry, p);
    if (p->retry) lp_desktop_update_timer(p->desk, p->retry, LP_CLIPBOARD_RETRY_MS);
}

static int on_readable(int fd, uint32_t mask, void *data) {
    lp_clipboard_port *p = data;
    char buf[16384];
    while (p->fd == fd) {
        ssize_t r = read(fd, buf, sizeof buf);
        if (r > 0) {
            lp_clipboard_port_feed(p, buf, (size_t)r);
            continue;
        }
        if (r < 0 && errno == EINTR) continue;
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (mask & (LP_SOURCE_HANGUP | LP_SOURCE_ERROR)) reopen_later(p);
            break;
        }
        reopen_later(p);
        break;
    }
    return 0;
}

static int attach(lp_clipboard_port *p) {
    int fd = open(p->path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -errno;
    p->source = lp_desktop_add_fd(p->desk, fd, LP_SOURCE_READABLE, on_readable, p);
    if (!p->source) {
        close(fd);
        return -ENOSYS;
    }
    p->fd = fd;
    return 0;
}

static int on_retry(int fd, uint32_t mask, void *data) {
    lp_clipboard_port *p = data;
    if (p->fd < 0 && attach(p) < 0 && p->retry) lp_desktop_update_timer(p->desk, p->retry, LP_CLIPBOARD_RETRY_MS);
    return 0;
}

int lp_clipboard_port_open(lp_clipboard_port *p, lp_desktop *d, const char *path) {
    lp_clipboard_port_close(p);
    p->desk = d;
    snprintf(p->path, sizeof p->path, "%s", path ? path : LP_CLIPBOARD_PORT);
    if (!d || !d->add_fd || !d->add_timer) return -ENOSYS;
    return attach(p);
}

void lp_clipboard_port_close(lp_clipboard_port *p) {
    if (p->desk) shut(p);
    if (p->retry) lp_desktop_remove_source(p->desk, p->retry);
    p->retry = NULL;
    reset_message(p);
}
