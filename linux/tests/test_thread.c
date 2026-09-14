/* The Thread on the desktop: the threadd client over a fake threadd on a unix socket,
 * and the Threads app driven headlessly. */
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE    /* mkdtemp under _POSIX_C_SOURCE */
#endif
#include <errno.h>
#include <fcntl.h>
#include <math.h>
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
#include "maryui/lp_files.h"
#include "maryui/lp_graph.h"
#include "maryui/lp_thread.h"

static lp_desktop d;
static char dir[64], sock_path[128];
static unsigned changes;

#ifdef HAVE_JSONC
#include <json-c/json.h>

static int listen_at(void) {
    if (!dir[0]) {
        snprintf(dir, sizeof dir, "/tmp/lp-thread-XXXXXX");
        if (!mkdtemp(dir)) return -1;
        snprintf(sock_path, sizeof sock_path, "%s/local.sock", dir);
    }
    unlink(sock_path);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof addr.sun_path, "%s", sock_path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0 || listen(fd, 4) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int accept_within(int listener, int ms) {
    struct pollfd p = { .fd = listener, .events = POLLIN };
    return poll(&p, 1, ms) > 0 ? accept(listener, NULL, NULL) : -1;
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

static struct json_object *hear(int fd) {
    char line[8192];
    size_t n = 0;
    struct pollfd p = { .fd = fd, .events = POLLIN };
    while (n < sizeof line - 1 && poll(&p, 1, 1000) > 0) {
        if (read(fd, line + n, 1) != 1) break;
        if (line[n] == '\n') { line[n] = 0; return json_tokener_parse(line); }
        n++;
    }
    return NULL;
}

static const char *field(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) ? json_object_get_string(v) : "";
}

static lp_app_instance *thread_instance;
static int thread_model_changed(void *state, lp_desktop *desk, unsigned model, unsigned what) {
    if (model == LP_MODEL_THREAD) changes |= what;
    return 0;
}
static const lp_app watcher = { .id = "watcher", .title = "Watcher", .model_changed = thread_model_changed };

static void fresh(void) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_test_loop_install(&d);
    lp_desktop_register_builtin_apps(&d);
    lp_desktop_register_app(&d, &watcher);
    changes = 0;
    /* a window whose app reports every model change */
    char id[12];
    lp_desktop_open_app_with(&d, "watcher", NULL, NULL, id);
    thread_instance = lp_desktop_instance(&d, id);
    if (thread_instance) thread_instance->state = (void *)1;
}

static int connected(void) { return lp_thread_connected(&d.thread); }
static int got_stats(void) { return (changes & LP_THREAD_CHANGED_STATS) != 0; }
static int got_error(void) { return (changes & LP_THREAD_CHANGED_ERROR) != 0; }
static int disconnected(void) { return !lp_thread_connected(&d.thread); }

