/* Spotlight's conversation with Mary (Linux, PARITY D18) — dialogue on paper, Mary's
 * macOS session window brought into the panel: what you said is a small line behind a
 * thin accent rule, what she answers is body text under it, older exchanges fade, and
 * a status row over the well says what she is doing. The panel is size.spotlight-chat
 * tall under the bar however long the conversation grows; the well scrolls and stays
 * on the newest exchange unless it was scrolled away. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/components/lp_layout_components.h"
#include "maryui/components/lp_liquid_bubble.h"
#include "maryui/components/lp_monogram.h"
#include "maryui/lp_brush.h"
#include "maryui/lp_bubble.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_shadow.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"
#include "spotlight_chat.h"

#define WELL_INSET LP_SPACE_1           /* from the panel's bottom padding, as the results well sits */
#define TEXT_INSET LP_SPACE_4
#define RULE_W 2
#define RULE_GAP 10
#define LINE_GAP 12                     /* a question and its answer */
#define EXCHANGE_GAP 28                 /* one exchange and the next (the Mac's utterance gap) */
#define PARAGRAPH_GAP 16                /* between a reply's paragraphs */
#define PASSAGE_SIZE 18                 /* Mary's passages: font.display italic, kerned 0.3, 7 px between lines, ink at 0.75 */
#define PASSAGE_KERN 0.3f
#define PASSAGE_LINE 7
#define PASSAGE_INK 0.75f
#define USER_SIZE 16
#define DOT 6
#define METER_BARS 24
#define METER_STEP 4                    /* a 2px bar and a 2px gap */
#define CARET_MS 550.0
#define DOTS_PERIOD_MS 2000.0
#define DOTS_STAGGER_MS 350.0

static const float TWO_PI = 6.2831853f;

const char *lp_spotlight_chat_placeholder(const lp_spotlight_view *v) {
    const lp_mary *m = v->mary;
    if (!m || !lp_mary_connected(m)) return "Mary isn’t running";
    switch (m->state) {
    case LP_MARY_LISTENING:
    case LP_MARY_HEARING: return "Listening…";
    case LP_MARY_TRANSCRIBING: return "Reading it back…";
    case LP_MARY_THINKING: return "Thinking…";
    case LP_MARY_SPEAKING: return "Type to ask something else…";
    default: return "Ask Mary anything…";
    }
}

void lp_spotlight_ask_orb(lp_ctx *ctx, lp_rect bar, const lp_spotlight_view *v, lp_spotlight_result *res) {
    const lp_mary *m = v->mary;
    float size = LP_SIZE_SPOTLIGHT_ACCESSORY, inset = (bar.h - size) / 2;
    lp_rect r = LP_RECT(bar.x + bar.w - inset - size, bar.y + inset, size, size);
    lp_id id = LP_ID("spotlight.ask");
    if (lp_clicked(ctx, id, r)) res->ask_pressed = 1;
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    cairo_t *cr = ctx->cr;
    int active = lp_mary_active(m), connected = lp_mary_connected(m);
    float t = (float)(ctx->now_ms / 1000.0), fill = 0.25f;
    switch (m->state) {
    case LP_MARY_LISTENING:
    case LP_MARY_HEARING: fill = 0.2f + 0.8f * fminf(1, m->level * 14); break;     /* the orb follows your voice */
    case LP_MARY_TRANSCRIBING:
    case LP_MARY_THINKING: fill = 0.5f + 0.15f * sinf(t * TWO_PI / 1.2f); break;
    case LP_MARY_SPEAKING: fill = 0.65f + 0.25f * sinf(t * TWO_PI / 0.9f); break;
    default: break;
    }
    if (!connected) cairo_push_group(cr);
    lp_liquid_bubble(ctx, id, r.x + size / 2, r.y + size / 2, size, active ? LP_TINT_ACCENT : LP_TINT_PLATINUM, 0, fill, "", 0);
    float mark = 14;
    lp_monogram_paint(cr, LP_RECT(r.x + (size - mark) / 2, r.y + (size - mark) / 2, mark, mark), LP_MONOGRAM_FLAT,
                      active ? LP_INK_ON_ACCENT : LP_INK_SECONDARY);
    if (!connected) {
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, 0.45);     /* maryd is away: the orb dims until it is back */
    }
    if (active) lp_want_frame_rect(ctx, r);
}

