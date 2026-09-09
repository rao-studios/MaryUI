#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_text_area.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

/* MARK: - The document */

static void reserve(lp_text_doc *d, int extra) {
    if (d->len + extra + 1 <= d->cap) return;
    int cap = d->cap ? d->cap : 256;
    while (d->len + extra + 1 > cap) cap *= 2;
    d->text = realloc(d->text, (size_t)cap);
    d->cap = cap;
}

void lp_text_doc_init(lp_text_doc *d) {
    memset(d, 0, sizeof *d);
    reserve(d, 0);
    d->text[0] = 0;
    d->preferred_x = -1;
}

void lp_text_doc_free(lp_text_doc *d) { free(d->text); memset(d, 0, sizeof *d); }

void lp_text_doc_set(lp_text_doc *d, const char *text) {
    if (!d->text) lp_text_doc_init(d);
    int n = text ? (int)strlen(text) : 0;
    d->len = 0;
    reserve(d, n);
    memcpy(d->text, text ? text : "", (size_t)n + 1);
    d->len = n;
    d->cursor = d->anchor = n;
    d->preferred_x = -1;
}

int lp_text_doc_has_selection(const lp_text_doc *d) { return d->cursor != d->anchor; }
void lp_text_doc_selection(const lp_text_doc *d, int *start, int *end) {
    int s = d->cursor < d->anchor ? d->cursor : d->anchor, e = d->cursor < d->anchor ? d->anchor : d->cursor;
    if (start) *start = s;
    if (end) *end = e;
}

int lp_text_doc_delete_selection(lp_text_doc *d) {
    if (!lp_text_doc_has_selection(d)) return 0;
    int s, e;
    lp_text_doc_selection(d, &s, &e);
    memmove(d->text + s, d->text + e, (size_t)(d->len - e + 1));
    d->len -= e - s;
    d->cursor = d->anchor = s;
    return 1;
}

void lp_text_doc_insert(lp_text_doc *d, const char *utf8, int n) {
    if (!d->text) lp_text_doc_init(d);
    lp_text_doc_delete_selection(d);
    if (n <= 0) return;
    reserve(d, n);
    memmove(d->text + d->cursor + n, d->text + d->cursor, (size_t)(d->len - d->cursor + 1));
    memcpy(d->text + d->cursor, utf8, (size_t)n);
    d->len += n;
    d->cursor += n;
    d->anchor = d->cursor;
}

void lp_text_doc_select_all(lp_text_doc *d) { d->anchor = 0; d->cursor = d->len; }

int lp_text_doc_word_count(const lp_text_doc *d) {
    int words = 0, in_word = 0;
    for (int i = 0; i < d->len; i++) {
        int space = isspace((unsigned char)d->text[i]);
        if (!space && !in_word) words++;
        in_word = !space;
    }
    return words;
}

int lp_text_doc_char_count(const lp_text_doc *d) {
    int n = 0;
    for (int i = 0; i < d->len; i++) if (((unsigned char)d->text[i] & 0xC0) != 0x80) n++;
    return n;
}

void lp_text_doc_line_col(const lp_text_doc *d, int index, int *line, int *col) {
    int ln = 1, c = 1;
    if (index > d->len) index = d->len;
    for (int i = 0; i < index; i++) {
        if (d->text[i] == '\n') { ln++; c = 1; }
        else if (((unsigned char)d->text[i] & 0xC0) != 0x80) c++;
    }
    if (line) *line = ln;
    if (col) *col = c;
}

/* MARK: - The clipboard */

static lp_text_clipboard shared_clipboard;
lp_text_clipboard *lp_text_clipboard_shared(void) { return &shared_clipboard; }

void lp_text_clipboard_set(lp_text_clipboard *c, const char *text, int len) {
    free(c->text);
    c->text = malloc((size_t)len + 1);
    memcpy(c->text, text, (size_t)len);
    c->text[len] = 0;
    c->len = len;
}

/* MARK: - Keys */

static int utf8_prev(const char *s, int i) { if (i <= 0) return 0; i--; while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--; return i; }
static int utf8_next(const char *s, int i, int len) { if (i >= len) return len; i++; while (i < len && ((unsigned char)s[i] & 0xC0) == 0x80) i++; return i; }

static void move_to(lp_text_doc *d, int index, int extend) {
    d->cursor = index;
    if (!extend) d->anchor = index;
    d->preferred_x = -1;
}

