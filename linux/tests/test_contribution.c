/* The highlights and "From the thread" (PARITY D26, D27): a reply's credits parsed from reply.end, the
 * brush stroke's determinism and palette, the passage painted with its strokes, a tap on one, and the
 * window it opens. */
#define _DARWIN_C_SOURCE 1
#include <cairo.h>
#include <math.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "lp_test.h"
#include "maryui/components/lp_spotlight_panel.h"
#include "maryui/lp_brush.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_mary.h"

static lp_desktop d;

LP_TEST(a_stroke_is_the_same_twice_and_owners_keep_their_colours) {
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 200, 60);
    cairo_t *cr = cairo_create(s);
    lp_rect r = LP_RECT(10, 10, 160, 24);
    lp_brush_path(cr, r, lp_brush_seed("mary#0-12", 0));
    double x1, y1, x2, y2;
    cairo_path_extents(cr, &x1, &y1, &x2, &y2);
    cairo_new_path(cr);
    lp_brush_path(cr, r, lp_brush_seed("mary#0-12", 0));
    double a1, b1, a2, b2;
    cairo_path_extents(cr, &a1, &b1, &a2, &b2);
    LP_ASSERT_NEAR(x1, a1, 1e-9);
    LP_ASSERT_NEAR(y2, b2, 1e-9);
    LP_ASSERT(x1 >= r.x - 1 && x2 <= r.x + r.w + 1);            /* the stroke spans its rectangle */
    LP_ASSERT(y1 >= r.y - r.h * 0.3f && y2 <= r.y + r.h * 1.3f);  /* and wobbles a little past it */
    cairo_new_path(cr);
    lp_brush_path(cr, r, lp_brush_seed("mary#0-12", 1));        /* another line: another stroke */
    cairo_path_extents(cr, &a1, &b1, &a2, &b2);
    LP_ASSERT(fabs(y1 - b1) > 1e-6 || fabs(y2 - b2) > 1e-6);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
    LP_ASSERT_EQ(lp_brush_hash(""), 5381);
    LP_ASSERT_EQ(lp_brush_hash("a"), 5381 * 33 + 'a');
    lp_color gold = lp_brush_palette(0);
    LP_ASSERT_NEAR(gold.r, 0.68f, 1e-6);
    lp_color one = lp_brush_color("thread-1|alice"), again = lp_brush_color("thread-1|alice");
    LP_ASSERT(one.r == again.r && one.g == again.g && one.b == again.b);
    LP_ASSERT_NEAR(lp_brush_opacity(0, 1000, 0, 0), 0, 1e-6);
    LP_ASSERT_NEAR(lp_brush_opacity(1350, 1000, 0, 0), 0.25f, 1e-6);   /* easeIn: t² at the half */
    LP_ASSERT_NEAR(lp_brush_opacity(1350, 1000, 1, 0), 0.0816f, 1e-3);  /* the second stroke starts 150 ms later */
    LP_ASSERT_NEAR(lp_brush_opacity(2000, 1000, 0, 0), 1, 1e-6);
    LP_ASSERT_NEAR(lp_brush_opacity(0, 1000, 0, 1), 1, 1e-6);          /* reduced motion: at once */
}

#ifdef HAVE_JSONC
#include <json-c/json.h>

static const char *CONTRIBUTION =
    "{\"owners\":[{\"thread_id\":\"t1\",\"owner_id\":\"alice\",\"document_ids\":[\"doc-a\",\"doc-b\"],\"influence\":{\"doc-a\":0.75,\"doc-b\":0.25},\"royalty\":0.6,"
    "\"spans\":[{\"lower\":38,\"upper\":72},{\"lower\":0,\"upper\":36}]},"
    "{\"thread_id\":\"t2\",\"owner_id\":\"bob\",\"document_ids\":[\"doc-c\"],\"influence\":{\"doc-c\":1},\"royalty\":0.4,\"spans\":[{\"lower\":73,\"upper\":110}]}]}";
static const char *RETRIEVED =
    "[{\"document_id\":\"doc-a\",\"group_id\":\"files-alice\",\"name\":\"gauge-theory.txt\",\"family\":\"file\",\"lane\":\"personal\",\"score\":3.1},"
    "{\"document_id\":\"doc-c\",\"group_id\":\"shared\",\"name\":\"music.txt\",\"family\":\"file\",\"lane\":\"personal\",\"score\":4.4}]";
static const char *REPLY = "You explored gauge theory in real depth. It became a way of seeing fields. Someone compared the whole thing to music.";