LP_TEST(requests_go_out_as_lines_and_answers_come_back_by_kind) {
    fresh();
    int listener = listen_at();
    LP_ASSERT(listener >= 0);
    LP_ASSERT_EQ(lp_thread_start(&d.thread, sock_path), 0);
    int srv = accept_within(listener, 1000);
    LP_ASSERT(srv >= 0);
    fcntl(srv, F_SETFL, fcntl(srv, F_GETFL) | O_NONBLOCK);
    lp_test_loop_run(50, connected);
    LP_ASSERT(connected());
    LP_ASSERT_EQ(lp_thread_stats(&d.thread), 0);
    const char *lanes[1] = { "personal" };
    LP_ASSERT_EQ(lp_thread_search(&d.thread, "bread", lanes, 1), 0);
    LP_ASSERT_EQ(lp_thread_graph(&d.thread, "Paris", NULL, NULL, 0, 2, 30, 1), 0);
    struct json_object *req = hear(srv);
    LP_ASSERT_STR(field(req, "type"), "stats");
    LP_ASSERT_STR(field(req, "source"), "desktop");
    json_object_put(req);
    req = hear(srv);
    LP_ASSERT_STR(field(req, "type"), "search");
    LP_ASSERT_STR(field(req, "query"), "bread");
    json_object_put(req);
    req = hear(srv);
    LP_ASSERT_STR(field(req, "type"), "graph");
    LP_ASSERT_STR(field(req, "entity"), "Paris");
    LP_ASSERT_STR(field(req, "hops"), "2");
    json_object_put(req);
    /* answers in order: the stats, an error for the search, the graph */
    say(srv, "{\"type\":\"stats.result\",\"documents\":3,\"entities\":5}\n");
    lp_test_loop_run(500, got_stats);
    LP_ASSERT(got_stats());
    struct json_object *stats = lp_thread_answer(&d.thread, LP_THREAD_STATS);
    LP_ASSERT(stats != NULL);
    LP_ASSERT_STR(field(stats, "documents"), "3");
    say(srv, "{\"type\":\"error\",\"code\":\"ENOSYS\",\"message\":\"no embedder\"}\n");
    lp_test_loop_run(500, got_error);
    LP_ASSERT(got_error());
    LP_ASSERT_STR(d.thread.error, "no embedder");
    LP_ASSERT_EQ(d.thread.error_kind, LP_THREAD_SEARCH);
    LP_ASSERT(lp_thread_answer(&d.thread, LP_THREAD_SEARCH) == NULL);
    changes = 0;
    say(srv, "{\"type\":\"graph.result\",\"entities\":[{\"id\":\"a\",\"name\":\"Paris\",\"kind\":\"place\",\"mention_count\":2,\"document_ids\":[\"d\"]},"
             "{\"id\":\"b\",\"name\":\"France\",\"kind\":\"place\",\"mention_count\":1,\"document_ids\":[]}],"
             "\"relationships\":[{\"id\":\"r\",\"subject_id\":\"a\",\"predicate\":\"capital of\",\"object_id\":\"b\",\"weight\":3}],\"entity_count\":2,\"relationship_count\":1}\n");
    lp_test_loop_run(500, NULL);
    LP_ASSERT(changes & LP_THREAD_CHANGED_GRAPH);
    LP_ASSERT_EQ(d.thread.pending_count, 0);
    /* threadd goes away and comes back */
    close(srv);
    close(listener);
    lp_test_loop_run(500, disconnected);
    LP_ASSERT(disconnected());
    LP_ASSERT(lp_thread_answer(&d.thread, LP_THREAD_STATS) != NULL);   /* the answers are kept */
    listener = listen_at();
    lp_test_loop_run(3000, connected);
    LP_ASSERT(connected());
    srv = accept_within(listener, 1000);
    close(srv);
    close(listener);
    lp_thread_free(&d.thread);
    unlink(sock_path);
}

LP_TEST(the_graph_lays_out_selects_and_finds_nodes) {
    lp_graph g;
    lp_graph_init(&g);
    struct json_object *answer = json_tokener_parse(
        "{\"entities\":[{\"id\":\"a\",\"name\":\"Paris\",\"kind\":\"place\",\"mention_count\":2},{\"id\":\"b\",\"name\":\"France\",\"kind\":\"place\",\"mention_count\":1},"
        "{\"id\":\"c\",\"name\":\"notes.txt\",\"kind\":\"file\",\"mention_count\":1}],"
        "\"relationships\":[{\"id\":\"r\",\"subject_id\":\"a\",\"predicate\":\"capital of\",\"object_id\":\"b\",\"weight\":3},{\"id\":\"x\",\"subject_id\":\"a\",\"predicate\":\"in\",\"object_id\":\"zzz\",\"weight\":1}],"
        "\"entity_count\":3,\"relationship_count\":2}");
    lp_graph_load(&g, answer, 0);
    LP_ASSERT_EQ(g.node_count, 3);
    LP_ASSERT_EQ(g.edge_count, 1);                     /* an edge to an unknown node is dropped */
    LP_ASSERT_EQ(g.total_entities, 3);
    LP_ASSERT_EQ(lp_graph_find(&g, "france"), 1);
    LP_ASSERT_EQ(lp_graph_find(&g, "nowhere"), -1);
    for (int i = 0; i < g.node_count; i++) LP_ASSERT(g.nodes[i].x > 0 && g.nodes[i].x < 1 && g.nodes[i].y > 0 && g.nodes[i].y < 1);
    /* connected nodes end up nearer than unconnected ones */
    float ab = hypotf(g.nodes[0].x - g.nodes[1].x, g.nodes[0].y - g.nodes[1].y);
    float ac = hypotf(g.nodes[0].x - g.nodes[2].x, g.nodes[0].y - g.nodes[2].y);
    LP_ASSERT(ab < ac);
    /* a click on a node selects it; the picture survives a reload with the selection kept */
    lp_rect canvas = LP_RECT(0, 0, 400, 400);
    float x, y;
    LP_ASSERT(lp_graph_project(&g, canvas, 0, &x, &y, NULL));
    LP_ASSERT_EQ(lp_graph_hit(&g, canvas, x, y), 0);
    LP_ASSERT_EQ(lp_graph_hit(&g, canvas, -50, -50), -1);
    g.selected = 1;
    lp_graph_load(&g, answer, 1);
    LP_ASSERT_EQ(g.selected, 1);
    lp_color place = lp_graph_kind_color("place"), other = lp_graph_kind_color("something-new");
    LP_ASSERT(place.r != other.r || place.g != other.g || place.b != other.b);
    json_object_put(answer);
}