/* The status row's words and dot. */
static const char *status_of(lp_ctx *ctx, const lp_mary *m, lp_color *dot) {
    lp_accent accent = lp_settings_accent(ctx->settings);
    lp_bubble_colors green = lp_bubble_tint_colors(LP_TINT_ZOOM, &accent.base, &accent.base, &accent.light);
    lp_bubble_colors red = lp_bubble_tint_colors(LP_TINT_CLOSE, &accent.base, &accent.base, &accent.light);
    *dot = LP_INK_TERTIARY;
    if (!lp_mary_connected(m)) return "Mary isn’t running";
    switch (m->state) {
    case LP_MARY_LISTENING: *dot = accent.base; return "Listening";
    case LP_MARY_HEARING: *dot = accent.base; return "Hearing you";
    case LP_MARY_TRANSCRIBING: *dot = LP_INK_SECONDARY; return "Reading it back";
    case LP_MARY_THINKING: *dot = LP_INK_SECONDARY; return "Thinking";
    case LP_MARY_SPEAKING: *dot = green.base; return "Speaking";
    case LP_MARY_ERROR: *dot = red.base; return m->error[0] ? m->error : "Something went wrong";
    default: return m->key_present ? "Standing by" : "Add a Mistral key in Settings › Mary";
    }
}

static void status_row(lp_ctx *ctx, const lp_mary *m, lp_rect row) {
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    lp_color dot;
    const char *label = status_of(ctx, m, &dot);
    float x = row.x + LP_SPACE_3, right = row.x + row.w - LP_SPACE_3;
    lp_fill_solid(cr, LP_RECT(x, row.y + (row.h - DOT) / 2, DOT, DOT), dot, DOT / 2.0f);
    x += DOT + LP_SPACE_2;

    lp_text_style hint = lp_text_style_default();
    hint.size_px = LP_TEXT_XS;
    hint.color = LP_INK_TERTIARY;
    if (lp_mary_active(m)) {
        const char *text = "esc to stop";
        float w = lp_text_measure(cr, text, &hint).w;
        right -= w;
        lp_text_draw(cr, text, LP_RECT(right, row.y, w + 1, row.h), &hint, LP_ALIGN_START);
        right -= LP_SPACE_3;
    }
    if (m->state == LP_MARY_LISTENING || m->state == LP_MARY_HEARING) {
        float loud = fminf(1, fmaxf(0, m->level * 14)), mx = right - METER_BARS * METER_STEP;
        lp_color ink = accent.base;
        ink.a *= 0.8f;
        for (int i = 0; i < METER_BARS; i++) {
            float h = 2 + sinf(3.14159265f * (i + 0.5f) / METER_BARS) * loud * 16;
            lp_fill_solid(cr, LP_RECT(mx + i * METER_STEP, row.y + (row.h - h) / 2, 2, h), ink, 1);
        }
        right = mx - LP_SPACE_3;
    }

    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_XS;
    st.weight = LP_TEXT_WEIGHT_MEDIUM;
    st.color = m->state == LP_MARY_ERROR ? LP_INK_PRIMARY : LP_INK_SECONDARY;
    st.ellipsize = 1;
    lp_text_draw(cr, label, LP_RECT(x, row.y, right - x, row.h), &st, LP_ALIGN_START);
}

static float exchange_alpha(int age) { return age == 0 ? 1.0f : age == 1 ? 0.72f : 0.5f; }

/* One wrapped paragraph at (x, y), drawn when `draw`; returns its height. */
static float paragraph(cairo_t *cr, const char *text, size_t len, const lp_text_style *st, float x, float y, float w, int draw) {
    lp_text_layout *l = lp_text_layout_new(cr, text, (int)len, st, w);
    if (!l) return 0;
    float h = lp_text_layout_size(l).h;
    if (draw) lp_text_layout_draw(cr, l, x, y, st->color);
    lp_text_layout_free(l);
    return h;
}

