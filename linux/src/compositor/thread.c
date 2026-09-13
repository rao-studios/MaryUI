/* The Thread on the scene (PARITY D25): the desktop's client for threadd lives on the
 * compositor's event loop; what it answers repaints the windows that show it (the
 * Thread app, Disk Utility, Get Info) through lp_desktop_models_changed. */
#include <errno.h>
#include <wlr/util/log.h>

#include "maryui/lp_thread.h"
#include "server.h"

void mui_thread_init(struct mui_server *server) {
    lp_desktop *d = &server->desktop;
    int rc = lp_thread_start(&d->thread, NULL);
    if (rc == -ENOSYS) wlr_log(WLR_INFO, "thread: built without json-c; the Thread app shows nothing");
    else if (rc < 0) wlr_log(WLR_INFO, "thread: no event loop to reach threadd on");
    else wlr_log(WLR_INFO, "thread: reaching threadd at %s", d->thread.path);
}

void mui_thread_finish(struct mui_server *server) { lp_thread_free(&server->desktop.thread); }
