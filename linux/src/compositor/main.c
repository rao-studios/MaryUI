/* maryui-desktop entry point. Runs until SIGTERM/SIGINT (systemd stopping
 * maryos-desktop.service, or the dev launcher swapping the binary). */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"

static int handle_signal(int signal_number, void *data) {
    struct mui_server *server = data;
    wlr_log(WLR_INFO, "signal %d: leaving", signal_number);
    wl_display_terminate(server->display);
    return 0;
}

static int usage(int status) {
    fprintf(status ? stderr : stdout,
        "usage: maryui-desktop [--version]\n"
        "Environment: WLR_RENDERER=pixman|gles2, WLR_BACKENDS, MARYUI_DEBUG=1 (verbose log), MARYUI_DEBUG=frames (motion wake/idle + frame-time histogram)\n");
    return status;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) { printf("maryui-desktop %s\n", lp_version()); return 0; }
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) return usage(0);
        return usage(2);
    }
    const char *debug = getenv("MARYUI_DEBUG");
    int verbose = debug && (strcmp(debug, "1") == 0 || strstr(debug, "all") || strstr(debug, "verbose"));
    wlr_log_init(verbose ? WLR_DEBUG : WLR_INFO, NULL);

    struct mui_server server = { 0 };
    server.debug_frames = debug && strstr(debug, "frames") != NULL;
    if (!mui_server_init(&server)) return 1;

    struct wl_event_loop *loop = wl_display_get_event_loop(server.display);
    struct wl_event_source *sigterm = wl_event_loop_add_signal(loop, SIGTERM, handle_signal, &server);
    struct wl_event_source *sigint = wl_event_loop_add_signal(loop, SIGINT, handle_signal, &server);

    const char *socket = wl_display_add_socket_auto(server.display);
    if (!socket) {
        wlr_log(WLR_ERROR, "could not create a Wayland socket (is XDG_RUNTIME_DIR set?)");
        return 1;
    }
    if (!wlr_backend_start(server.backend)) {
        wlr_log(WLR_ERROR, "could not start the backend");
        mui_server_finish(&server);
        return 1;
    }
    setenv("WAYLAND_DISPLAY", socket, 1);
    wlr_log(WLR_INFO, "maryui-desktop %s on WAYLAND_DISPLAY=%s", lp_version(), socket);

    wl_display_run(server.display);

    wl_event_source_remove(sigterm);
    wl_event_source_remove(sigint);
    mui_server_finish(&server);
    return 0;
}
