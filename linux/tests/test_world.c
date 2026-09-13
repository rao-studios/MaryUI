/* The world the desktop publishes to Mary (PARITY D28): each app's surface as JSON, the focus, the
 * document window and the selection handoff, and the publisher over a fake maryd: on request, on a
 * change, and one app on demand. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "lp_test.h"
#include "lp_test_loop.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_mary.h"
#include "maryui/lp_world.h"

#ifdef HAVE_JSONC
#include <json-c/json.h>

static lp_desktop d;
static char dir[64], sock_path[128];

static const char *field(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) ? json_object_get_string(v) : "";
}
static struct json_object *child(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) ? v : NULL;
}

static void *open_textedit(char id[12]) {
    LP_ASSERT(lp_desktop_open_app_with(&d, "textedit", NULL, NULL, id));
    void *state = lp_desktop_instance(&d, id)->state;
    lp_textedit_set_text(state, "Tides", "The tide comes in twice a day, and the moon pulls it.");
    return state;
}

LP_TEST(a_surface_is_the_apps_own_account_of_its_front_window) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    lp_app_surface s;
    memset(&s, 0, sizeof s);
    s.focused = -1;
    LP_ASSERT_EQ(lp_app_surface_add(&s, "button", "button", "Save", 0, 1), 0);
    LP_ASSERT_EQ(lp_app_surface_add(&s, "textarea", "text area", "Document", 1, 1), 1);
    LP_ASSERT_EQ(s.focused, 1);
    char id[12];
    void *state = open_textedit(id);
    lp_textedit_select(state, 18, 29);
    char *json = lp_desktop_world_json(&d, 1000);
    LP_ASSERT(json != NULL);
    struct json_object *world = json_tokener_parse(json);
    free(json);
    LP_ASSERT_STR(field(world, "type"), "world");
    LP_ASSERT_STR(field(world, "focus"), "applications:textedit");
    struct json_object *places = child(world, "places");
    LP_ASSERT(places && json_object_array_length(places) == 1);
    struct json_object *place = json_object_array_get_idx(places, 0), *surface = child(place, "surface"), *document = child(surface, "document");
    LP_ASSERT_STR(field(place, "place"), "applications:textedit");
    LP_ASSERT_STR(field(child(surface, "application"), "name"), "TextEdit");
    LP_ASSERT_STR(field(child(surface, "activeWindow"), "title"), "Tides");
    LP_ASSERT_STR(field(document, "name"), "Tides");
    LP_ASSERT_STR(field(document, "text"), "The tide comes in twice a day, and the moon pulls it.");
    LP_ASSERT_EQ(json_object_get_int(child(document, "total")), 53);
    LP_ASSERT_EQ(json_object_get_int(child(document, "upper")), 53);
    struct json_object *elements = child(surface, "elements");
    LP_ASSERT_EQ(json_object_array_length(elements), 3);
    LP_ASSERT_STR(field(json_object_array_get_idx(elements, 1), "identity"), "textarea|Document");
    LP_ASSERT_STR(field(child(surface, "focused"), "label"), "Document");
    LP_ASSERT_EQ(json_object_array_length(child(world, "windows")), 1);
    json_object_put(world);
    /* one app on demand, with its place */
    json = lp_desktop_app_surface_json(&d, "textedit", 1000);
    LP_ASSERT(json && strstr(json, "\"place\":\"applications:textedit\""));
    free(json);
    LP_ASSERT(lp_desktop_app_surface_json(&d, "finder", 1000) == NULL);     /* no window */
    LP_ASSERT(lp_desktop_app_surface_json(&d, "nothing", 1000) == NULL);
}

/* ---- a fake maryd on a unix socket ---- */

static int listen_at(void) {
    if (!dir[0]) {
        snprintf(dir, sizeof dir, "/tmp/lp-world-XXXXXX");
        if (!mkdtemp(dir)) return -1;
        snprintf(sock_path, sizeof sock_path, "%s/mary.sock", dir);
    }
    unlink(sock_path);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof addr.sun_path, "%s", sock_path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0 || listen(fd, 4) < 0) { close(fd); return -1; }
    return fd;
}

static int accept_within(int listener, int ms) {
    struct pollfd p = { .fd = listener, .events = POLLIN };
    for (int i = 0; i < ms / 10; i++) {
        lp_test_loop_run(10, NULL);
        if (poll(&p, 1, 0) > 0) return accept(listener, NULL, NULL);
    }
    return -1;
}

static void say(int fd, const char *text) {
    size_t n = strlen(text);
    while (n) {
        ssize_t w = write(fd, text, n);
        if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { lp_test_loop_run(10, NULL); continue; }
        if (w <= 0) return;
        text += w;
        n -= (size_t)w;
    }
}

