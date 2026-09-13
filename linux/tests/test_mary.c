/* The desktop's client for maryd, against a fake maryd on a unix socket: hello and
 * lines split across reads, a turn streaming into the conversation, the 24-message
 * window, the line cap, requests as JSON lines, the key zeroed once sent, skill calls,
 * and reconnecting when maryd comes back. Runs on a Mac as well as Linux. */
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "lp_test.h"
#include "lp_test_loop.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_mary.h"

static lp_desktop d;
static unsigned changes;
static int wakes;
static char dir[64], sock_path[100];     /* fits sockaddr_un.sun_path */

static void on_mary(lp_desktop *desk, unsigned what) {
    changes |= what;
    if (what & LP_MARY_WAKE) wakes++;
}

static void fresh(int with_loop) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    if (with_loop) lp_test_loop_install(&d);
    d.on_mary = on_mary;
    changes = 0;
    wakes = 0;
}

#ifdef HAVE_JSONC
#include <json-c/json.h>

static int listen_at(void) {
    if (!dir[0]) {
        snprintf(dir, sizeof dir, "/tmp/lp-mary-XXXXXX");
        if (!mkdtemp(dir)) return -1;
        snprintf(sock_path, sizeof sock_path, "%s/mary.sock", dir);
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

/* Writes it all. A Mac's unix sockets buffer only a few kilobytes, so when the socket is
 * full the client's loop runs to drain it, rather than both sides waiting on each other. */
static void say(int fd, const char *text) {
    size_t n = strlen(text);
    while (n) {
        ssize_t w = write(fd, text, n);
        if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            lp_test_loop_run(10, NULL);
            continue;
        }
        if (w <= 0) return;
        text += w;
        n -= (size_t)w;
    }
}

/* One line from the client, parsed; NULL after a second. */
static struct json_object *hear(int fd) {
    char line[4096];
    size_t n = 0;
    struct pollfd p = { .fd = fd, .events = POLLIN };
    while (n < sizeof line - 1 && poll(&p, 1, 1000) > 0) {
        if (read(fd, line + n, 1) != 1) break;
        if (line[n] == '\n') {
            line[n] = 0;
            return json_tokener_parse(line);
        }
        n++;
    }
    return NULL;
}

static const char *field(struct json_object *o, const char *key) {
    struct json_object *v;
    return o && json_object_object_get_ex(o, key, &v) ? json_object_get_string(v) : "";
}

static int want_messages;
static int enough_messages(void) { return d.mary.message_count >= want_messages; }
static lp_mary_state want_state;
static int reached_state(void) { return d.mary.state == want_state; }
static int disconnected(void) { return !lp_mary_connected(&d.mary); }

/* A connected client and the fake maryd's end of it. */
static int connect_pair(int *listener) {
    *listener = listen_at();
    LP_ASSERT(*listener >= 0);
    LP_ASSERT_EQ(lp_mary_start(&d.mary, sock_path), 0);
    int srv = accept_within(*listener, 1000);
    if (srv >= 0) fcntl(srv, F_SETFL, fcntl(srv, F_GETFL) | O_NONBLOCK);
    LP_ASSERT(srv >= 0);
    LP_ASSERT(lp_mary_connected(&d.mary));
    return srv;
}

LP_TEST(reads_hello_even_when_it_arrives_in_pieces) {
    fresh(1);
    int listener, srv = connect_pair(&listener);
    say(srv, "{\"type\":\"hello\",\"state\":\"idle\",\"key_present\":true,\"wake\":true,\"tail\":[{\"role\":\"user\",");
    lp_test_loop_run(50, NULL);
    LP_ASSERT_EQ(d.mary.message_count, 0);
    say(srv, "\"content\":\"Hi\"},{\"role\":\"assistant\",\"content\":\"Hello.\"}]}\n");
    want_messages = 2;
    lp_test_loop_run(1000, enough_messages);
    LP_ASSERT_EQ(d.mary.message_count, 2);
    LP_ASSERT(d.mary.key_present && d.mary.wake);
    LP_ASSERT(d.mary.messages[1].role == LP_MARY_REPLY);
    LP_ASSERT_STR(d.mary.messages[1].text, "Hello.");
    LP_ASSERT(changes & LP_MARY_CHANGED_CONNECTION);
    lp_mary_free(&d.mary);
    close(srv);
    close(listener);
}

LP_TEST(a_turn_streams_into_the_conversation) {
    fresh(1);
    int listener, srv = connect_pair(&listener);
    say(srv, "{\"type\":\"wake\"}\n{\"type\":\"state\",\"state\":\"listening\"}\n{\"type\":\"level\",\"rms\":0.2}\n"
             "{\"type\":\"transcript\",\"text\":\"what's the cap\",\"final\":false}\n");
    want_state = LP_MARY_LISTENING;
    lp_test_loop_run(1000, reached_state);
    lp_test_loop_run(50, NULL);
    LP_ASSERT_EQ(wakes, 1);
    LP_ASSERT_NEAR(d.mary.level, 0.2, 1e-6);
    LP_ASSERT_STR(d.mary.partial, "what's the cap");
    LP_ASSERT(lp_mary_active(&d.mary));

    say(srv, "{\"type\":\"transcript\",\"text\":\"what's the capital of France?\",\"final\":true}\n"
             "{\"type\":\"state\",\"state\":\"thinking\"}\n{\"type\":\"reply.delta\",\"text\":\"Paris is\"}\n"
             "{\"type\":\"reply.delta\",\"text\":\" the capital.\"}\n{\"type\":\"state\",\"state\":\"speaking\"}\n");
    want_state = LP_MARY_SPEAKING;
    lp_test_loop_run(1000, reached_state);
    LP_ASSERT_EQ(d.mary.message_count, 2);
    LP_ASSERT_STR(d.mary.partial, "");
    LP_ASSERT_STR(d.mary.messages[1].text, "Paris is the capital.");
    LP_ASSERT(d.mary.messages[1].streaming);
    say(srv, "{\"type\":\"reply.end\",\"cancelled\":false}\n{\"type\":\"state\",\"state\":\"idle\"}\n");
    want_state = LP_MARY_IDLE;
    lp_test_loop_run(1000, reached_state);
    LP_ASSERT(!d.mary.messages[1].streaming);
    LP_ASSERT(!lp_mary_active(&d.mary));

    say(srv, "{\"type\":\"error\",\"stage\":\"key\",\"message\":\"No Mistral key yet.\"}\n");
    changes = 0;
    lp_test_loop_run(200, NULL);
    LP_ASSERT(changes & LP_MARY_CHANGED_ERROR);
    LP_ASSERT_STR(d.mary.error, "No Mistral key yet.");
    lp_mary_free(&d.mary);
    close(srv);
    close(listener);
}

LP_TEST(keeps_the_last_24_messages_and_skips_a_line_past_the_cap) {
    fresh(1);
    int listener, srv = connect_pair(&listener);
    char line[128];
    for (int i = 0; i < 30; i++) {
        snprintf(line, sizeof line, "{\"type\":\"transcript\",\"text\":\"q%d\",\"final\":true}\n", i);
        say(srv, line);
    }
    want_messages = 24;
    lp_test_loop_run(1000, NULL);
    LP_ASSERT_EQ(d.mary.message_count, 24);
    LP_ASSERT_STR(d.mary.messages[0].text, "q6");
    LP_ASSERT_STR(d.mary.messages[23].text, "q29");

    char *huge = malloc(70000);
    memset(huge, 'x', 69998);
    huge[0] = '{';
    huge[69998] = '\n';
    huge[69999] = 0;
    say(srv, huge);
    free(huge);
    say(srv, "{\"type\":\"state\",\"state\":\"thinking\"}\n");
    want_state = LP_MARY_THINKING;
    lp_test_loop_run(2000, reached_state);
    LP_ASSERT_EQ(d.mary.state, LP_MARY_THINKING);
    LP_ASSERT_EQ(d.mary.message_count, 24);
    lp_mary_free(&d.mary);
    close(srv);
    close(listener);
}

LP_TEST(requests_arrive_as_json_lines) {
    fresh(1);
    int listener, srv = connect_pair(&listener);
    LP_ASSERT_EQ(lp_mary_ask(&d.mary, "say \"hi\"\nplease"), 0);
    struct json_object *o = hear(srv);
    LP_ASSERT_STR(field(o, "type"), "ask");
    LP_ASSERT_STR(field(o, "text"), "say \"hi\"\nplease");
    json_object_put(o);
    LP_ASSERT_EQ(lp_mary_listen(&d.mary), 0);
    o = hear(srv);
    LP_ASSERT_STR(field(o, "type"), "listen");
    json_object_put(o);
    LP_ASSERT_EQ(lp_mary_set_wake(&d.mary, 0), 0);
    o = hear(srv);
    LP_ASSERT_STR(field(o, "type"), "config");
    LP_ASSERT_STR(field(o, "wake"), "false");
    json_object_put(o);
    LP_ASSERT_EQ(lp_mary_stop(&d.mary), 0);
    o = hear(srv);
    LP_ASSERT_STR(field(o, "type"), "stop");
    json_object_put(o);
    LP_ASSERT_EQ(lp_mary_ask(&d.mary, ""), -EINVAL);
    LP_ASSERT_EQ(lp_mary_send_line(&d.mary, "{\"a\":1}\n{}"), -EINVAL);
    lp_mary_free(&d.mary);
    close(srv);
    close(listener);
}

LP_TEST(the_key_is_sent_once_and_every_copy_is_zeroed) {
    fresh(1);
    int listener, srv = connect_pair(&listener);
    char key[] = "abcDEF1234567890ghij";
    size_t len = strlen(key);
    LP_ASSERT_EQ(lp_mary_set_key(&d.mary, key, len), 0);
    for (size_t i = 0; i < len; i++) LP_ASSERT_EQ(key[i], 0);
    struct json_object *o = hear(srv);
    LP_ASSERT_STR(field(o, "type"), "key.set");
    LP_ASSERT_STR(field(o, "key"), "abcDEF1234567890ghij");
    json_object_put(o);
    LP_ASSERT_EQ(d.mary.out_len, 0);
    int clean = 1;
    for (size_t i = 0; d.mary.out && i < d.mary.out_cap; i++) clean &= d.mary.out[i] == 0;
    LP_ASSERT(clean);

    char bad[] = "abc-def\"}";
    LP_ASSERT_EQ(lp_mary_set_key(&d.mary, bad, strlen("abc-def\"}")), -EINVAL);
    LP_ASSERT_EQ(bad[0], 0);
    lp_mary_free(&d.mary);
    close(srv);
    close(listener);
}

static char invoked[256];
static void on_skill(lp_desktop *desk, const char *call_id, const char *app, const char *skill, const char *args) {
    snprintf(invoked, sizeof invoked, "%s %s %s %s", call_id, app, skill, args);
}
static int was_invoked(void) { return invoked[0] != 0; }

LP_TEST(skill_calls_reach_the_handler_or_are_answered_unknown) {
    fresh(1);
    int listener, srv = connect_pair(&listener);
    say(srv, "{\"type\":\"skill.invoke\",\"call_id\":\"c1\",\"app\":\"settings\",\"skill\":\"open_pane\",\"args\":{\"pane\":\"sound\"}}\n");
    lp_test_loop_run(200, NULL);
    struct json_object *o = hear(srv);
    LP_ASSERT_STR(field(o, "type"), "skill.result");
    LP_ASSERT_STR(field(o, "call_id"), "c1");
    LP_ASSERT_STR(field(o, "error"), "unknown");
    json_object_put(o);

    invoked[0] = 0;
    d.mary.on_skill_invoke = on_skill;
    say(srv, "{\"type\":\"skill.invoke\",\"call_id\":\"c2\",\"app\":\"settings\",\"skill\":\"open_pane\",\"args\":{\"pane\":\"sound\"}}\n");
    lp_test_loop_run(1000, was_invoked);
    LP_ASSERT_STR(invoked, "c2 settings open_pane {\"pane\":\"sound\"}");
    lp_mary_free(&d.mary);
    close(srv);
    close(listener);
}

LP_TEST(reconnects_when_maryd_comes_back) {
    fresh(1);
    int listener, srv = connect_pair(&listener);
    close(srv);
    close(listener);
    unlink(sock_path);
    changes = 0;
    lp_test_loop_run(1000, disconnected);
    LP_ASSERT(!lp_mary_connected(&d.mary));
    LP_ASSERT(changes & LP_MARY_CHANGED_CONNECTION);
    LP_ASSERT_EQ(lp_mary_ask(&d.mary, "anyone?"), -ENOTCONN);

    lp_test_loop_run(600, NULL);                   /* a retry or two finds nothing */
    listener = listen_at();
    srv = -1;
    for (int i = 0; i < 100 && srv < 0; i++) {
        lp_test_loop_run(50, NULL);
        srv = accept_within(listener, 0);
    }
    LP_ASSERT(srv >= 0);
    lp_test_loop_run(50, NULL);
    LP_ASSERT(lp_mary_connected(&d.mary));
    lp_mary_free(&d.mary);
    close(srv);
    close(listener);
    unlink(sock_path);
    rmdir(dir);
}

LP_TEST(starting_needs_an_event_loop) {
    fresh(0);
    LP_ASSERT_EQ(lp_mary_start(&d.mary, "/tmp/nowhere.sock"), -ENOSYS);
    LP_ASSERT(lp_mary_available());
}
#endif

LP_TEST(without_json_c_there_is_no_mary) {
    fresh(0);
#ifndef HAVE_JSONC
    LP_ASSERT(!lp_mary_available());
    LP_ASSERT_EQ(lp_mary_start(&d.mary, "/tmp/nowhere.sock"), -ENOSYS);
#endif
    LP_ASSERT_EQ(d.mary.fd, -1);
    LP_ASSERT_EQ(lp_mary_ask(&d.mary, "hello"), -ENOTCONN);
}

int main(void) {
#ifdef HAVE_JSONC
    LP_RUN(reads_hello_even_when_it_arrives_in_pieces);
    LP_RUN(a_turn_streams_into_the_conversation);
    LP_RUN(keeps_the_last_24_messages_and_skips_a_line_past_the_cap);
    LP_RUN(requests_arrive_as_json_lines);
    LP_RUN(the_key_is_sent_once_and_every_copy_is_zeroed);
    LP_RUN(skill_calls_reach_the_handler_or_are_answered_unknown);
    LP_RUN(reconnects_when_maryd_comes_back);
    LP_RUN(starting_needs_an_event_loop);
#endif
    LP_RUN(without_json_c_there_is_no_mary);
    LP_TEST_MAIN_END();
}
