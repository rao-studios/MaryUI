/* Mary in Spotlight (Linux, PARITY D18): the keys that ask her and move through the
 * conversation, what reaches maryd, the panel's fixed height, and drawing every state.
 * maryd is a socketpair standing behind the desktop's lp_mary. C-only. */
#define _DARWIN_C_SOURCE 1
#include <cairo.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "lp_test.h"
#include "maryui/components/lp_spotlight_panel.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_mary.h"

#define CTRL 4

static lp_desktop d;
static int maryd = -1;          /* maryd's end of the socket */

static void setup(int connected) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    lp_spotlight_open(&d.spotlight);
    maryd = -1;
    int sv[2];
    if (connected && socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0) {
        d.mary.fd = sv[0];
        maryd = sv[1];
    }
}

static void teardown(void) {
    lp_mary_free(&d.mary);
    if (maryd >= 0) close(maryd);
}

/* Everything the desktop has sent maryd so far. */
static const char *sent(void) {
    static char buf[4096];
    ssize_t n = maryd >= 0 ? recv(maryd, buf, sizeof buf - 1, MSG_DONTWAIT) : -1;
    buf[n > 0 ? n : 0] = 0;
    return buf;
}

static void say(lp_mary_role role, const char *text) {
    lp_mary_message *m = &d.mary.messages[d.mary.message_count++];
    m->role = role;
    m->text = strdup(text);
    m->len = strlen(text);
}

LP_TEST(ctrl_return_asks_mary_and_opens_the_conversation) {
    setup(1);
    if (!lp_mary_available()) { teardown(); return; }
    lp_spotlight_set_query(&d.spotlight, "tell me a joke");
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Return, CTRL), 1);
    LP_ASSERT(d.spotlight.open);
    LP_ASSERT(d.spotlight_chat);
    LP_ASSERT_STR(d.spotlight.query.text, "");
    const char *out = sent();
    LP_ASSERT(strstr(out, "\"type\":\"ask\"") && strstr(out, "\"text\":\"tell me a joke\""));
    teardown();
}

LP_TEST(a_blank_bar_opens_the_microphone) {
    setup(1);
    if (!lp_mary_available()) { teardown(); return; }
    LP_ASSERT_EQ(lp_desktop_ask_mary(&d), 1);
    LP_ASSERT(d.spotlight_chat);
    LP_ASSERT(strstr(sent(), "\"type\":\"listen\"") != NULL);
    d.mary.state = LP_MARY_LISTENING;            /* pressed again while she listens: stop */
    lp_desktop_ask_mary(&d);
    LP_ASSERT(strstr(sent(), "\"type\":\"stop\"") != NULL);
    teardown();
}

LP_TEST(plain_return_still_opens_the_selection) {
    setup(1);
    lp_spotlight_set_query(&d.spotlight, "finder");
    int before = d.instance_count;
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Return, 0), 1);
    LP_ASSERT(!d.spotlight.open);
    LP_ASSERT(!d.spotlight_chat);
    LP_ASSERT_EQ(d.instance_count, before + 1);
    LP_ASSERT_STR(sent(), "");
    teardown();
}

LP_TEST(in_the_conversation_return_asks_and_the_arrows_scroll) {
    setup(1);
    if (!lp_mary_available()) { teardown(); return; }
    d.spotlight_chat = 1;
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Return, 0), 1);      /* a blank bar: nothing to ask */
    LP_ASSERT_STR(sent(), "");
    lp_spotlight_set_query(&d.spotlight, "and tomorrow?");
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Return, 0), 1);
    LP_ASSERT(strstr(sent(), "\"text\":\"and tomorrow?\"") != NULL);
    d.spotlight_chat_scroll.y = 100;
    int selection = d.spotlight.selection;
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Up, 0), 1);
    LP_ASSERT_NEAR(d.spotlight_chat_scroll.y, 100 - LP_SPOTLIGHT_CHAT_STEP, 0.01);
    lp_desktop_key(&d, XKB_KEY_Up, 0);
    lp_desktop_key(&d, XKB_KEY_Up, 0);
    LP_ASSERT_NEAR(d.spotlight_chat_scroll.y, 0, 0.01);
    lp_desktop_key(&d, XKB_KEY_Down, 0);
    LP_ASSERT_NEAR(d.spotlight_chat_scroll.y, LP_SPOTLIGHT_CHAT_STEP, 0.01);
    LP_ASSERT_EQ(d.spotlight.selection, selection);
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_a, 0), 0);           /* letters are the bar's */
    teardown();
}

LP_TEST(escape_stops_mary_first_and_then_closes_keeping_the_conversation) {
    setup(1);
    if (!lp_mary_available()) { teardown(); return; }
    d.spotlight_chat = 1;
    say(LP_MARY_USER, "What's the capital of France?");
    say(LP_MARY_REPLY, "Paris.");
    d.mary.state = LP_MARY_SPEAKING;
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Escape, 0), 1);
    LP_ASSERT(d.spotlight.open);
    LP_ASSERT(strstr(sent(), "\"type\":\"stop\"") != NULL);
    d.mary.state = LP_MARY_IDLE;
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Escape, 0), 1);
    LP_ASSERT(!d.spotlight.open);
    LP_ASSERT(!d.spotlight_chat);
    LP_ASSERT_EQ(d.mary.message_count, 2);
    teardown();
}