/* The byte offset of code point `cp` in `text` (clamped to len). */
static size_t byte_at(const char *text, size_t len, int cp) {
    size_t i = 0;
    for (int seen = 0; i < len && seen < cp; i++) if (((unsigned char)text[i] & 0xC0) != 0x80) seen++;
    while (i < len && ((unsigned char)text[i] & 0xC0) == 0x80) i++;
    return i;
}

/* What the pointer is over, asked in the EVENT pass. */
struct probe {
    float x, y;
    int message, owner;     /* the highlighted passage under the pointer, or -1 */
};

/* Mary's passage: the reply as paragraphs on paper, with a brush stroke under every credited line
 * (ContributionHighlightText.swift, PARITY D26/D27). Returns its height. */
static float passage(lp_ctx *ctx, const lp_mary_message *msg, int message_index, float alpha, float x, float y0, float w, int draw,
                     struct probe *probe) {
    cairo_t *cr = ctx->cr;
    lp_text_style st = lp_text_style_default();
    st.font = LP_FONT_DISPLAY;
    st.size_px = PASSAGE_SIZE;
    st.italic = 1;
    st.letter_spacing = PASSAGE_KERN;
    st.line_spacing = PASSAGE_LINE;
    st.color = LP_INK_PRIMARY;
    st.color.a *= PASSAGE_INK * alpha;
    /* A reply still arriving ends in a caret that blinks; laid out with it either way, so the text never re-wraps. */
    size_t n = msg->len;
    char *text = msg->text, *with_caret = NULL;
    if (msg->streaming && (with_caret = malloc(n + 4))) {
        memcpy(with_caret, msg->text, n);
        memcpy(with_caret + n, fmod(ctx->now_ms, 2 * CARET_MS) < CARET_MS ? "\xE2\x96\x8F" : "\xE2\x80\x80", 3);
        with_caret[n + 3] = 0;
        text = with_caret;
        n += 3;
    }
    int instant = ctx->settings && ctx->settings->reduced_motion;
    if (draw && msg->span_count && msg->highlighted_ms <= 0) ((lp_mary_message *)msg)->highlighted_ms = ctx->now_ms;   /* the strokes begin now */
    float y = y0;
    size_t pstart = 0;
    int stroke_index = 0, cp_start = 0, fading = 0;
    while (pstart <= n) {
        /* the paragraph: up to the next blank line */
        const char *sep = pstart < n ? strstr(text + pstart, "\n\n") : NULL;
        size_t pend = sep ? (size_t)(sep - text) : n;
        int cp_end = cp_start;
        for (size_t i = pstart; i < pend; i++) if (((unsigned char)text[i] & 0xC0) != 0x80) cp_end++;
        lp_text_layout *l = lp_text_layout_new(cr, text + pstart, (int)(pend - pstart), &st, w);
        float h = l ? lp_text_layout_size(l).h : 0;
        if (l && msg->span_count) {
            for (int si = 0; si < msg->span_count; si++) {
                const lp_mary_span *span = &msg->spans[si];
                int lo = span->lower > cp_start ? span->lower : cp_start, hi = span->upper < cp_end ? span->upper : cp_end;
                if (lo >= hi) continue;
                size_t b0 = byte_at(text + pstart, pend - pstart, lo - cp_start), b1 = byte_at(text + pstart, pend - pstart, hi - cp_start);
                lp_rect rects[32];
                int count = lp_text_layout_range_rects(l, (int)b0, (int)b1, rects, 32);
                char span_id[96];
                snprintf(span_id, sizeof span_id, "%s#%d-%d", msg->owners[span->owner].id, lo - cp_start, hi - cp_start);
                for (int k = 0; k < count; k++) {
                    lp_rect r = LP_RECT(x + rects[k].x - LP_BRUSH_INFLATE_W / 2.0f, y + rects[k].y - LP_BRUSH_INFLATE_H / 2.0f,
                                        rects[k].w + LP_BRUSH_INFLATE_W, rects[k].h + LP_BRUSH_INFLATE_H);
                    if (probe && lp_rect_contains(r, probe->x, probe->y)) { probe->message = message_index; probe->owner = span->owner; }
                    if (draw) {
                        float opacity = lp_brush_opacity(ctx->now_ms, msg->highlighted_ms, stroke_index, instant);
                        if (opacity < 1) fading = 1;
                        lp_color c = lp_brush_color(msg->owners[span->owner].id);
                        c.a = LP_BRUSH_ALPHA * opacity * alpha;
                        lp_brush_path(cr, r, lp_brush_seed(span_id, k));
                        lp_set_color(cr, c);
                        cairo_fill(cr);
                    }
                    stroke_index++;
                }
            }
        }
        if (l && draw) lp_text_layout_draw(cr, l, x, y, st.color);
        if (l) lp_text_layout_free(l);
        y += h;
        if (!sep) break;
        y += PARAGRAPH_GAP;
        pstart = pend + 2;
        cp_start = cp_end + 2;
    }
    free(with_caret);
    if (fading) lp_want_frame_rect(ctx, LP_RECT(x, y0, w, y - y0));
    return y - y0;
}

