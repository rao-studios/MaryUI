#include "maryui/lp_pane.h"

#include <math.h>
#include <string.h>

#include "maryui/components/lp_button.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

const lp_color LP_PANE_GOLD = { 0.68f, 0.56f, 0.38f, 1 };
const lp_color LP_PANE_SAGE = { 0.38f, 0.55f, 0.38f, 1 };
const lp_color LP_PANE_BLUE = { 0.22f, 0.44f, 0.65f, 1 };
const lp_color LP_PANE_MAUVE = { 0.65f, 0.40f, 0.55f, 1 };
const lp_color LP_PANE_RED = { 0.72f, 0.30f, 0.26f, 1 };

static int drawing(const lp_ctx *ctx) { return ctx->pass == LP_PASS_DRAW && ctx->cr != NULL; }

static lp_text_style small(lp_color color) {
    lp_text_style s = lp_text_style_default();
    s.size_px = LP_TEXT_SM;
    s.color = color;
    return s;
}

lp_rect lp_pane_header_paint(lp_ctx *ctx, lp_rect *area, const lp_pane_header *h) {
    float height = LP_PANE_HEADER_H + (h->note && h->note[0] ? LP_PANE_HEADER_NOTE_H : 0);
    lp_rect head = lp_rect_cut_top(area, height);
    float x = head.x + LP_PANE_INSET, w = head.w - 2 * LP_PANE_INSET;
    lp_rect title = LP_RECT(x, head.y + 12, w, 28), sub = LP_RECT(x, head.y + 40, w, 16);
    if (drawing(ctx) && lp_clip_intersects(ctx->cr, head)) {
        cairo_t *cr = ctx->cr;
        lp_fill_solid(cr, head, LP_SURFACE_BODY, 0);
        lp_text_style ts = lp_text_style_default();
        ts.size_px = LP_TEXT_XL;
        ts.weight = LP_TEXT_WEIGHT_BOLD;
        ts.emboss = 1;
        ts.ellipsize = 1;
        lp_text_draw(cr, h->title ? h->title : "", LP_RECT(title.x, title.y, title.w - 200, title.h), &ts, LP_ALIGN_START);
        /* the dot and its word, at the subtitle line's right */
        float right = x + w;
        if (h->live >= 0) {
            if (h->status && h->status[0]) {
                lp_text_style ss = small(LP_INK_TERTIARY);
                float sw = lp_text_measure(cr, h->status, &ss).w;
                lp_text_draw(cr, h->status, LP_RECT(right - sw, sub.y, sw + 1, sub.h), &ss, LP_ALIGN_START);
                right -= sw + LP_SPACE_2;
            }
            lp_pane_live_dot(ctx, right - 4, sub.y + sub.h / 2, h->live);
            right -= 8 + LP_SPACE_3;
        }
        if (h->subtitle && h->subtitle[0]) {
            lp_text_style ss = small(LP_INK_TERTIARY);
            ss.ellipsize = 1;
            lp_text_draw(cr, h->subtitle, LP_RECT(sub.x, sub.y, right - sub.x, sub.h), &ss, LP_ALIGN_START);
        }
        if (h->note && h->note[0]) {
            lp_text_style ns = lp_text_style_default();
            ns.font = LP_FONT_DISPLAY;
            ns.italic = 1;
            ns.size_px = LP_TEXT_MD;
            ns.color = LP_INK_SECONDARY;
            ns.ellipsize = 1;
            lp_text_draw(cr, h->note, LP_RECT(x, head.y + LP_PANE_HEADER_H - 4, w, 18), &ns, LP_ALIGN_START);
        }
        lp_draw_hairline(cr, LP_RECT(head.x, head.y + head.h - 1, head.w, 0), LP_EDGE_TOP, LP_EDGE_DIVIDER);
    }
    return LP_RECT(x + w - 320, title.y + (title.h - LP_SIZE_CONTROL_HEIGHT) / 2, 320, LP_SIZE_CONTROL_HEIGHT);
}