int lp_text_doc_key(lp_text_doc *d, uint32_t keysym, uint32_t mods, const char *utf8, lp_text_clipboard *clip) {
    if (!d->text) lp_text_doc_init(d);
    int shift = (mods & LP_MOD_SHIFT) != 0;
    int cmd = (mods & (LP_MOD_CTRL | LP_MOD_LOGO)) != 0;
    int s, e;
    lp_text_doc_selection(d, &s, &e);
    switch (keysym) {
    case XKB_KEY_BackSpace:
        if (lp_text_doc_delete_selection(d)) return 1;
        if (d->cursor == 0) return 0;
        d->anchor = utf8_prev(d->text, d->cursor);
        lp_text_doc_delete_selection(d);
        return 1;
    case XKB_KEY_Delete:
        if (lp_text_doc_delete_selection(d)) return 1;
        if (d->cursor >= d->len) return 0;
        d->anchor = utf8_next(d->text, d->cursor, d->len);
        lp_text_doc_delete_selection(d);
        return 1;
    case XKB_KEY_Left:
        if (!shift && lp_text_doc_has_selection(d)) move_to(d, s, 0);
        else move_to(d, utf8_prev(d->text, d->cursor), shift);
        return 0;
    case XKB_KEY_Right:
        if (!shift && lp_text_doc_has_selection(d)) move_to(d, e, 0);
        else move_to(d, utf8_next(d->text, d->cursor, d->len), shift);
        return 0;
    case XKB_KEY_Home: {
        int i = d->cursor;
        while (i > 0 && d->text[i - 1] != '\n') i--;
        move_to(d, cmd ? 0 : i, shift);
        return 0;
    }
    case XKB_KEY_End: {
        int i = d->cursor;
        while (i < d->len && d->text[i] != '\n') i++;
        move_to(d, cmd ? d->len : i, shift);
        return 0;
    }
    case XKB_KEY_Return: case XKB_KEY_KP_Enter:
        lp_text_doc_insert(d, "\n", 1);
        return 1;
    case XKB_KEY_Tab:
        lp_text_doc_insert(d, "\t", 1);
        return 1;
    default: break;
    }
    if (cmd) {
        switch (keysym) {
        case XKB_KEY_a: case XKB_KEY_A: lp_text_doc_select_all(d); return 0;
        case XKB_KEY_c: case XKB_KEY_C:
            if (clip && e > s) lp_text_clipboard_set(clip, d->text + s, e - s);
            return 0;
        case XKB_KEY_x: case XKB_KEY_X:
            if (clip && e > s) lp_text_clipboard_set(clip, d->text + s, e - s);
            return lp_text_doc_delete_selection(d);
        case XKB_KEY_v: case XKB_KEY_V:
            if (clip && clip->len > 0) { lp_text_doc_insert(d, clip->text, clip->len); return 1; }
            return 0;
        default: return 0;
        }
    }
    if (mods & LP_MOD_ALT) return 0;
    size_t n = strlen(utf8);
    if (n == 0 || (unsigned char)utf8[0] < 0x20 || utf8[0] == 0x7f) return 0;
    lp_text_doc_insert(d, utf8, (int)n);
    return 1;
}

/* MARK: - The widget */

#define PAD LP_SPACE_2
#define LANE 10   /* the scrollbar's lane */

