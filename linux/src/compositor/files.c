/* Directory watching for the Finder: one inotify descriptor on the event
 * loop, a refcounted watch per directory (lp_desktop.watch), and a 50 ms
 * coalescing timer that turns events into lp_desktop_files_changed. */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "maryui/lp_files.h"
#include "chrome.h"

#define COALESCE_MS 50
#define MASK (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF)

struct watch { int wd; int refs; char path[LP_FILES_PATH_MAX]; };

struct mui_files {
    int fd;
    struct wl_event_source *source, *timer;
    struct watch *watches;
    int count, cap;
    char (*pending)[LP_FILES_PATH_MAX];
    int npending, pcap;
};

static struct watch *find_path(struct mui_files *f, const char *path) {
    for (int i = 0; i < f->count; i++) if (strcmp(f->watches[i].path, path) == 0) return &f->watches[i];
    return NULL;
}

static void drop_watch(struct mui_files *f, int i) {
    f->watches[i] = f->watches[--f->count];
}

static void queue(struct mui_server *server, const char *path) {
    struct mui_files *f = server->files;
    for (int i = 0; i < f->npending; i++) if (strcmp(f->pending[i], path) == 0) return;
    if (f->npending == f->pcap) {
        int cap = f->pcap ? f->pcap * 2 : 8;
        void *grown = realloc(f->pending, (size_t)cap * sizeof *f->pending);
        if (!grown) return;
        f->pending = grown;
        f->pcap = cap;
    }
    snprintf(f->pending[f->npending++], LP_FILES_PATH_MAX, "%s", path);
    wl_event_source_timer_update(f->timer, COALESCE_MS);
}

static int on_timer(void *data) {
    struct mui_server *server = data;
    struct mui_files *f = server->files;
    int n = f->npending;
    f->npending = 0;
    for (int i = 0; i < n; i++) lp_desktop_files_changed(&server->desktop, f->pending[i]);
    return 0;
}

static int on_inotify(int fd, uint32_t mask, void *data) {
    struct mui_server *server = data;
    struct mui_files *f = server->files;
    char buf[16384] __attribute__((aligned(__alignof__(struct inotify_event))));
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        if (n <= 0) break;
        for (char *p = buf; p < buf + n;) {
            const struct inotify_event *ev = (const struct inotify_event *)p;
            for (int i = 0; i < f->count; i++) {
                if (f->watches[i].wd != ev->wd) continue;
                queue(server, f->watches[i].path);
                if (ev->mask & IN_IGNORED) drop_watch(f, i); /* the directory is gone; the kernel dropped the watch */
                break;
            }
            p += sizeof *ev + ev->len;
        }
    }
    return 0;
}

static void desktop_watch(lp_desktop *d, const char *dir, int on) {
    struct mui_server *server = d->host;
    struct mui_files *f = server->files;
    if (!f || f->fd < 0) return;
    struct watch *w = find_path(f, dir);
    if (on) {
        if (w) { w->refs++; return; }
        int wd = inotify_add_watch(f->fd, dir, MASK);
        if (wd < 0) { wlr_log(WLR_INFO, "files: cannot watch %s: %s", dir, strerror(errno)); return; }
        if (f->count == f->cap) {
            int cap = f->cap ? f->cap * 2 : 8;
            void *grown = realloc(f->watches, (size_t)cap * sizeof *f->watches);
            if (!grown) { inotify_rm_watch(f->fd, wd); return; }
            f->watches = grown;
            f->cap = cap;
        }
        w = &f->watches[f->count++];
        w->wd = wd;
        w->refs = 1;
        snprintf(w->path, sizeof w->path, "%s", dir);
    } else if (w && --w->refs <= 0) {
        inotify_rm_watch(f->fd, w->wd);
        drop_watch(f, (int)(w - f->watches));
    }
}

void mui_files_init(struct mui_server *server) {
    struct mui_files *f = calloc(1, sizeof *f);
    server->files = f;
    f->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (f->fd < 0) { wlr_log(WLR_ERROR, "files: inotify_init1: %s (no live refresh)", strerror(errno)); return; }
    struct wl_event_loop *loop = wl_display_get_event_loop(server->display);
    f->source = wl_event_loop_add_fd(loop, f->fd, WL_EVENT_READABLE, on_inotify, server);
    f->timer = wl_event_loop_add_timer(loop, on_timer, server);
    server->desktop.watch = desktop_watch;
}

void mui_files_finish(struct mui_server *server) {
    struct mui_files *f = server->files;
    if (!f) return;
    if (f->source) wl_event_source_remove(f->source);
    if (f->timer) wl_event_source_remove(f->timer);
    if (f->fd >= 0) close(f->fd);
    free(f->watches);
    free(f->pending);
    free(f);
    server->files = NULL;
    server->desktop.watch = NULL;
}
