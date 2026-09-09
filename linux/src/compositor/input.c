/* Seat, keyboards and the pointer. Keys go to the desktop (shortcuts, menus,
 * Spotlight, the focused built-in window) and otherwise to the focused client.
 * Clients repeat held keys themselves from the seat's repeat info; a key a
 * chrome consumed is repeated here with the same rate and delay.
 * Ctrl+Alt+Backspace ends the session so a development VM is never stuck. */
#include <stdio.h>
#include <stdlib.h>
#include <linux/input-event-codes.h>

#include "maryui/lp_ui.h"
#include "server.h"

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
    struct mui_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
    wlr_seat_keyboard_notify_modifiers(keyboard->server->seat, &keyboard->wlr_keyboard->modifiers);
    mui_drag_mods_changed(keyboard->server); /* Alt during a drag: copy instead of move */
}

static bool handle_desktop_key(struct mui_server *server, uint32_t modifiers, xkb_keysym_t sym, const char *utf8, int pressed) {
    if (pressed && (modifiers & WLR_MODIFIER_CTRL) && (modifiers & WLR_MODIFIER_ALT) && sym == XKB_KEY_BackSpace) {
        wlr_log(WLR_INFO, "Ctrl+Alt+Backspace: leaving");
        wl_display_terminate(server->display);
        return true;
    }
    return mui_desktop_key(server, sym, modifiers, utf8, pressed) != 0;
}

static int is_modifier_sym(xkb_keysym_t sym) {
    return (sym >= XKB_KEY_Shift_L && sym <= XKB_KEY_Hyper_R) || sym == XKB_KEY_Caps_Lock || sym == XKB_KEY_Num_Lock;
}

void mui_input_disarm_repeat(struct mui_server *server) {
    server->key_repeat.active = 0;
    if (server->key_repeat.timer) wl_event_source_timer_update(server->key_repeat.timer, 0);
}

static int repeat_fire(void *data) {
    struct mui_server *server = data;
    if (!server->key_repeat.active) return 0;
    mui_desktop_key(server, server->key_repeat.keysym, server->key_repeat.mods, server->key_repeat.utf8, 1);
    if (!server->key_repeat.active) return 0; /* the handler disarmed it (Spotlight closed, focus moved) */
    int rate = server->key_repeat.rate > 0 ? server->key_repeat.rate : 25;
    wl_event_source_timer_update(server->key_repeat.timer, 1000 / rate);
    return 0;
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
    struct mui_keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct mui_server *server = keyboard->server;
    struct wlr_keyboard_key_event *event = data;
    int pressed = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;

    uint32_t keycode = event->keycode + 8; /* libinput keycode -> xkbcommon */
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);
    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);

    /* Any press, or the release of the repeating key, ends a repeat. */
    if (server->key_repeat.active && (pressed || event->keycode == server->key_repeat.keycode)) mui_input_disarm_repeat(server);

    bool handled = false;
    char utf8[8] = "";
    xkb_state_key_get_utf8(keyboard->wlr_keyboard->xkb_state, keycode, utf8, sizeof utf8);
    for (int i = 0; i < nsyms; i++) handled |= handle_desktop_key(server, modifiers, syms[i], utf8, pressed);
    if (handled && pressed && server->key_to_chrome && nsyms > 0 && !is_modifier_sym(syms[0]) && server->key_repeat.timer) {
        server->key_repeat.active = 1;
        server->key_repeat.keycode = event->keycode;
        server->key_repeat.keysym = syms[0];
        server->key_repeat.mods = modifiers;
        server->key_repeat.rate = keyboard->wlr_keyboard->repeat_info.rate;
        snprintf(server->key_repeat.utf8, sizeof server->key_repeat.utf8, "%s", utf8);
        int delay = keyboard->wlr_keyboard->repeat_info.delay;
        wl_event_source_timer_update(server->key_repeat.timer, delay > 0 ? delay : 600);
    }
    if (!handled) {
        wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);
        wlr_seat_keyboard_notify_key(server->seat, event->time_msec, event->keycode, event->state);
    }
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
    struct mui_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->destroy.link);
    wl_list_remove(&keyboard->link);
    free(keyboard);
}

static void new_keyboard(struct mui_server *server, struct wlr_input_device *device) {
    struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);
    struct mui_keyboard *keyboard = calloc(1, sizeof(*keyboard));
    keyboard->server = server;
    keyboard->wlr_keyboard = wlr_keyboard;

    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    wlr_keyboard_set_keymap(wlr_keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

    keyboard->modifiers.notify = keyboard_handle_modifiers;
    wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
    keyboard->key.notify = keyboard_handle_key;
    wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
    keyboard->destroy.notify = keyboard_handle_destroy;
    wl_signal_add(&device->events.destroy, &keyboard->destroy);

    wlr_seat_set_keyboard(server->seat, wlr_keyboard);
    wl_list_insert(&server->keyboards, &keyboard->link);
}

void mui_input_handle_new(struct wl_listener *listener, void *data) {
    struct mui_server *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;
    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        new_keyboard(server, device);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        wlr_cursor_attach_input_device(server->cursor, device);
        break;
    default:
        break;
    }
    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards)) caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    wlr_seat_set_capabilities(server->seat, caps);
}