/* The dialogue from (x, y0) in a column `w` wide: laid out, and drawn when `draw`. Returns its height. */
static float dialogue(lp_ctx *ctx, const lp_mary *m, float x, float y0, float w, int draw, struct probe *probe) {
    cairo_t *cr = ctx->cr;
    int age[LP_MARY_MESSAGES];
    for (int i = m->message_count - 1, later = 0; i >= 0; i--) {
        age[i] = later;
        if (m->messages[i].role == LP_MARY_USER) later++;
    }
    lp_color gold = lp_brush_palette(0);
    float y = y0 + LP_SPACE_3;
    for (int i = 0; i < m->message_count; i++) {
        const lp_mary_message *msg = &m->messages[i];
        float alpha = exchange_alpha(age[i]);
        if (i > 0) y += msg->role == LP_MARY_USER ? EXCHANGE_GAP : LINE_GAP;
        if (msg->role == LP_MARY_USER) {
            /* what you said: a small sans line behind Mary's gold rule */
            lp_text_style st = lp_text_style_default();
            st.size_px = USER_SIZE;
            st.color = LP_INK_SECONDARY;
            st.color.a *= alpha;
            float h = paragraph(cr, msg->text, msg->len, &st, x + RULE_W + RULE_GAP, y, w - RULE_W - RULE_GAP, draw);
            if (draw) {
                lp_color rule = gold;
                rule.a *= 0.8f * alpha;
                lp_fill_solid(cr, LP_RECT(x, y, RULE_W, h), rule, 1);
            }
            y += h;
        } else {
            y += passage(ctx, msg, i, alpha, x, y, w, draw, probe);
            if (msg->note) {
                /* the words came but the voice did not: say why, quietly, under them */
                char said[300];
                snprintf(said, sizeof said, "Not spoken: %s", msg->note);
                lp_text_style quiet = lp_text_style_default();
                quiet.size_px = LP_TEXT_XS;
                quiet.color = LP_INK_TERTIARY;
                quiet.color.a *= alpha;
                y += 3 + paragraph(cr, said, strlen(said), &quiet, x, y + 3, w, draw);
            }
        }
    }
    if (m->partial[0]) {
        /* What Mary has heard so far: pending, in the tertiary ink, behind a fainter rule. */
        if (m->message_count) y += EXCHANGE_GAP;
        lp_text_style st = lp_text_style_default();
        st.size_px = USER_SIZE;
        st.color = LP_INK_TERTIARY;
        float h = paragraph(cr, m->partial, strlen(m->partial), &st, x + RULE_W + RULE_GAP, y, w - RULE_W - RULE_GAP, draw);
        if (draw) {
            lp_color rule = gold;
            rule.a *= 0.35f;
            lp_fill_solid(cr, LP_RECT(x, y, RULE_W, h), rule, 1);
        }
        y += h;
    }
    const lp_mary_message *last = m->message_count ? &m->messages[m->message_count - 1] : NULL;
    if (m->state == LP_MARY_THINKING && (!last || last->role == LP_MARY_USER)) {
        /* Three dots breathe while the answer is on its way. */
        y += LINE_GAP + 2;
        for (int k = 0; draw && k < 3; k++) {
            double phase = (ctx->now_ms - k * DOTS_STAGGER_MS) / DOTS_PERIOD_MS;
            lp_color ink = LP_INK_SECONDARY;
            ink.a *= 0.25f + 0.75f * (0.5f + 0.5f * (float)sin(phase * TWO_PI));
            lp_fill_solid(cr, LP_RECT(x + k * 9, y, 5, 5), ink, 2.5f);
        }
        y += 5;
    }
    return y - y0 + LP_SPACE_3;
}