lp_rect lp_pane_bar(lp_ctx *ctx, lp_rect *area) {
    lp_rect bar = lp_rect_cut_top(area, LP_PANE_BAR_H);
    if (drawing(ctx) && lp_clip_intersects(ctx->cr, bar)) {
        lp_fill_solid(ctx->cr, bar, LP_SURFACE_BODY, 0);
        lp_draw_hairline(ctx->cr, LP_RECT(bar.x, bar.y + bar.h - 1, bar.w, 0), LP_EDGE_TOP, LP_EDGE_DIVIDER);
    }
    return LP_RECT(bar.x + LP_PANE_INSET, bar.y, bar.w - 2 * LP_PANE_INSET, bar.h);
}

lp_rect lp_pane_content(lp_rect area) { return lp_rect_inset(area, LP_PANE_INSET, LP_SPACE_4); }

void lp_pane_card_frame(lp_ctx *ctx, lp_rect box, const char *title) {
    if (!drawing(ctx) || !lp_clip_intersects(ctx->cr, box)) return;
    /* a card lifts off the body: a whiter fill, and its rounded outline stroked rather than four hairlines */
    lp_fill_solid(ctx->cr, box, LP_RGBA(1, 1, 1, 0.6f), LP_RADIUS_MD);
    lp_path_rrect(ctx->cr, LP_RECT(box.x + 0.5f, box.y + 0.5f, box.w - 1, box.h - 1), LP_RADIUS_MD);
    lp_set_color(ctx->cr, LP_EDGE_DIVIDER);
    cairo_set_line_width(ctx->cr, 1);
    cairo_stroke(ctx->cr);
    if (!title || !title[0]) return;
    lp_text_style ts = lp_text_style_default();
    ts.size_px = LP_TEXT_XS;
    ts.weight = LP_TEXT_WEIGHT_SEMIBOLD;
    ts.uppercase = 1;
    ts.letter_spacing = LP_TEXT_XS * 0.04f;
    ts.color = LP_INK_TERTIARY;
    ts.ellipsize = 1;
    lp_text_draw(ctx->cr, title, LP_RECT(box.x + LP_PANE_CARD_PAD, box.y + 8, box.w - 2 * LP_PANE_CARD_PAD, 16), &ts, LP_ALIGN_START);
}

lp_rect lp_pane_card(lp_ctx *ctx, lp_rect *column, const char *title, float body_h, lp_rect *box_out) {
    float h = LP_PANE_CARD_TITLE_H + body_h + LP_PANE_CARD_PAD;
    lp_rect cut = lp_rect_cut_top(column, h + LP_PANE_CARD_GAP);
    lp_rect box = LP_RECT(cut.x, cut.y, cut.w, h);
    lp_pane_card_frame(ctx, box, title);
    if (box_out) *box_out = box;
    return LP_RECT(box.x + LP_PANE_CARD_PAD, box.y + LP_PANE_CARD_TITLE_H, box.w - 2 * LP_PANE_CARD_PAD, body_h);
}

int lp_pane_lines_of(const char *text, float w) {
    if (!text || !*text) return 1;
    float per_line = w / 6.4f;
    if (per_line < 8) per_line = 8;
    int lines = 0;
    const char *p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        lines += 1 + (int)((float)len / per_line);
        p += len + (nl ? 1 : 0);
    }
    return lines ? lines : 1;
}

float lp_pane_paragraph_h(const char *text, float w) { return (float)lp_pane_lines_of(text, w) * LP_PANE_LEADING + 4; }

void lp_pane_row(lp_ctx *ctx, float x, float *y, float w, const char *label, const char *value) {
    lp_rect r = LP_RECT(x, *y, w, LP_PANE_ROW);
    if (drawing(ctx) && lp_clip_intersects(ctx->cr, r)) {
        lp_text_style ls = small(LP_INK_TERTIARY), vs = small(LP_INK_PRIMARY);
        ls.ellipsize = 1;
        vs.ellipsize = 1;
        lp_text_draw(ctx->cr, label ? label : "", LP_RECT(x, *y, LP_PANE_LABEL_W, LP_PANE_ROW), &ls, LP_ALIGN_END);
        lp_text_draw(ctx->cr, value && *value ? value : "\xE2\x80\x94", LP_RECT(x + LP_PANE_LABEL_W + LP_PANE_GUTTER, *y, w - LP_PANE_LABEL_W - LP_PANE_GUTTER, LP_PANE_ROW), &vs, LP_ALIGN_START);
    }
    *y += LP_PANE_ROW;
}

