/* Bring-up and teardown of everything wlroots: backend (DRM + libinput via
 * libseat, or the Wayland/X11 nested backends when run inside another
 * session), renderer (GLES2 where there is a GPU, pixman otherwise; honours
 * WLR_RENDERER), allocator, the scene graph with its four layers, the seat
 * and the cursor. Five scene layers: wallpaper, windows, the clock, menus, Spotlight. */
#include <stdlib.h>

#include "server.h"

bool mui_server_init(struct mui_server *server) {
    /* The whole output: no menu bar reserves a strip at the top any more. */
    lp_desktop_init(&server->desktop, LP_RECT(0, 0, 1280, 800), server);
    lp_desktop_register_builtin_apps(&server->desktop);
    server->settings = &server->desktop.settings;
    server->branding = server->desktop.branding;
    wl_list_init(&server->windows);
    lp_motion_engine_init(&server->engine, mui_engine_wake, server);
    server->menu_shown_index = -1;
    wlr_log(WLR_INFO, "desktop for %s (accent %s)", server->branding.pretty_name,
        server->settings->accent == LP_ACCENT_GRAPHITE ? "graphite" : "blue");

    server->display = wl_display_create();
    if (!server->display) return false;

    server->backend = wlr_backend_autocreate(server->display, &server->session);
    if (!server->backend) {
        wlr_log(WLR_ERROR, "no backend: is this a seat (tty with logind/seatd) or a Wayland/X11 session?");
        return false;
    }
    server->renderer = wlr_renderer_autocreate(server->backend);
    if (!server->renderer) {
        wlr_log(WLR_ERROR, "no renderer (try WLR_RENDERER=pixman)");
        return false;
    }
    wlr_renderer_init_wl_display(server->renderer, server->display);
    server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
    if (!server->allocator) {
        wlr_log(WLR_ERROR, "no allocator");
        return false;
    }

    server->compositor = wlr_compositor_create(server->display, 5, server->renderer);
    server->subcompositor = wlr_subcompositor_create(server->display);
    server->data_device = wlr_data_device_manager_create(server->display);
    server->xdg_shell = wlr_xdg_shell_create(server->display, 3);
    server->decoration_manager = wlr_xdg_decoration_manager_v1_create(server->display);

    server->output_layout = wlr_output_layout_create();
    wl_list_init(&server->outputs);
    server->new_output.notify = mui_output_handle_new;
    wl_signal_add(&server->backend->events.new_output, &server->new_output);

    server->scene = wlr_scene_create();
    server->scene_layout = wlr_scene_attach_output_layout(server->scene, server->output_layout);
    server->layer_wallpaper = wlr_scene_tree_create(&server->scene->tree);
    server->layer_windows = wlr_scene_tree_create(&server->scene->tree);
    server->layer_menubar = wlr_scene_tree_create(&server->scene->tree);
    server->layer_menus = wlr_scene_tree_create(&server->scene->tree);
    server->layer_spotlight = wlr_scene_tree_create(&server->scene->tree);

    server->seat = wlr_seat_create(server->display, "seat0");
    server->cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server->cursor, server->output_layout);
    mui_input_init(server);
    mui_cursor_init(server);
    mui_desktop_init(server);
    mui_xdg_init(server);
    return true;
}

void mui_server_finish(struct mui_server *server) {
    wl_display_destroy_clients(server->display);
    mui_desktop_finish(server);
    mui_input_finish(server);
    mui_cursor_finish(server);
    wlr_scene_node_destroy(&server->scene->tree.node);
    wlr_cursor_destroy(server->cursor);
    wlr_output_layout_destroy(server->output_layout);
    wlr_allocator_destroy(server->allocator);
    wlr_renderer_destroy(server->renderer);
    wlr_backend_destroy(server->backend);
    wl_display_destroy(server->display);
}