static int held_buttons = 0;

static void pointer_moved(struct mui_server *server, uint32_t time_msec) {
    mui_desktop_pointer_event(server, server->cursor->x, server->cursor->y, held_buttons, 0, 0, time_msec);
}

static void cursor_motion(struct wl_listener *listener, void *data) {
    struct mui_server *server = wl_container_of(listener, server, cursor_motion);
    struct wlr_pointer_motion_event *event = data;
    wlr_cursor_move(server->cursor, &event->pointer->base, event->delta_x, event->delta_y);
    pointer_moved(server, event->time_msec);
}

static void cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct mui_server *server = wl_container_of(listener, server, cursor_motion_absolute);
    struct wlr_pointer_motion_absolute_event *event = data;
    wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);
    pointer_moved(server, event->time_msec);
}

static int button_bit(uint32_t button) {
    switch (button) {
    case BTN_LEFT: return LP_BUTTON_LEFT;
    case BTN_RIGHT: return LP_BUTTON_RIGHT;
    case BTN_MIDDLE: return LP_BUTTON_MIDDLE;
    default: return 0;
    }
}

static void cursor_button(struct wl_listener *listener, void *data) {
    struct mui_server *server = wl_container_of(listener, server, cursor_button);
    struct wlr_pointer_button_event *event = data;
    int bit = button_bit(event->button);
    int pressed = event->state == WLR_BUTTON_PRESSED ? bit : 0;
    int released = event->state == WLR_BUTTON_RELEASED ? bit : 0;
    if (pressed) held_buttons |= bit; else held_buttons &= ~bit;
    if (mui_desktop_pointer_event(server, server->cursor->x, server->cursor->y, held_buttons, pressed, released, event->time_msec)) return;
    wlr_seat_pointer_notify_button(server->seat, event->time_msec, event->button, event->state);
}

static void cursor_axis(struct wl_listener *listener, void *data) {
    struct mui_server *server = wl_container_of(listener, server, cursor_axis);
    struct wlr_pointer_axis_event *event = data;
    float dx = event->orientation == WLR_AXIS_ORIENTATION_HORIZONTAL ? (float)event->delta : 0;
    float dy = event->orientation == WLR_AXIS_ORIENTATION_VERTICAL ? (float)event->delta : 0;
    if (mui_desktop_scroll(server, server->cursor->x, server->cursor->y, dx, dy)) return;
    wlr_seat_pointer_notify_axis(server->seat, event->time_msec, event->orientation, event->delta,
        event->delta_discrete, event->source);
}

static void cursor_frame(struct wl_listener *listener, void *data) {
    struct mui_server *server = wl_container_of(listener, server, cursor_frame);
    wlr_seat_pointer_notify_frame(server->seat);
}

static void seat_request_set_cursor(struct wl_listener *listener, void *data) {
    struct mui_server *server = wl_container_of(listener, server, request_set_cursor);
    struct wlr_seat_pointer_request_set_cursor_event *event = data;
    struct wlr_seat_client *focused = server->seat->pointer_state.focused_client;
    if (focused == event->seat_client && server->seat->pointer_state.focused_surface) {
        wlr_cursor_set_surface(server->cursor, event->surface, event->hotspot_x, event->hotspot_y);
        server->cursor_shape = MUI_CURSOR_COUNT; /* a client image is showing */
    }
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
    struct mui_server *server = wl_container_of(listener, server, request_set_selection);
    struct wlr_seat_request_set_selection_event *event = data;
    wlr_seat_set_selection(server->seat, event->source, event->serial);
}

void mui_input_finish(struct mui_server *server) {
    if (server->key_repeat.timer) wl_event_source_remove(server->key_repeat.timer);
    server->key_repeat.timer = NULL;
    server->key_repeat.active = 0;
}

void mui_input_init(struct mui_server *server) {
    wl_list_init(&server->keyboards);
    server->key_repeat.timer = wl_event_loop_add_timer(wl_display_get_event_loop(server->display), repeat_fire, server);
    server->new_input.notify = mui_input_handle_new;
    wl_signal_add(&server->backend->events.new_input, &server->new_input);

    server->cursor_motion.notify = cursor_motion;
    wl_signal_add(&server->cursor->events.motion, &server->cursor_motion);
    server->cursor_motion_absolute.notify = cursor_motion_absolute;
    wl_signal_add(&server->cursor->events.motion_absolute, &server->cursor_motion_absolute);
    server->cursor_button.notify = cursor_button;
    wl_signal_add(&server->cursor->events.button, &server->cursor_button);
    server->cursor_axis.notify = cursor_axis;
    wl_signal_add(&server->cursor->events.axis, &server->cursor_axis);
    server->cursor_frame.notify = cursor_frame;
    wl_signal_add(&server->cursor->events.frame, &server->cursor_frame);

    server->request_set_cursor.notify = seat_request_set_cursor;
    wl_signal_add(&server->seat->events.request_set_cursor, &server->request_set_cursor);
    server->request_set_selection.notify = seat_request_set_selection;
    wl_signal_add(&server->seat->events.request_set_selection, &server->request_set_selection);
}