void lp_pane_paragraph(lp_ctx *ctx, float x, float *y, float w, const char *text, lp_color color, int italic) {
    if (drawing(ctx) && text) {
        lp_text_style s = small(color);
        s.italic = italic;
        if (italic) s.font = LP_FONT_DISPLAY;
        lp_text_layout *l = lp_text_layout_new(ctx->cr, text, -1, &s, w);
        lp_size size = lp_text_layout_size(l);
        lp_text_layout_draw(ctx->cr, l, x, *y, color);
        lp_text_layout_free(l);
        *y += size.h + 4;
        return;
    }
    *y += lp_pane_paragraph_h(text, w);
}

void lp_pane_mono(lp_ctx *ctx, float x, float y, float w, const char *text, lp_color color) {
    if (!drawing(ctx)) return;
    lp_text_style s = lp_text_style_default();
    s.font = LP_FONT_MONO;
    s.size_px = LP_TEXT_XS;
    s.color = color;
    s.ellipsize = 1;
    lp_text_draw(ctx->cr, text ? text : "", LP_RECT(x, y, w, LP_PANE_ROW), &s, LP_ALIGN_START);
}

static lp_text_style capsule_style(lp_color color) {
    lp_text_style s = lp_text_style_default();
    s.size_px = LP_TEXT_XS;
    s.weight = LP_TEXT_WEIGHT_SEMIBOLD;
    s.uppercase = 1;
    s.letter_spacing = 0.5f;
    s.color = color;
    return s;
}

static float capsule_width(lp_ctx *ctx, const char *label) {
    lp_text_style s = capsule_style(LP_INK_PRIMARY);
    if (ctx->cr) return lp_text_measure(ctx->cr, label, &s).w + 12;
    return (float)strlen(label) * 7.2f + 12;
}

float lp_pane_capsule(lp_ctx *ctx, float x, float y, const char *label, lp_color color) {
    float w = capsule_width(ctx, label);
    if (!drawing(ctx)) return w;
    lp_rect r = LP_RECT(x, y, w, 16);
    if (!lp_clip_intersects(ctx->cr, r)) return w;
    lp_text_style s = capsule_style(color);
    lp_color fill = color;
    fill.a = 0.16f;
    lp_fill_solid(ctx->cr, r, fill, 8);
    lp_text_draw(ctx->cr, label, LP_RECT(r.x + 6, r.y, w - 12, r.h), &s, LP_ALIGN_START);
    return w;
}

float lp_pane_capsule_right(lp_ctx *ctx, float right, float y, const char *label, lp_color color) {
    float w = capsule_width(ctx, label);
    return lp_pane_capsule(ctx, right - w, y, label, color);
}

int lp_pane_chip(lp_ctx *ctx, lp_id id, float x, float y, const char *label, int selected, lp_size *size) {
    lp_button_opts o = { selected ? LP_BUTTON_PRIMARY : LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, 0 };
    *size = lp_button_measure(ctx, label, o);
    return lp_button(ctx, id, LP_RECT(x, y, size->w, size->h), label, o);
}

void lp_pane_live_dot(lp_ctx *ctx, float x, float y, int on) {
    if (!drawing(ctx)) return;
    cairo_arc(ctx->cr, x, y, 4, 0, 2 * M_PI);
    lp_set_color(ctx->cr, on ? LP_PANE_SAGE : LP_INK_DISABLED);
    cairo_fill(ctx->cr);
}

void lp_pane_empty(lp_ctx *ctx, lp_rect area, const char *text) {
    if (!drawing(ctx) || !lp_clip_intersects(ctx->cr, area)) return;
    lp_text_style s = small(LP_INK_TERTIARY);
    lp_text_draw(ctx->cr, text ? text : "", area, &s, LP_ALIGN_CENTER);
}