/* The next line of `type` from the desktop (others are dropped), within a second. */
static struct json_object *hear(int fd, const char *type) {
    static char line[1 << 16];
    for (int tries = 0; tries < 100; tries++) {
        size_t n = 0;
        struct pollfd p = { .fd = fd, .events = POLLIN };
        while (n < sizeof line - 1) {
            lp_test_loop_run(5, NULL);
            if (poll(&p, 1, 20) <= 0) { if (n) continue; break; }
            if (read(fd, line + n, 1) != 1) break;
            if (line[n] == '\n') break;
            n++;
        }
        if (!n) continue;
        line[n] = 0;
        struct json_object *msg = json_tokener_parse(line);
        if (msg && strcmp(field(msg, "type"), type) == 0) return msg;
        if (msg) json_object_put(msg);
    }
    return NULL;
}

LP_TEST(the_world_goes_to_maryd_on_request_on_change_and_one_app_on_demand) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    lp_test_loop_install(&d);
    int listener = listen_at();
    LP_ASSERT(listener >= 0);
    LP_ASSERT_EQ(lp_mary_start(&d.mary, sock_path), 0);
    int fd = accept_within(listener, 2000);
    LP_ASSERT(fd >= 0);
    lp_test_loop_run(50, NULL);
    LP_ASSERT(lp_mary_connected(&d.mary));
    char id[12];
    void *state = open_textedit(id);         /* opening a window publishes, after 50 ms */
    struct json_object *world = hear(fd, "world");
    LP_ASSERT(world != NULL);
    LP_ASSERT_STR(field(world, "focus"), "applications:textedit");
    if (world) json_object_put(world);
    LP_ASSERT(d.world.published >= 1);
    int published = d.world.published;
    /* a turn starts: maryd asks, the desktop answers at once */
    say(fd, "{\"type\":\"world.request\"}\n");
    world = hear(fd, "world");
    LP_ASSERT(world != NULL);
    if (world) json_object_put(world);
    LP_ASSERT_EQ(d.world.published, published + 1);
    /* a selection in the front window is a handoff of its own, sent once, cleared when it goes */
    lp_textedit_select(state, 18, 29);
    say(fd, "{\"type\":\"world.request\"}\n");
    struct json_object *selection = hear(fd, "selection");
    LP_ASSERT(selection != NULL);
    LP_ASSERT_STR(field(selection, "text"), "twice a day");
    LP_ASSERT_STR(field(selection, "applicationID"), "textedit");
    LP_ASSERT_STR(field(selection, "document"), "Tides");
    LP_ASSERT_EQ(json_object_get_int(child(selection, "lower")), 18);
    LP_ASSERT_EQ(json_object_get_int(child(selection, "upper")), 29);
    LP_ASSERT(json_object_get_boolean(child(selection, "editable")));
    if (selection) json_object_put(selection);
    say(fd, "{\"type\":\"world.request\"}\n");
    world = hear(fd, "world");                       /* the same selection again: no second handoff */
    LP_ASSERT(world != NULL);
    if (world) json_object_put(world);
    LP_ASSERT(d.world.selection_text && strcmp(d.world.selection_text, "twice a day") == 0);
    lp_textedit_select(state, 0, 0);
    say(fd, "{\"type\":\"world.request\"}\n");
    struct json_object *clear = hear(fd, "selection.clear");
    LP_ASSERT(clear != NULL);
    LP_ASSERT_STR(field(clear, "applicationID"), "textedit");
    if (clear) json_object_put(clear);
    /* one app's surface on demand, and an unknown one */
    say(fd, "{\"type\":\"app.state\",\"call_id\":\"call-7\",\"app\":\"textedit\"}\n");
    struct json_object *result = hear(fd, "app.state.result");
    LP_ASSERT(result != NULL);
    LP_ASSERT_STR(field(result, "call_id"), "call-7");
    LP_ASSERT(json_object_get_boolean(child(result, "ok")));
    LP_ASSERT_STR(field(child(child(result, "surface"), "activeWindow"), "title"), "Tides");
    if (result) json_object_put(result);
    say(fd, "{\"type\":\"app.state\",\"call_id\":\"call-8\",\"app\":\"mail\"}\n");
    result = hear(fd, "app.state.result");
    LP_ASSERT(result != NULL);
    LP_ASSERT(!json_object_get_boolean(child(result, "ok")));
    LP_ASSERT_STR(field(result, "error"), "unknown");
    if (result) json_object_put(result);
    /* the poll follows the fastest app with a window: TextEdit's fifteen seconds */
    LP_ASSERT_EQ(d.world.poll_s, 15);
    lp_world_free(&d);
    lp_mary_free(&d.mary);
    close(fd);
    close(listener);
    unlink(sock_path);
    rmdir(dir);
}

int main(void) {
    LP_RUN(a_surface_is_the_apps_own_account_of_its_front_window);
    LP_RUN(the_world_goes_to_maryd_on_request_on_change_and_one_app_on_demand);
    return lp_test_failures;
}
#else
int main(void) { return 0; }
#endif