int lp_text_area(lp_ctx *ctx, lp_id id, lp_rect r, lp_text_area_state *s, lp_text_area_opts o) {
    lp_text_doc *doc = &s->doc;
    if (!doc->text) lp_text_doc_init(doc);
    int changed = 0;
    lp_text_style st = lp_text_style_default();
    if (o.mono) st.font = LP_FONT_MONO;
    st.color = o.disabled ? LP_INK_DISABLED : LP_INK_PRIMARY;
    float wrap_w = r.w - 2 * PAD - LANE;
    if (wrap_w < 20) wrap_w = 20;
    lp_text_layout *l = lp_text_layout_new(ctx->cr, doc->text, doc->len, &st, wrap_w);
    lp_size ts = lp_text_layout_size(l);

    if (ctx->pass == LP_PASS_EVENT && !o.disabled) {
        lp_hot(ctx, id, r);
        int inside = lp_hit(ctx, r);
        float tx = r.x + PAD, ty = r.y + PAD - s->scroll.y;
        if (inside) ctx->cursor = LP_CURSOR_TEXT;
        if (inside && (ctx->in.pressed & LP_BUTTON_LEFT)) {
            int idx = lp_text_layout_xy_to_index(l, ctx->in.mx - tx, ctx->in.my - ty);
            if (!(ctx->in.mods & LP_MOD_SHIFT)) doc->anchor = idx;
            doc->cursor = idx;
            doc->preferred_x = -1;
            s->dragging = 1;
            ctx->focus = id;
            ctx->dirty = 1;
        } else if (!inside && (ctx->in.pressed & LP_BUTTON_LEFT) && ctx->focus == id) {
            ctx->focus = 0;
            ctx->dirty = 1;
        }
        if (s->dragging && (ctx->in.buttons & LP_BUTTON_LEFT) && !(ctx->in.pressed & LP_BUTTON_LEFT) && !isnan(ctx->in.mx)) {
            int idx = lp_text_layout_xy_to_index(l, ctx->in.mx - tx, ctx->in.my - ty);
            if (idx != doc->cursor) { doc->cursor = idx; ctx->dirty = 1; }
        }
        if (s->dragging && (ctx->in.released & LP_BUTTON_LEFT)) s->dragging = 0;
        if (ctx->focus == id && ctx->in.key_pressed) {
            int shift = (ctx->in.mods & LP_MOD_SHIFT) != 0;
            int handled = 1;
            switch (ctx->in.keysym) {
            case XKB_KEY_Up: case XKB_KEY_Down: {
                if (doc->preferred_x < 0) doc->preferred_x = lp_text_layout_index_to_pos(l, doc->cursor).x;
                int idx = lp_text_layout_move_line(l, doc->cursor, ctx->in.keysym == XKB_KEY_Up ? -1 : 1, doc->preferred_x);
                if (idx < 0) idx = ctx->in.keysym == XKB_KEY_Up ? 0 : doc->len;
                doc->cursor = idx;
                if (!shift) doc->anchor = idx;
                break;
            }
            case XKB_KEY_Home: case XKB_KEY_End: {
                if (ctx->in.mods & (LP_MOD_CTRL | LP_MOD_LOGO)) { handled = 0; break; }
                int a, b;
                lp_text_layout_line_bounds(l, doc->cursor, &a, &b);
                doc->cursor = ctx->in.keysym == XKB_KEY_Home ? a : b;
                if (!shift) doc->anchor = doc->cursor;
                doc->preferred_x = -1;
                break;
            }
            default: handled = 0;
            }
            if (!handled) changed = lp_text_doc_key(doc, ctx->in.keysym, ctx->in.mods, ctx->in.utf8, lp_text_clipboard_shared());
            ctx->dirty = 1;
            if (changed) {
                lp_text_layout_free(l);
                l = lp_text_layout_new(ctx->cr, doc->text, doc->len, &st, wrap_w);
                ts = lp_text_layout_size(l);
            }
            /* keep the caret in view */
            lp_rect caret = lp_text_layout_index_to_pos(l, doc->cursor);
            float top = PAD + caret.y, bottom = top + caret.h + PAD;
            if (top - s->scroll.y < 0) s->scroll.y = top;
            else if (bottom - s->scroll.y > r.h) s->scroll.y = bottom - r.h;
        }
    }

    int draw = ctx->pass == LP_PASS_DRAW && ctx->cr;
    cairo_t *cr = ctx->cr;
    if (draw) lp_fill_solid(cr, r, LP_SURFACE_WELL, LP_RADIUS_SM);
    lp_size content = { r.w, ts.h + 2 * PAD };
    lp_rect c = lp_scroll_begin(ctx, lp_id_index(id, 1), r, content, &s->scroll);
    if (draw) {
        float tx = c.x + PAD, ty = c.y + PAD;
        lp_accent accent = lp_settings_accent(ctx->settings);
        int focused = ctx->focus == id && !o.disabled;
        if (lp_text_doc_has_selection(doc)) {
            int a, b;
            lp_text_doc_selection(doc, &a, &b);
            lp_rect rects[256];
            int n = lp_text_layout_range_rects(l, a, b, rects, 256);
            lp_color fill = ctx->active_window && focused ? accent.soft : LP_PLATINUM_4;
            for (int i = 0; i < n; i++) lp_fill_solid(cr, LP_RECT(tx + rects[i].x, ty + rects[i].y, rects[i].w, rects[i].h), fill, 0);
        }
        if (doc->len == 0 && o.placeholder) {
            lp_text_style ps = st;
            ps.color = LP_INK_TERTIARY;
            lp_text_draw_at(cr, o.placeholder, tx, ty + lp_text_layout_index_to_pos(l, 0).h * 0.78f, &ps);
        } else {
            lp_text_layout_draw(cr, l, tx, ty, st.color);
        }
        if (focused) {
            lp_rect caret = lp_text_layout_index_to_pos(l, doc->cursor);
            lp_fill_solid(cr, LP_RECT(tx + caret.x, ty + caret.y, 1, caret.h), LP_INK_PRIMARY, 0);
        }
    }
    lp_scroll_end(ctx);
    if (draw) {
        lp_draw_inset_shadows(cr, r, LP_RADIUS_SM, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT);
        if (ctx->focus == id && !o.disabled) lp_draw_focus_ring(cr, r, LP_RADIUS_SM, lp_settings_accent(ctx->settings).focus_ring, 3);
    }
    lp_text_layout_free(l);
    return changed;
}
