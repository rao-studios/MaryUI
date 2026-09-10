#include <math.h>
#include <string.h>

#include "maryui/components/lp_button.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

static lp_text_style label_style(lp_button_opts o) {
    lp_text_style s = lp_text_style_default();
    s.weight = LP_TEXT_WEIGHT_MEDIUM;
    s.size_px = o.size == LP_CONTROL_SM ? LP_TEXT_SM : LP_TEXT_MD;
    s.emboss = 1;
    return s;
}

lp_size lp_button_measure(lp_ctx *ctx, const char *label, lp_button_opts o) {
    float h = o.size == LP_CONTROL_SM ? LP_SIZE_CONTROL_HEIGHT_SM : LP_SIZE_CONTROL_HEIGHT;
    float pad = o.size == LP_CONTROL_SM ? LP_SPACE_2 : LP_SPACE_3;
    float icon = o.icon < LP_ICON_COUNT ? 16 : 0;
    if (o.icon_only) return (lp_size){ h, h };
    lp_text_style st = label_style(o);
    float text = ctx->cr && label ? lp_text_measure(ctx->cr, label, &st).w : (label ? strlen(label) * 7.0f : 0);
    float w = 2 * pad + icon + (icon && text ? LP_SPACE_1 : 0) + text;
    return (lp_size){ fmaxf(w, h), h };
}

int lp_button(lp_ctx *ctx, lp_id id, lp_rect r, const char *label, lp_button_opts o) {
    if (o.disabled) {
        lp_hot(ctx, 0, LP_RECT(0, 0, 0, 0));
    }
    int clicked = o.disabled ? 0 : lp_clicked(ctx, id, r);
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return clicked;
    cairo_t *cr = ctx->cr;
    int hot = !o.disabled && lp_is_hot(ctx, id);
    int active = !o.disabled && lp_is_active(ctx, id) && hot;
    lp_accent accent = lp_settings_accent(ctx->settings);
    float radius = o.icon_only ? lp_radius_flex(ctx, LP_RADIUS_SM) : LP_RADIUS_PILL;
    /* Pressing an icon button rounds it further: the squish under a thumb. */
    if (o.icon_only && lp_is_active(ctx, id)) radius *= LP_RADIUS_FLEX_PRESS;
    lp_color top = LP_SURFACE_RAISED_TOP, bottom = LP_SURFACE_RAISED_BOTTOM, ink = LP_INK_PRIMARY, emboss = LP_INK_EMBOSS;
    if (hot) { top = LP_PLATINUM_0; bottom = LP_PLATINUM_2; }
    if (active) { top = LP_SURFACE_PRESSED_TOP; bottom = LP_SURFACE_PRESSED_BOTTOM; }
    if (o.variant == LP_BUTTON_PRIMARY) {
        top = accent.light; bottom = accent.base; ink = LP_INK_ON_ACCENT; emboss = LP_RGBA(0, 0, 0, 0.25f);
        if (hot) { top = accent.light; bottom = accent.deep; }
        if (active) { top = accent.deep; bottom = accent.base; }
    }
    if (o.disabled) ink = LP_INK_DISABLED;
    float alpha = o.disabled ? 0.7f : 1;
    lp_rect box = r;
    if (active) box.y += 0.5f;

    int paint_metal = o.variant != LP_BUTTON_QUIET || hot;
    cairo_save(cr);
    if (alpha < 1) cairo_push_group(cr);
    if (paint_metal) {
        static const lp_shadow_layer outer[] = { { 0, 0, 0, 0, 1, { 0, 0, 0, 0.32f } }, { 0, 0, 1, 2, 0, { 0, 0, 0, 0.18f } } };
        if (o.variant == LP_BUTTON_QUIET) lp_draw_outer_shadows(cr, box, radius, outer, 1);
        else if (!active) lp_draw_outer_shadows(cr, box, radius, outer, 2);
        else lp_draw_outer_shadows(cr, box, radius, outer, 1);
        lp_fill_vgradient(cr, box, top, bottom, radius);
        if (o.variant != LP_BUTTON_QUIET) lp_draw_brush(cr, box, radius, LP_BRUSH_OPACITY * 0.8f, ctx->world_x, ctx->world_y);
        if (active) lp_draw_inset_shadows(cr, box, radius, LP_SHADOW_EMBOSS_PRESSED, LP_SHADOW_EMBOSS_PRESSED_COUNT);
        else lp_draw_inset_shadows(cr, box, radius, LP_SHADOW_EMBOSS_RAISED, LP_SHADOW_EMBOSS_RAISED_COUNT);
    }
    if (ctx->focus == id && !o.disabled) lp_draw_focus_ring(cr, box, radius, accent.focus_ring, 3);

    /* content: icon + label, centred */
    lp_text_style st = label_style(o);
    st.color = ink;
    st.emboss_color = emboss;
    float icon = o.icon < LP_ICON_COUNT ? 16 : 0;
    float text_w = !o.icon_only && label ? lp_text_measure(cr, label, &st).w : 0;
    float total = icon + (icon && text_w ? LP_SPACE_1 : 0) + text_w;
    float x = box.x + (box.w - total) / 2;
    if (icon) {
        lp_color ic = o.variant == LP_BUTTON_PRIMARY ? ink : (o.disabled ? LP_INK_DISABLED : LP_INK_SECONDARY);
        lp_icon_draw(cr, o.icon, x, box.y + (box.h - 16) / 2, 16, 0, ic);
        x += icon + LP_SPACE_1;
    }
    if (text_w) lp_text_draw(cr, label, LP_RECT(x, box.y, text_w + 2, box.h), &st, LP_ALIGN_START);
    if (alpha < 1) { cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, alpha); }
    cairo_restore(cr);
    return clicked;
}
