#include <stdio.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_controls.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

void lp_text_buffer_set(lp_text_buffer *b, const char *text) {
    snprintf(b->text, sizeof b->text, "%s", text ? text : "");
    b->len = (int)strlen(b->text);
    b->cursor = b->len;
    b->all_selected = 0;
}

void lp_text_buffer_set_selected(lp_text_buffer *b, const char *text) {
    lp_text_buffer_set(b, text);
    b->all_selected = b->len > 0;
}

/* The selection covers everything: the next character (or a delete) replaces it. */
static void clear_all(lp_text_buffer *b) {
    b->text[0] = 0;
    b->len = 0;
    b->cursor = 0;
    b->all_selected = 0;
}

static int utf8_prev(const char *s, int i) { if (i <= 0) return 0; i--; while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--; return i; }
static int utf8_next(const char *s, int i, int len) { if (i >= len) return len; i++; while (i < len && ((unsigned char)s[i] & 0xC0) == 0x80) i++; return i; }

static int edit(lp_text_buffer *b, const lp_input *in) {
    if (!in->key_pressed) return 0;
    if (b->all_selected) {
        switch (in->keysym) {
        case XKB_KEY_BackSpace: case XKB_KEY_Delete: clear_all(b); return 1;
        case XKB_KEY_Left: case XKB_KEY_Home: b->all_selected = 0; b->cursor = 0; return 0;
        case XKB_KEY_Right: case XKB_KEY_End: b->all_selected = 0; b->cursor = b->len; return 0;
        default: break;
        }
        /* printable text replaces the whole name */
        if (!(in->mods & (LP_MOD_CTRL | LP_MOD_LOGO | LP_MOD_ALT)) && in->utf8[0] &&
            (unsigned char)in->utf8[0] >= 0x20 && in->utf8[0] != 0x7f) clear_all(b);
        else b->all_selected = 0;
    }
    switch (in->keysym) {
    case XKB_KEY_BackSpace: {
        if (b->cursor == 0) return 0;
        int p = utf8_prev(b->text, b->cursor);
        memmove(b->text + p, b->text + b->cursor, (size_t)(b->len - b->cursor + 1));
        b->len -= b->cursor - p;
        b->cursor = p;
        return 1;
    }
    case XKB_KEY_Delete: {
        if (b->cursor >= b->len) return 0;
        int n = utf8_next(b->text, b->cursor, b->len);
        memmove(b->text + b->cursor, b->text + n, (size_t)(b->len - n + 1));
        b->len -= n - b->cursor;
        return 1;
    }
    case XKB_KEY_Left: b->cursor = utf8_prev(b->text, b->cursor); return 0;
    case XKB_KEY_Right: b->cursor = utf8_next(b->text, b->cursor, b->len); return 0;
    case XKB_KEY_Home: b->cursor = 0; return 0;
    case XKB_KEY_End: b->cursor = b->len; return 0;
    default: break;
    }
    if (in->mods & (LP_MOD_CTRL | LP_MOD_LOGO | LP_MOD_ALT)) return 0;
    size_t n = strlen(in->utf8);
    if (n == 0 || (unsigned char)in->utf8[0] < 0x20 || in->utf8[0] == 0x7f) return 0;
    if (b->len + (int)n >= (int)sizeof b->text) return 0;
    memmove(b->text + b->cursor + n, b->text + b->cursor, (size_t)(b->len - b->cursor + 1));
    memcpy(b->text + b->cursor, in->utf8, n);
    b->len += (int)n;
    b->cursor += (int)n;
    return 1;
}

int lp_text_field(lp_ctx *ctx, lp_id id, lp_rect r, lp_text_buffer *b, lp_text_field_opts o) {
    float pad = o.round ? LP_SPACE_3 : LP_SPACE_2;
    float radius = o.round ? LP_RADIUS_PILL : lp_radius_flex(ctx, LP_RADIUS_SM);
    int changed = 0;
    if (!o.disabled) {
        lp_hot(ctx, id, r);
        if (ctx->pass == LP_PASS_EVENT) {
            if (lp_hit(ctx, r)) ctx->cursor = LP_CURSOR_TEXT;
            if (lp_hit(ctx, r) && (ctx->in.pressed & LP_BUTTON_LEFT)) { if (ctx->focus != id) ctx->dirty = 1; ctx->focus = id; b->all_selected = 0; }
            else if (!lp_hit(ctx, r) && (ctx->in.pressed & LP_BUTTON_LEFT) && ctx->focus == id) { ctx->focus = 0; ctx->dirty = 1; }
            if (ctx->focus == id && ctx->in.key_pressed) { changed = edit(b, &ctx->in); ctx->dirty = 1; }
        }
    }
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return changed;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    lp_fill_solid(cr, r, LP_SURFACE_WELL, radius);
    lp_draw_inset_shadows(cr, r, radius, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT);
    if (ctx->focus == id && !o.disabled) lp_draw_focus_ring(cr, r, radius, accent.focus_ring, 3);
    float x = r.x + pad, icon_px = o.large ? 16 : 14;
    if (o.icon < LP_ICON_COUNT) { lp_icon_draw(cr, o.icon, x, r.y + (r.h - icon_px) / 2, icon_px, 0, LP_INK_TERTIARY); x += icon_px + (o.large ? LP_SPACE_2 : LP_SPACE_1); }
    lp_text_style st = lp_text_style_default();
    if (o.large) st.size_px = LP_TEXT_LG;
    st.ellipsize = 1;
    lp_rect text_rect = LP_RECT(x, r.y, r.x + r.w - pad - x, r.h);
    cairo_save(cr);
    cairo_rectangle(cr, text_rect.x, text_rect.y, text_rect.w, text_rect.h);
    cairo_clip(cr);
    if (b->len == 0 && o.placeholder) {
        st.color = LP_INK_TERTIARY;
        lp_text_draw(cr, o.placeholder, text_rect, &st, LP_ALIGN_START);
    } else {
        st.color = o.disabled ? LP_INK_DISABLED : LP_INK_PRIMARY;
        st.ellipsize = 0;
        if (b->all_selected && ctx->focus == id && !o.disabled) {
            lp_size ts = lp_text_measure(cr, b->text, &st);
            float w = ts.w < text_rect.w ? ts.w : text_rect.w;
            lp_fill_solid(cr, LP_RECT(text_rect.x - 1, r.y + (r.h - st.size_px - 6) / 2, w + 2, st.size_px + 6), accent.light, LP_RADIUS_XS);
            st.color = LP_INK_ON_ACCENT;
        }
        lp_text_draw(cr, b->text, text_rect, &st, LP_ALIGN_START);
    }
    if (ctx->focus == id && !o.disabled && !b->all_selected) {
        char head[256];
        memcpy(head, b->text, (size_t)b->cursor);
        head[b->cursor] = 0;
        float cx = text_rect.x + (b->cursor ? lp_text_measure(cr, head, &st).w : 0);
        float ch = o.large ? st.size_px + 6 : r.h - 8;
        lp_fill_solid(cr, LP_RECT(cx, r.y + (r.h - ch) / 2, 1, ch), LP_INK_PRIMARY, 0);
    }
    cairo_restore(cr);
    return changed;
}