void lp_spotlight_chat(lp_ctx *ctx, const lp_spotlight_view *v, lp_rect panel, float y, lp_spotlight_result *res) {
    const lp_mary *m = v->mary;
    if (!m) return;
    int draw = ctx->pass == LP_PASS_DRAW && ctx->cr;
    cairo_t *cr = ctx->cr;
    lp_rect row = LP_RECT(panel.x + LP_SPOTLIGHT_PAD, y, panel.w - 2 * LP_SPOTLIGHT_PAD, LP_SPOTLIGHT_CHAT_STATUS_H);
    if (draw) status_row(ctx, m, row);

    float top = y + LP_SPOTLIGHT_CHAT_STATUS_H;
    lp_rect well = LP_RECT(row.x, top, row.w, panel.y + panel.h - LP_SPOTLIGHT_PAD - WELL_INSET - top);
    if (well.h <= 0) return;
    float radius = lp_radius_flex(ctx, LP_RADIUS_SPOTLIGHT_RESULTS), column = well.w - 2 * TEXT_INSET;
    static lp_scroll_state pinned;
    static float content_h;             /* the last draw's layout: the EVENT pass scrolls against it */
    lp_scroll_state *scroll = v->chat_scroll ? v->chat_scroll : &pinned;
    if (!v->chat_scroll) pinned.y = 1e9f;
    int empty = !m->message_count && !m->partial[0] && m->state != LP_MARY_THINKING;

    if (draw) {
        int at_end = scroll->y >= content_h - well.h - 1;
        content_h = empty ? well.h : dialogue(ctx, m, 0, 0, column, 0, NULL);
        if (at_end) scroll->y = 1e9f;   /* stay on the newest words as they arrive */
        cairo_save(cr);
        lp_path_rrect(cr, well, radius);
        cairo_clip(cr);
        lp_fill_solid(cr, well, LP_SURFACE_WELL, 0);
    }
    lp_rect origin = lp_scroll_begin(ctx, LP_ID("spotlight.chat"), well, (lp_size){ well.w, content_h }, scroll);
    if (draw && empty) {
        lp_text_style st = lp_text_style_default();
        st.size_px = LP_TEXT_SM;
        st.color = LP_INK_TERTIARY;
        const char *hint = lp_mary_connected(m) ? "Ask Mary anything — say “Hey Mary”, or type and press ⌃↩."
                                                : "Mary will be back as soon as maryd is running again.";
        lp_text_draw(cr, hint, LP_RECT(well.x + TEXT_INSET, well.y, column, well.h), &st, LP_ALIGN_CENTER);
    } else if (draw) {
        dialogue(ctx, m, origin.x + TEXT_INSET, origin.y, column, 1, NULL);
    } else if (!empty && lp_hit(ctx, well) && !isnan(ctx->in.mx)) {
        /* the pointer over a credited passage: a hand, and a click opens "From the thread" */
        struct probe probe = { ctx->in.mx, ctx->in.my, -1, -1 };
        dialogue(ctx, m, origin.x + TEXT_INSET, origin.y, column, 0, &probe);
        if (probe.message >= 0) {
            ctx->cursor = LP_CURSOR_POINTER;
            if ((ctx->in.pressed & LP_BUTTON_LEFT) && res) {
                res->contribution_message = probe.message;
                res->contribution_owner = probe.owner;
                ctx->dirty = 1;
            }
        }
    }
    lp_scroll_end(ctx);
    if (draw) {
        cairo_restore(cr);
        lp_draw_inset_shadows(cr, well, radius, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT);
    }
    int streaming = m->message_count && m->messages[m->message_count - 1].streaming;
    if (lp_mary_active(m) || streaming) lp_want_frame_rect(ctx, LP_RECT(panel.x, y, panel.w, panel.y + panel.h - y));
}