static void credit(lp_mary_message *msg) {
    struct json_object *c = json_tokener_parse(CONTRIBUTION), *r = json_tokener_parse(RETRIEVED);
    lp_mary_message_credit(msg, c, r);
    json_object_put(c);
    json_object_put(r);
}

LP_TEST(reply_end_credits_the_reply_with_owners_and_spans_in_reading_order) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    lp_mary_feed(&d.mary, "{\"type\":\"reply.delta\",\"text\":\"You explored gauge theory in real depth.\"}\n", 0);
    char line[1024];
    snprintf(line, sizeof line, "{\"type\":\"reply.delta\",\"text\":\"You explored gauge theory in real depth.\"}\n");
    lp_mary_feed(&d.mary, line, strlen(line));
    snprintf(line, sizeof line, "{\"type\":\"reply.end\",\"cancelled\":false,\"contribution\":%s,\"retrieved\":%s}\n", CONTRIBUTION, RETRIEVED);
    lp_mary_feed(&d.mary, line, strlen(line));
    LP_ASSERT_EQ(d.mary.message_count, 1);
    lp_mary_message *msg = &d.mary.messages[0];
    LP_ASSERT_EQ(msg->streaming, 0);
    LP_ASSERT_EQ(msg->owner_count, 2);
    LP_ASSERT_STR(msg->owners[0].owner_id, "alice");
    LP_ASSERT_STR(msg->owners[0].id, "t1|alice");
    LP_ASSERT_NEAR(msg->owners[0].royalty, 0.6f, 1e-6);
    LP_ASSERT_EQ(msg->owners[0].documents, 2);
    LP_ASSERT_EQ(msg->span_count, 3);
    LP_ASSERT_EQ(msg->spans[0].lower, 0);                 /* sorted into reading order */
    LP_ASSERT_EQ(msg->spans[1].lower, 38);
    LP_ASSERT_EQ(msg->spans[2].owner, 1);
    LP_ASSERT_EQ(lp_mary_span_owner_at(msg, 5), 0);
    LP_ASSERT_EQ(lp_mary_span_owner_at(msg, 80), 1);
    LP_ASSERT_EQ(lp_mary_span_owner_at(msg, 72), -1);
    LP_ASSERT(msg->contribution != NULL && msg->retrieved != NULL);
    lp_mary_free(&d.mary);
}

static lp_ctx ctx;
static cairo_surface_t *surface;
static cairo_t *cr;
static lp_spotlight_view view;
static float px, py;

/* One pass over the panel at its resting place; returns the result. */
static lp_spotlight_result pass(enum lp_pass which, lp_input in, double now) {
    lp_ctx_begin(&ctx, which, which == LP_PASS_DRAW ? cr : NULL, &in, LP_RECT(0, 0, 1280, 800), now);
    lp_spotlight_result res;
    lp_spotlight_panel(&ctx, px, py, &view, &res);
    lp_ctx_end(&ctx);
    return res;
}

