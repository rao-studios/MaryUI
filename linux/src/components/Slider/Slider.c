#include <math.h>
#include <stdio.h>
#include <string.h>

#include "maryui/components/lp_controls.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

static float lp_clamp_local(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float snap(float v, float min, float max, float step) {
    if (step > 0) v = min + roundf((v - min) / step) * step;
    return lp_clamp_local(v, min, max);
}

int lp_slider(lp_ctx *ctx, lp_id id, lp_rect r, float *value, lp_slider_opts o) {
    if (o.min == 0 && o.max == 0 && o.step == 0) { o.max = 100; o.step = 1; }
    lp_text_style label = lp_text_style_default();
    label.size_px = LP_TEXT_SM; label.color = LP_INK_SECONDARY;
    lp_text_style val = lp_text_style_default();
    val.font = LP_FONT_MONO; val.size_px = LP_TEXT_XS; val.color = LP_INK_TERTIARY; val.tabular_nums = 1;
    char text[32] = "";
    if (o.show_value) {
        if (o.format) o.format(*value, text, sizeof text);
        else if (fabsf(*value - roundf(*value)) < 1e-4f) snprintf(text, sizeof text, "%d", (int)roundf(*value));
        else snprintf(text, sizeof text, "%.2f", *value);
    }
    /* grid auto 1fr auto, gap space.2 */
    float lw = o.label ? (ctx->cr ? lp_text_measure(ctx->cr, o.label, &label).w : 7.0f * strlen(o.label)) : 0;
    float vw = o.show_value ? fmaxf(ctx->cr ? lp_text_measure(ctx->cr, text, &val).w : 7.0f * strlen(text), 3.5f * 6.5f) : 0;
    lp_rect rail_area = r;
    if (o.label) { rail_area.x += lw + LP_SPACE_2; rail_area.w -= lw + LP_SPACE_2; }
    if (o.show_value) rail_area.w -= vw + LP_SPACE_2;
    float pct = o.max > o.min ? (*value - o.min) / (o.max - o.min) : 0;
    lp_rect rail = LP_RECT(rail_area.x + 8, r.y + (r.h - 5) / 2, rail_area.w - 16, 5);
    float thumb_x = rail.x + pct * rail.w;
    lp_rect hit = LP_RECT(rail_area.x, r.y, rail_area.w, r.h);

    int changed = 0;
    if (!o.disabled) {
        lp_hot(ctx, id, hit);
        if (ctx->pass == LP_PASS_EVENT) {
            if (lp_hit(ctx, hit) && (ctx->in.pressed & LP_BUTTON_LEFT)) { ctx->active = id; ctx->dirty = 1; }
            if (ctx->active == id && (ctx->in.buttons & LP_BUTTON_LEFT) && !isnan(ctx->in.mx)) {
                float p = rail.w > 0 ? (ctx->in.mx - rail.x) / rail.w : 0;
                float next = snap(o.min + lp_clamp_local(p, 0, 1) * (o.max - o.min), o.min, o.max, o.step);
                if (next != *value) { *value = next; changed = 1; }
                ctx->dirty = 1;
            }
            if (ctx->focus == id && ctx->in.key_pressed) {
                float d = ctx->in.keysym == 0xff53 ? 1 : (ctx->in.keysym == 0xff51 ? -1 : 0); /* Right / Left */
                if (d) { float next = snap(*value + d * (o.step > 0 ? o.step : 1), o.min, o.max, o.step); if (next != *value) { *value = next; changed = 1; } ctx->dirty = 1; }
            }
        }
    }
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return changed;
    cairo_t *cr = ctx->cr;
    lp_accent accent = lp_settings_accent(ctx->settings);
    cairo_save(cr);
    if (o.disabled) cairo_push_group(cr);
    if (o.label) lp_text_draw(cr, o.label, LP_RECT(r.x, r.y, lw + 2, r.h), &label, LP_ALIGN_START);
    lp_fill_hsplit(cr, rail, accent.base, LP_PLATINUM_4, pct, LP_RADIUS_PILL);
    lp_draw_inset_shadows(cr, rail, LP_RADIUS_PILL, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT);
    /* thumb: 16px, radial platinum.0 → platinum.3 70% → platinum.5 */
    lp_rect thumb = LP_RECT(thumb_x - 8, rail.y + rail.h / 2 - 8, 16, 16);
    static const lp_shadow_layer thumb_shadow[] = { { 0, 0, 0, 0, 1, { 0, 0, 0, 0.32f } }, { 0, 0, 1, 3, 0, { 0, 0, 0, 0.3f } } };
    lp_draw_outer_shadows(cr, thumb, 8, thumb_shadow, 2);
    cairo_pattern_t *p = cairo_pattern_create_radial(thumb.x + 8, thumb.y + 4.8, 0, thumb.x + 8, thumb.y + 4.8, 10);
    lp_color c0 = LP_PLATINUM_0, c3 = LP_PLATINUM_3, c5 = LP_PLATINUM_5;
    cairo_pattern_add_color_stop_rgba(p, 0, c0.r, c0.g, c0.b, 1);
    cairo_pattern_add_color_stop_rgba(p, 0.7, c3.r, c3.g, c3.b, 1);
    cairo_pattern_add_color_stop_rgba(p, 1, c5.r, c5.g, c5.b, 1);
    cairo_arc(cr, thumb.x + 8, thumb.y + 8, 8, 0, 2 * M_PI);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
    lp_draw_inset_shadows(cr, thumb, 8, LP_SHADOW_EMBOSS_RAISED, LP_SHADOW_EMBOSS_RAISED_COUNT);
    if (ctx->focus == id && !o.disabled) lp_draw_focus_ring(cr, thumb, 8, accent.focus_ring, 3);
    if (o.show_value) lp_text_draw(cr, text, LP_RECT(r.x + r.w - vw, r.y, vw, r.h), &val, LP_ALIGN_END);
    if (o.disabled) { cairo_pop_group_to_source(cr); cairo_paint_with_alpha(cr, 0.5); }
    cairo_restore(cr);
    return changed;
}
