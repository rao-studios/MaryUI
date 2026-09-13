/* Mary on the scene (PARITY D18): the desktop's client for maryd lives on the
 * compositor's event loop, what it hears repaints Spotlight, and "Hey Mary" opens
 * Spotlight on the conversation, listening. */
#include <errno.h>
#include <wlr/util/log.h>

#include "maryui/lp_mary.h"
#include "chrome.h"
#include "window.h"

static void on_mary(lp_desktop *d, unsigned what) {
    struct mui_server *server = d->host;
    /* maryd (re)connected: tell it what each app lets Mary do, and her wake word and voice, which it does not keep. */
    if ((what & LP_MARY_CHANGED_CONNECTION) && lp_mary_connected(&d->mary)) {
        lp_desktop_publish_skills(d);
        lp_desktop_publish_mary_config(d);
        lp_desktop_publish_world(d);        /* what is on screen (PARITY D28), then on every change and poll */
    }
    if (what & LP_MARY_WAKE) {
        lp_desktop_mary_wake(d);
        if (!server->spotlight) {
            mui_spotlight_request_sync(server);     /* opening makes the chrome: deferred, as every open and close is */
            return;
        }
        mui_spotlight_resize(server);               /* from the dock to the conversation */
    }
    if (!server->spotlight) return;
    mui_chrome_damage_all(server->spotlight);
    mui_chrome_repaint(server->spotlight, mui_now_ms());
}

void mui_mary_init(struct mui_server *server) {
    lp_desktop *d = &server->desktop;
    d->on_mary = on_mary;
    int rc = lp_mary_start(&d->mary, NULL);
    if (rc == -ENOSYS) wlr_log(WLR_INFO, "mary: built without json-c; Spotlight has no Ask Mary");
    else if (rc < 0) wlr_log(WLR_INFO, "mary: no XDG_RUNTIME_DIR, so there is no maryd to reach");
    else wlr_log(WLR_INFO, "mary: reaching maryd at %s", d->mary.path);
}

void mui_mary_finish(struct mui_server *server) {
    server->desktop.on_mary = NULL;
    lp_world_free(&server->desktop);
    lp_mary_free(&server->desktop.mary);
}