LP_TEST(the_passage_paints_its_strokes_and_a_tap_opens_from_the_thread) {
    lp_desktop_init(&d, LP_RECT(0, 0, 1280, 800), NULL);
    lp_desktop_register_builtin_apps(&d);
    lp_spotlight_open(&d.spotlight);
    d.spotlight_chat = 1;
    d.mary.fd = 0;
    d.mary.key_present = 1;
    lp_mary_feed(&d.mary, "{\"type\":\"transcript\",\"text\":\"Tell me about my notes.\",\"final\":true}\n", 0);
    char line[1200];
    snprintf(line, sizeof line, "{\"type\":\"transcript\",\"text\":\"Tell me about my notes.\",\"final\":true}\n");
    lp_mary_feed(&d.mary, line, strlen(line));
    snprintf(line, sizeof line, "{\"type\":\"reply.delta\",\"text\":\"%s\"}\n", REPLY);
    lp_mary_feed(&d.mary, line, strlen(line));
    lp_mary_message *msg = &d.mary.messages[1];
    credit(msg);
    msg->streaming = 0;                                          /* the reply is whole (reply.end did this) */
    LP_ASSERT_EQ(msg->span_count, 3);
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1280, 800);
    cr = cairo_create(surface);
    memset(&ctx, 0, sizeof ctx);
    ctx.settings = &d.settings;
    ctx.active_window = 1;
    view = lp_desktop_spotlight_view(&d, NULL, 0);
    view.mary = &d.mary;
    view.chat = 1;
    lp_size size = lp_spotlight_max_size(&view);
    px = (1280 - size.w) / 2;
    py = 800 * LP_SPOTLIGHT_Y_FRACTION - LP_SIZE_SPOTLIGHT_BAR_HEIGHT / 2 - LP_SPOTLIGHT_PAD;
    /* the first draw starts the strokes' fade, which asks for frames until it is done */
    lp_input none = { .mx = NAN, .my = NAN };
    pass(LP_PASS_DRAW, none, 1000);
    LP_ASSERT(msg->highlighted_ms > 0);
    LP_ASSERT(ctx.wants_frame);
    pass(LP_PASS_DRAW, none, 1000 + 700 + 32 * 150 + 10);        /* a span wraps over several lines: one stroke each */
    LP_ASSERT(!ctx.wants_frame);                                 /* every stroke is in */
    /* under reduced motion nothing animates */
    d.settings.reduced_motion = 1;
    msg->highlighted_ms = 0;
    pass(LP_PASS_DRAW, none, 5000);
    LP_ASSERT(!ctx.wants_frame);
    d.settings.reduced_motion = 0;
    /* find a credited word by sweeping the well with the pointer: the hand shows over a stroke */
    lp_rect panel = pass(LP_PASS_EVENT, none, 5100).panel;
    float hx = -1, hy = -1;
    for (float y = panel.y + LP_SIZE_SPOTLIGHT_BAR_HEIGHT + 40; y < panel.y + panel.h && hx < 0; y += 10) {
        for (float x = panel.x + 30; x < panel.x + panel.w - 30; x += 12) {
            lp_input hover = { .mx = x, .my = y };
            pass(LP_PASS_EVENT, hover, 5100);
            if (ctx.cursor == LP_CURSOR_POINTER) { hx = x; hy = y; break; }
        }
    }
    LP_ASSERT(hx >= 0);
    lp_input press = { .mx = hx, .my = hy, .buttons = LP_BUTTON_LEFT, .pressed = LP_BUTTON_LEFT };
    lp_spotlight_result res = pass(LP_PASS_EVENT, press, 5200);
    LP_ASSERT_EQ(res.contribution_message, 1);
    LP_ASSERT(res.contribution_owner == 0 || res.contribution_owner == 1);
    /* the window it opens: the owner's contribution and sources by influence */
    char target[32], id[12];
    snprintf(target, sizeof target, "m%d:o%d", res.contribution_message, 0);
    LP_ASSERT_EQ(lp_desktop_open_app_with(&d, "contribution", target, NULL, id), 1);
    void *c = lp_desktop_instance(&d, id)->state;
    LP_ASSERT(c != NULL);
    LP_ASSERT_EQ(lp_contribution_owner(c), 0);
    LP_ASSERT_NEAR(lp_contribution_royalty(c), 0.6f, 1e-6);
    LP_ASSERT_EQ(lp_contribution_sources(c), 2);
    LP_ASSERT_STR(lp_contribution_source_name(c, 0), "gauge-theory.txt");    /* named by the retrieved list, first by influence */
    LP_ASSERT(strncmp(lp_contribution_source_name(c, 1), "Document ", 9) == 0);
    /* threadd answers the previews */
    const char *docs = "{\"type\":\"documents.result\",\"documents\":[{\"id\":\"doc-b\",\"name\":\"fibre bundles.md\",\"group_label\":\"Files\",\"texts\":[\"A bundle is a space that looks locally like a product.\"]}]}\n";
    d.thread.fd = 0;
    lp_thread_feed(&d.thread, docs, strlen(docs));
    LP_ASSERT_STR(lp_contribution_source_name(c, 1), "fibre bundles.md");
    LP_ASSERT(strstr(lp_contribution_source_preview(c, 1), "locally like a product") != NULL);
    /* it paints */
    lp_rect body = LP_RECT(0, 0, 440, 440);
    lp_ctx_begin(&ctx, LP_PASS_DRAW, cr, &none, body, 6000);
    lp_app_contribution.paint(c, &ctx, body, &d);
    lp_ctx_end(&ctx);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    lp_mary_free(&d.mary);
    lp_thread_free(&d.thread);
}
#endif

int main(void) {
    LP_RUN(a_stroke_is_the_same_twice_and_owners_keep_their_colours);
#ifdef HAVE_JSONC
    LP_RUN(reply_end_credits_the_reply_with_owners_and_spans_in_reading_order);
    LP_RUN(the_passage_paints_its_strokes_and_a_tap_opens_from_the_thread);
#endif
    LP_TEST_MAIN_END();
}
