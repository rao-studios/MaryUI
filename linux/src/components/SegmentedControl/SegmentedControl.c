#include <math.h>
#include <string.h>

#include "maryui/components/lp_controls.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

static lp_text_style seg_style(enum lp_control_size size) {
    lp_text_style st = lp_text_style_default();
    st.size_px = size == LP_CONTROL_SM ? LP_TEXT_XS : LP_TEXT_SM;
    st.weight = LP_TEXT_WEIGHT_MEDIUM;
    st.color = LP_INK_SECONDARY;
    return st;
}

static float segment_width(lp_ctx *ctx, const lp_segment *s, enum lp_control_size size) {
    float pad = size == LP_CONTROL_SM ? LP_SPACE_2 : LP_SPACE_3;
    float icon = s->icon < LP_ICON_COUNT ? (size == LP_CONTROL_SM ? 13 : 14) : 0;
    lp_text_style st = seg_style(size);
    float text = s->label ? (ctx->cr ? lp_text_measure(ctx->cr, s->label, &st).w : 7.0f * strlen(s->label)) : 0;
    return 2 * pad + icon + (icon && text ? LP_SPACE_1 : 0) + text;
}

lp_size lp_segmented_measure(lp_ctx *ctx, const lp_segment *options, int n, enum lp_control_size size) {
    float widest = 0;
    for (int i = 0; i < n; i++) widest = fmaxf(widest, segment_width(ctx, &options[i], size));
    float h = size == LP_CONTROL_SM ? LP_SIZE_CONTROL_HEIGHT_SM : LP_SIZE_CONTROL_HEIGHT;
    return (lp_size){ ceilf(widest) * n + 4, h };
}

int lp_segmented(lp_ctx *ctx, lp_id id, float x, float y, const lp_segment *options, int n, int *index, enum lp_control_size size) {
    lp_size sz = lp_segmented_measure(ctx, options, n, size);
    lp_rect track = LP_RECT(x, y, sz.w, sz.h);
    lp_rect inner = lp_rect_inset(track, 2, 2);
    float seg_w = inner.w / n;
    int changed = 0, hot = -1;
    for (int i = 0; i < n; i++) {
        lp_rect s = LP_RECT(inner.x + i * seg_w, inner.y, seg_w, inner.h);
        lp_id sid = lp_id_index(id, i);
        if (lp_hot(ctx, sid, s)) hot = i;
        if (lp_clicked(ctx, sid, s) && *index != i) { *index = i; changed = 1; ctx->dirty = 1; }
    }
    if (ctx->pass == LP_PASS_EVENT && ctx->focus == id && ctx->in.key_pressed) {
        int d = ctx->in.keysym == 0xff53 ? 1 : (ctx->in.keysym == 0xff51 ? -1 : 0);
        if (d) { *index = (*index + d + n) % n; changed = 1; ctx->dirty = 1; }
    }
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return changed;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    lp_fill_solid(cr, track, LP_PLATINUM_3, LP_RADIUS_PILL);
    lp_draw_inset_shadows(cr, track, LP_RADIUS_PILL, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT);
    /* hot blob beside the selection: platinum.1 at 85%, scaled 1.02×1.1 */
    if (hot >= 0 && hot != *index) {
        lp_rect b = LP_RECT(inner.x + hot * seg_w, inner.y, seg_w, inner.h);
        lp_rect scaled = LP_RECT(b.x + b.w / 2 - b.w * 0.51f, b.y + b.h / 2 - b.h * 0.55f, b.w * 1.02f, b.h * 1.1f);
        cairo_save(cr);
        lp_path_rrect(cr, track, LP_RADIUS_PILL);
        cairo_clip(cr);
        lp_fill_solid(cr, scaled, lp_color_with_alpha(LP_PLATINUM_1, 0.85f), LP_RADIUS_PILL);
        cairo_restore(cr);
    }
    /* thumb */
    lp_rect thumb = LP_RECT(inner.x + (*index) * seg_w, inner.y, seg_w, inner.h);
    static const lp_shadow_layer thumb_shadow[] = { { 0, 0, 1, 2, 0, { 0, 0, 0, 0.25f } } };
    lp_draw_outer_shadows(cr, thumb, LP_RADIUS_PILL, thumb_shadow, 1);
    lp_fill_vgradient(cr, thumb, LP_SURFACE_RAISED_TOP, LP_SURFACE_RAISED_BOTTOM, LP_RADIUS_PILL);
    lp_draw_inset_shadows(cr, thumb, LP_RADIUS_PILL, LP_SHADOW_EMBOSS_RAISED, LP_SHADOW_EMBOSS_RAISED_COUNT);
    if (ctx->focus == id) lp_draw_focus_ring(cr, thumb, LP_RADIUS_PILL, accent.focus_ring, 2);
    for (int i = 0; i < n; i++) {
        lp_rect s = LP_RECT(inner.x + i * seg_w, inner.y, seg_w, inner.h);
        lp_text_style st = seg_style(size);
        int selected = i == *index;
        if (selected) { st.color = LP_INK_PRIMARY; st.emboss = 1; }
        float icon = options[i].icon < LP_ICON_COUNT ? (size == LP_CONTROL_SM ? 13 : 14) : 0;
        float text = options[i].label ? lp_text_measure(cr, options[i].label, &st).w : 0;
        float total = icon + (icon && text ? LP_SPACE_1 : 0) + text;
        float cx = s.x + (s.w - total) / 2;
        if (icon) { lp_icon_draw(cr, options[i].icon, cx, s.y + (s.h - icon) / 2, icon, 0, st.color); cx += icon + (text ? LP_SPACE_1 : 0); }
        if (text) lp_text_draw(cr, options[i].label, LP_RECT(cx, s.y, text + 2, s.h), &st, LP_ALIGN_START);
    }
    return changed;
}