LP_TEST(closing_spotlight_while_mary_speaks_dismisses_her) {
    setup(1);
    if (!lp_mary_available()) { teardown(); return; }
    d.spotlight_chat = 1;
    d.mary.state = LP_MARY_SPEAKING;
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_space, CTRL), 1);
    LP_ASSERT(!d.spotlight.open);
    LP_ASSERT(!d.spotlight_chat);
    LP_ASSERT(strstr(sent(), "\"type\":\"dismiss\"") != NULL);
    teardown();
}

LP_TEST(what_was_typed_stays_in_the_bar_while_maryd_is_away) {
    setup(0);
    if (!lp_mary_available()) { teardown(); return; }
    lp_spotlight_set_query(&d.spotlight, "hello?");
    LP_ASSERT_EQ(lp_desktop_key(&d, XKB_KEY_Return, CTRL), 1);
    LP_ASSERT(d.spotlight_chat);
    LP_ASSERT_STR(d.spotlight.query.text, "hello?");
    teardown();
}

LP_TEST(the_conversation_is_one_fixed_height) {
    setup(0);
    lp_spotlight_view view = lp_desktop_spotlight_view(&d, NULL, 0);
    view.mary = &d.mary;
    view.chat = 1;
    float want = 3 * LP_SPOTLIGHT_PAD + LP_SIZE_SPOTLIGHT_BAR_HEIGHT + LP_SIZE_SPOTLIGHT_CHAT;
    LP_ASSERT_NEAR(lp_spotlight_measure(&view).h, want, 0.01);
    LP_ASSERT_NEAR(lp_spotlight_max_size(&view).h, want, 0.01);
    for (int i = 0; i < 12; i++) say(i % 2 ? LP_MARY_REPLY : LP_MARY_USER, "A long enough line to wrap across the column more than once, and then some.");
    LP_ASSERT_NEAR(lp_spotlight_measure(&view).h, want, 0.01);
    view.chat = 0;
    LP_ASSERT(lp_spotlight_measure(&view).h != want);
    teardown();
}

/* Draws the panel once; returns whether it asked for another frame. */
static int draw(const lp_spotlight_view *view, double now_ms) {
    lp_size size = lp_spotlight_max_size(view);
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)size.w + 20, (int)size.h + 20);
    cairo_t *cr = cairo_create(s);
    lp_ctx ctx = { 0 };
    ctx.settings = &d.settings;
    ctx.active_window = 1;
    ctx.focus = LP_SPOTLIGHT_QUERY_ID;
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, NULL, LP_RECT(0, 0, size.w + 20, size.h + 20), now_ms);
    lp_spotlight_panel(&ctx, 10, 10, view, NULL);
    int wants = ctx.wants_frame;
    lp_ctx_end(&ctx);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
    return wants;
}

LP_TEST(every_state_draws_and_only_a_busy_mary_animates) {
    setup(1);
    lp_spotlight_view view = lp_desktop_spotlight_view(&d, NULL, 0);
    view.mary = &d.mary;
    view.chat = 1;
    d.mary.key_present = 1;
    LP_ASSERT_EQ(draw(&view, 0), 0);                             /* empty and idle */
    say(LP_MARY_USER, "What's the weather like in Lyon this weekend?");
    for (lp_mary_state s = LP_MARY_IDLE; s <= LP_MARY_ERROR; s++) {
        d.mary.state = s;
        int wants = draw(&view, 1200);
        if (lp_mary_active(&d.mary) && !wants) LP_FAIL("%s did not animate", lp_mary_state_name(s));
    }
    d.mary.state = LP_MARY_THINKING;
    say(LP_MARY_REPLY, "Mostly sunny on Saturday");
    d.mary.messages[1].streaming = 1;
    LP_ASSERT(draw(&view, 400));
    d.mary.state = LP_MARY_IDLE;
    d.mary.messages[1].streaming = 0;
    snprintf(d.mary.partial, sizeof d.mary.partial, "and on Sunday");
    LP_ASSERT_EQ(draw(&view, 400), 0);
    teardown();
}

LP_TEST(a_reply_that_was_not_spoken_says_so_under_it) {
    setup(1);
    lp_spotlight_view view = lp_desktop_spotlight_view(&d, NULL, 0);
    view.mary = &d.mary;
    view.chat = 1;
    d.mary.key_present = 1;
    say(LP_MARY_USER, "Say something.");
    say(LP_MARY_REPLY, "Here is something.");
    d.mary.messages[1].note = strdup("Mistral refused to speak it (HTTP 403)");
    LP_ASSERT_EQ(draw(&view, 400), 0);                             /* drawn under the reply, and nothing to animate */
    teardown();
}

int main(void) {
    LP_RUN(ctrl_return_asks_mary_and_opens_the_conversation);
    LP_RUN(a_blank_bar_opens_the_microphone);
    LP_RUN(plain_return_still_opens_the_selection);
    LP_RUN(in_the_conversation_return_asks_and_the_arrows_scroll);
    LP_RUN(escape_stops_mary_first_and_then_closes_keeping_the_conversation);
    LP_RUN(closing_spotlight_while_mary_speaks_dismisses_her);
    LP_RUN(what_was_typed_stays_in_the_bar_while_maryd_is_away);
    LP_RUN(the_conversation_is_one_fixed_height);
    LP_RUN(every_state_draws_and_only_a_busy_mary_animates);
    LP_RUN(a_reply_that_was_not_spoken_says_so_under_it);
    LP_TEST_MAIN_END();
}