static lp_ctx ctx;
static const lp_rect BODY = { 0, 0, 900, 560 };
static double now_ms = 1000;

static void pass(void *state, lp_input in) {
    now_ms += 50;
    lp_ctx_begin(&ctx, LP_PASS_EVENT, NULL, &in, BODY, now_ms);
    lp_app_thread.paint(state, &ctx, BODY, &d);
    lp_ctx_end(&ctx);
}

LP_TEST(the_threads_app_opens_on_a_tab_asks_threadd_and_centres_on_a_file) {
    fresh();
    memset(&ctx, 0, sizeof ctx);
    ctx.settings = &d.settings;
    ctx.active_window = 1;
    int listener = listen_at();
    LP_ASSERT(listener >= 0);
    LP_ASSERT_EQ(lp_thread_start(&d.thread, sock_path), 0);
    int srv = accept_within(listener, 1000);
    LP_ASSERT(srv >= 0);
    fcntl(srv, F_SETFL, fcntl(srv, F_GETFL) | O_NONBLOCK);
    lp_test_loop_run(50, connected);
    char id[12];
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "thread", "schemas", NULL, id), 1);
    void *state = lp_desktop_instance(&d, id)->state;
    LP_ASSERT(state != NULL);
    LP_ASSERT_EQ(lp_thread_app_tab(state), LP_THREAD_TAB_SCHEMAS);
    /* opening asked for everything; drain the requests */
    int asked_stats = 0, asked_schemas = 0, asked_graph = 0;
    for (int i = 0; i < 8; i++) {
        struct json_object *req = hear(srv);
        if (!req) break;
        const char *type = field(req, "type");
        asked_stats += strcmp(type, "stats") == 0;
        asked_schemas += strcmp(type, "schemas") == 0;
        asked_graph += strcmp(type, "graph") == 0;
        json_object_put(req);
    }
    LP_ASSERT(asked_stats >= 1 && asked_schemas >= 1 && asked_graph >= 1);
    /* View Thread on a file: the record is asked for, and its node seeds the graph */
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "thread", "graph:file=/home/mary/Documents/notes.txt", NULL, NULL), 1);
    LP_ASSERT_EQ(lp_thread_app_tab(state), LP_THREAD_TAB_GRAPH);
    LP_ASSERT_STR(lp_thread_app_file(state), "/home/mary/Documents/notes.txt");
    struct json_object *req = hear(srv);
    LP_ASSERT_STR(field(req, "type"), "file.record");
    LP_ASSERT_STR(field(req, "path"), "/home/mary/Documents/notes.txt");
    json_object_put(req);
    /* answer the outstanding requests in order, then the record */
    int outstanding = d.thread.pending_count;
    for (int i = 0; i < outstanding - 1; i++) say(srv, "{\"type\":\"error\",\"message\":\"not now\"}\n");
    say(srv, "{\"type\":\"file.record.result\",\"document\":{\"id\":\"file-1\",\"name\":\"notes.txt\"},\"partitions\":[{\"seq\":0}],"
             "\"file\":{\"path\":\"/home/mary/Documents/notes.txt\",\"kind\":\"text\",\"size\":10,\"mtime_ms\":1757700000000,\"content_hash\":\"abc\",\"seen_ms\":1757700000000},"
             "\"entities\":[{\"id\":\"f1\",\"name\":\"Documents\",\"kind\":\"folder\"},{\"id\":\"e1\",\"name\":\"Documents/notes.txt\",\"kind\":\"file\"}],\"relationships\":[],\"ledger\":[],\"enrich_state\":\"done\"}\n");
    lp_test_loop_run(500, NULL);
    LP_ASSERT_STR(lp_thread_app_seed(state), "Documents/notes.txt");
    req = hear(srv);
    LP_ASSERT_STR(field(req, "type"), "graph");
    LP_ASSERT_STR(field(req, "entity"), "Documents/notes.txt");
    json_object_put(req);
    say(srv, "{\"type\":\"graph.result\",\"entities\":[{\"id\":\"e1\",\"name\":\"Documents/notes.txt\",\"kind\":\"file\",\"mention_count\":1,\"document_ids\":[\"file-1\"]},"
             "{\"id\":\"f1\",\"name\":\"Documents\",\"kind\":\"folder\",\"mention_count\":9,\"document_ids\":[]}],"
             "\"relationships\":[{\"id\":\"r1\",\"subject_id\":\"e1\",\"predicate\":\"in\",\"object_id\":\"f1\",\"weight\":1}],\"entity_count\":2,\"relationship_count\":1}\n");
    lp_test_loop_run(500, NULL);
    LP_ASSERT_EQ(lp_thread_app_graph_nodes(state), 2);
    /* the tabs answer the View menu and paint without a crash */
    lp_app_thread.command(state, &d, LP_THREAD_TAB_LEDGER);
    LP_ASSERT_EQ(lp_thread_app_tab(state), LP_THREAD_TAB_LEDGER);
    for (int tab = 0; tab < 6; tab++) {
        lp_thread_app_set_tab(state, tab);
        pass(state, (lp_input){ .mx = NAN, .my = NAN });
    }
    lp_menu_model m = { .id = "view", .label = "View", .count = 0 };
    lp_app_thread.menu_entries(state, &d, LP_MENU_VIEW, &m);
    LP_ASSERT_EQ(m.count, 9);      /* six tabs, a separator, Reload, Reconcile */
    close(srv);
    close(listener);
    lp_thread_free(&d.thread);
    unlink(sock_path);
}

LP_TEST(the_finder_and_disk_utility_offer_the_thread) {
    fresh();
    lp_menu_model m = { .id = "file", .label = "File", .count = 0 };
    char id[12];
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "diskutil", NULL, NULL, id), 1);
    void *u = lp_desktop_instance(&d, id)->state;
    lp_app_diskutil.menu_entries(u, &d, LP_MENU_VIEW, &m);
    LP_ASSERT_EQ(m.count, 1);
    LP_ASSERT_STR(m.entries[0].label, "Show All Devices");
    LP_ASSERT(!lp_diskutil_show_all(u));
    lp_app_diskutil.command(u, &d, LP_DISKUTIL_SHOW_ALL);
    LP_ASSERT(lp_diskutil_show_all(u));
    m.count = 0;
    lp_app_diskutil.menu_entries(u, &d, LP_MENU_FILE, &m);
    LP_ASSERT_EQ(m.count, 9);
    LP_ASSERT_STR(m.entries[5].label, "View Thread");
    /* the Finder's commands open Threads on the right tab */
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "finder", NULL, NULL, id), 1);
    void *f = lp_desktop_instance(&d, id)->state;
    lp_app_finder.command(f, &d, LP_FINDER_THREAD_SCHEMAS);
    const lp_window_record *front = lp_wm_focused(&d.wm);
    LP_ASSERT(front != NULL);
    LP_ASSERT_STR(front->app_id, "thread");
    LP_ASSERT_EQ(lp_thread_app_tab(lp_desktop_instance(&d, front->id)->state), LP_THREAD_TAB_SCHEMAS);
    lp_thread_free(&d.thread);
}

LP_TEST(the_shared_mount_is_a_developers_volume) {
    LP_ASSERT(lp_files_volume_is_shared("/mnt/maryos-out"));
    LP_ASSERT(lp_files_volume_is_shared("/mnt/maryos-out/ui"));
    LP_ASSERT(!lp_files_volume_is_shared("/mnt/maryos-outer"));
    LP_ASSERT(!lp_files_volume_is_shared("/media/usb"));
    lp_volume vols[4];
    int n = lp_files_volumes_parse("/dev/vda2 / ext4 rw 0 0\n/dev/vdb /mnt/maryos-out ext4 rw 0 0\n", "MaryOS", vols, 4);
    LP_ASSERT_EQ(n, 2);                                /* parsing keeps it; lp_files_volumes hides it outside dev mode */
    LP_ASSERT_STR(vols[0].name, "MaryOS");
}

#else
LP_TEST(without_json_c_there_is_no_thread) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_test_loop_install(&d);
    LP_ASSERT_EQ(lp_thread_available(), 0);
    LP_ASSERT_EQ(lp_thread_start(&d.thread, "/nowhere"), -ENOSYS);
    LP_ASSERT_EQ(lp_thread_stats(&d.thread), -ENOSYS);
}
#endif

int main(void) {
#ifdef HAVE_JSONC
    LP_RUN(requests_go_out_as_lines_and_answers_come_back_by_kind);
    LP_RUN(the_graph_lays_out_selects_and_finds_nodes);
    LP_RUN(the_threads_app_opens_on_a_tab_asks_threadd_and_centres_on_a_file);
    LP_RUN(the_finder_and_disk_utility_offer_the_thread);
    LP_RUN(the_shared_mount_is_a_developers_volume);
#else
    LP_RUN(without_json_c_there_is_no_thread);
#endif
    if (dir[0]) rmdir(dir);
    LP_TEST_MAIN_END();
}
