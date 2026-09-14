/* SegmentedControl — a well holding a sliding platinum thumb (web/src/components/SegmentedControl).
 * The thumb and the hover bead live in one merged layer, as SvgDefs' xxs goo filter renders them:
 * blurred together, the alpha cut back to one silhouette, a specular dome lit by the room light and
 * a shaded band inside the lower rim — so hovering the segment beside the selection necks a bead of
 * metal toward it before the thumb slides over. The thumb rides a spring that settles in about
 * motion.normal; the bead eases in over motion.fast and out over motion.normal (the web's .hotBlob
 * transitions). Immediate mode keeps no state between frames, so a small table keyed by the
 * control's id holds each spring. Everything moves in the DRAW pass and asks for its next frame
 * through lp_want_frame_rect; a control at rest, or outside the damage, costs nothing. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/components/lp_controls.h"
#include "maryui/lp_blur.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_motion.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_spring.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

/* The thumb's slide, in segment units so a control measured wider next frame keeps its place: a
 * spring that settles in about motion.normal (200 ms). */
#define THUMB_SPRING ((lp_spring_params){ 4.5f, 0.85f })
#define THUMB_TOLERANCE 0.003f
#define THUMB_VELOCITY_TOLERANCE 0.03f
/* SvgDefs' SIZE_SCALE.xxs: the thumb and the bead already touch, so the blur only fuses their join;
 * more would round the pill's own caps off. */
#define GOO_XXS 0.55f
#define BEAD_ALPHA 0.85f
#define SLOTS 24

struct slot { lp_id id; int valid; int index; lp_spring thumb; double last_ms; };
static struct slot slots[SLOTS];

static struct slot *slot_for(lp_id id, double now_ms) {
    for (int i = 0; i < SLOTS; i++) if (slots[i].valid && slots[i].id == id) return &slots[i];
    struct slot *take = &slots[0];
    for (int i = 0; i < SLOTS; i++) {
        if (!slots[i].valid) { take = &slots[i]; break; }
        if (slots[i].last_ms < take->last_ms) take = &slots[i];
    }
    memset(take, 0, sizeof *take);
    take->id = id;
    take->valid = 1;
    take->index = -1;
    take->last_ms = now_ms;
    return take;
}

/* Where the thumb is this frame, stepping its spring by the time since the last one. Without a clock
 * (a render, a test at 0) or with reduced motion it simply sits at the index. */
static float thumb_position(lp_ctx *ctx, lp_id id, int index, int *moving) {
    struct slot *s = slot_for(id, ctx->now_ms);
    if (s->index < 0) { s->thumb = lp_spring_make((float)index, (float)index); s->index = index; s->last_ms = ctx->now_ms; }
    if (s->index != index) { s->thumb.target = (float)index; s->index = index; }
    if (ctx->now_ms <= 0 || (ctx->settings && ctx->settings->reduced_motion)) {
        lp_spring_snap(&s->thumb);
        s->last_ms = ctx->now_ms;
        return s->thumb.value;
    }
    float dt = (float)((ctx->now_ms - s->last_ms) / 1000.0);
    s->last_ms = ctx->now_ms;
    if (dt < 0) dt = 0;
    if (dt > LP_MOTION_MAX_DT) dt = LP_MOTION_MAX_DT;
    if (!lp_spring_settled(&s->thumb, THUMB_TOLERANCE, THUMB_VELOCITY_TOLERANCE)) {
        lp_spring_step(&s->thumb, dt, THUMB_SPRING);
        if (lp_spring_settled(&s->thumb, THUMB_TOLERANCE, THUMB_VELOCITY_TOLERANCE)) lp_spring_snap(&s->thumb);
        else *moving = 1;
    }
    return s->thumb.value;
}

float lp_segmented_thumb(lp_id id) {
    for (int i = 0; i < SLOTS; i++) if (slots[i].valid && slots[i].id == id) return slots[i].thumb.value;
    return -1;
}

static lp_text_style seg_style(enum lp_control_size size) {
    lp_text_style st = lp_text_style_default();
    st.size_px = size == LP_CONTROL_SM ? LP_TEXT_SM : LP_TEXT_MD;
    st.weight = LP_TEXT_WEIGHT_MEDIUM;
    st.color = LP_INK_SECONDARY;
    return st;
}

static float segment_width(lp_ctx *ctx, const lp_segment *s, enum lp_control_size size) {
    float pad = size == LP_CONTROL_SM ? LP_SPACE_3 : LP_SPACE_4;
    float icon = s->icon < LP_ICON_COUNT ? (size == LP_CONTROL_SM ? 13 : 14) : 0;
    lp_text_style st = seg_style(size);
    float text = s->label ? (ctx->cr ? lp_text_measure(ctx->cr, s->label, &st).w : 7.0f * strlen(s->label)) : 0;
    return 2 * pad + icon + (icon && text ? LP_SPACE_1 : 0) + text;
}

lp_size lp_segmented_measure(lp_ctx *ctx, const lp_segment *options, int n, enum lp_control_size size) {
    float widest = 0;
    for (int i = 0; i < n; i++) widest = fmaxf(widest, segment_width(ctx, &options[i], size));
    /* The control has its own heights now: a segmented control is not a button. */
    float h = size == LP_CONTROL_SM ? LP_SIZE_SEGMENTED_HEIGHT_SM : LP_SIZE_SEGMENTED_HEIGHT;
    return (lp_size){ ceilf(widest) * n + 2 * LP_SIZE_SEGMENTED_PAD, h };
}

/* MARK: - The merged layer
 *
 * SvgDefs' Goo filter in Cairo, in device pixels: the thumb (its raised gradient) and the bead
 * (platinum.1 at its eased opacity) are blurred together, the alpha put through the tension's
 * slope and intercept (colours kept), a dome blurred off the silhouette is lit by the room light
 * (feSpecularLighting) and composited in, and the band inside the lower rim is shaded. Painted
 * back at the track. The layer is a few hundred pixels across and only ever built for a control
 * inside the damage, which is what D8's first attempt lacked. */
static void paint_merged(cairo_t *cr, lp_rect track, lp_rect thumb, const lp_rect *bead, float bead_alpha, int flow) {
    double dsx = 1, dsy = 1;
    cairo_surface_get_device_scale(cairo_get_target(cr), &dsx, &dsy);
    float s = dsx > 0 ? (float)dsx : 1;
    float blur = (flow ? LP_GOO_BLUR_FLOW : LP_GOO_BLUR_REST) * GOO_XXS;
    float slope = flow ? LP_GOO_SLOPE_FLOW : LP_GOO_SLOPE_REST, intercept = flow ? LP_GOO_INTERCEPT_FLOW : LP_GOO_INTERCEPT_REST;
    float rim = fmaxf(1, blur * 0.5f);
    int pad = (int)ceilf((3 * blur + rim + 2) * s);
    int w = (int)ceilf(track.w * s) + 2 * pad, h = (int)ceilf(track.h * s) + 2 * pad;
    cairo_surface_t *layer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(layer) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(layer); return; }
    cairo_t *lc = cairo_create(layer);
    cairo_scale(lc, s, s);
    cairo_translate(lc, pad / s - track.x, pad / s - track.y);
    if (bead && bead_alpha > 0) lp_fill_solid(lc, *bead, lp_color_with_alpha(LP_PLATINUM_1, bead_alpha), LP_RADIUS_PILL);
    lp_fill_vgradient(lc, thumb, LP_SURFACE_RAISED_TOP, LP_SURFACE_RAISED_BOTTOM, LP_RADIUS_PILL);
    cairo_destroy(lc);
    cairo_surface_flush(layer);
    lp_blur_surface(layer, blur * s);

    size_t count = (size_t)w * (size_t)h;
    float *shape = malloc(count * sizeof *shape), *dome = malloc(count * sizeof *dome), *scratch = malloc(count * sizeof *scratch);
    float *cr_ = malloc(count * sizeof *cr_), *cg = malloc(count * sizeof *cg), *cb = malloc(count * sizeof *cb);
    if (!shape || !dome || !scratch || !cr_ || !cg || !cb) {
        free(shape); free(dome); free(scratch); free(cr_); free(cg); free(cb);
        cairo_surface_destroy(layer);
        return;
    }
    unsigned char *data = cairo_image_surface_get_data(layer);
    int stride = cairo_image_surface_get_stride(layer);
    for (int y = 0; y < h; y++) {
        const uint32_t *row = (const uint32_t *)(data + (size_t)y * stride);
        for (int x = 0; x < w; x++) {
            uint32_t px = row[x];
            float a = (float)((px >> 24) & 255) / 255.0f;
            float r = (float)((px >> 16) & 255) / 255.0f, g = (float)((px >> 8) & 255) / 255.0f, b = (float)(px & 255) / 255.0f;
            size_t i = (size_t)y * w + x;
            float a2 = slope * a + intercept;
            shape[i] = a2 < 0 ? 0 : (a2 > 1 ? 1 : a2);
            /* straight colour, as feColorMatrix sees it */
            if (a > 0.001f) { cr_[i] = r / a; cg[i] = g / a; cb[i] = b / a; }
            else cr_[i] = cg[i] = cb[i] = 0;
        }
    }
    memcpy(dome, shape, count * sizeof *dome);
    lp_blur_plane(dome, w, h, rim * s, scratch);
    lp_distant_light light = lp_distant_light_make(LP_GOO_SPECULAR_AZIMUTH, LP_GOO_SPECULAR_ELEVATION);
    int rim_px = (int)lroundf(rim * s);
    for (int y = 0; y < h; y++) {
        uint32_t *row = (uint32_t *)(data + (size_t)y * stride);
        for (int x = 0; x < w; x++) {
            size_t i = (size_t)y * w + x;
            float a = shape[i];
            if (a <= 0) { row[x] = 0; continue; }
            float spec = (float)lp_specular_at(dome, w, h, x, y, LP_GOO_SPECULAR_SCALE * s, LP_GOO_SPECULAR_CONSTANT, LP_GOO_SPECULAR_EXPONENT, light);
            float r = cr_[i] + spec, g = cg[i] + spec, b = cb[i] + spec;
            if (r > 1) r = 1;
            if (g > 1) g = 1;
            if (b > 1) b = 1;
            /* the band inside the lower rim: the silhouette minus itself lifted by the rim */
            float lifted = y - rim_px >= 0 ? shape[i - (size_t)rim_px * w] : 0;
            float band = a * (1 - lifted), dark = LP_GOO_RIM_SHADE * band;
            float keep = 1 - dark;
            float A = dark + a * keep;
            r *= a * keep; g *= a * keep; b *= a * keep;
            row[x] = ((uint32_t)lroundf(A * 255) << 24) | ((uint32_t)lroundf(r * 255) << 16) | ((uint32_t)lroundf(g * 255) << 8) | (uint32_t)lroundf(b * 255);
        }
    }
    free(shape); free(dome); free(scratch); free(cr_); free(cg); free(cb);
    cairo_surface_mark_dirty(layer);
    cairo_save(cr);
    cairo_translate(cr, track.x - pad / s, track.y - pad / s);
    cairo_scale(cr, 1 / s, 1 / s);
    cairo_set_source_surface(cr, layer, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_surface_destroy(layer);
}

int lp_segmented(lp_ctx *ctx, lp_id id, float x, float y, const lp_segment *options, int n, int *index, enum lp_control_size size) {
    return lp_segmented_masked(ctx, id, x, y, options, n, index, size, 0);
}

int lp_segmented_masked(lp_ctx *ctx, lp_id id, float x, float y, const lp_segment *options, int n, int *index, enum lp_control_size size, unsigned disabled_mask) {
    lp_size sz = lp_segmented_measure(ctx, options, n, size);
    lp_rect track = LP_RECT(x, y, sz.w, sz.h);
    lp_rect inner = lp_rect_inset(track, LP_SIZE_SEGMENTED_PAD, LP_SIZE_SEGMENTED_PAD);
    float seg_w = inner.w / n;
    int changed = 0;
    for (int i = 0; i < n; i++) {
        lp_rect s = LP_RECT(inner.x + i * seg_w, inner.y, seg_w, inner.h);
        lp_id sid = lp_id_index(id, i);
        int disabled = (disabled_mask >> i) & 1u;
        if (!disabled) lp_hot(ctx, sid, s);
        if (lp_clicked(ctx, sid, s) && !disabled && *index != i) { *index = i; changed = 1; ctx->dirty = 1; }
    }
    if (ctx->pass == LP_PASS_EVENT && ctx->focus == id && ctx->in.key_pressed) {
        int d = ctx->in.keysym == 0xff53 ? 1 : (ctx->in.keysym == 0xff51 ? -1 : 0);
        int next = *index;
        for (int step = 0; d && step < n; step++) {
            next = (next + d + n) % n;
            if (!((disabled_mask >> next) & 1u)) break;
        }
        if (d && next != *index && !((disabled_mask >> next) & 1u)) { *index = next; changed = 1; ctx->dirty = 1; }
    }
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return changed;
    cairo_t *cr = ctx->cr;
    lp_rect reach = LP_RECT(track.x - 4, track.y - 4, track.w + 8, track.h + 8);
    if (!lp_clip_intersects(cr, reach)) return changed;
    lp_accent accent = lp_settings_accent(ctx->settings);
    lp_fill_solid(cr, track, LP_PLATINUM_3, LP_RADIUS_PILL);
    lp_draw_inset_shadows(cr, track, LP_RADIUS_PILL, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT);

    int moving = 0;
    float at = thumb_position(ctx, id, *index, &moving);
    lp_rect thumb = LP_RECT(inner.x + at * seg_w, inner.y, seg_w, inner.h);
    /* The bead: the hovered segment (never the selected one) rising, the one just left falling — the
     * strongest when two overlap. Hovering the track loosens the tension (the web's :hover → flow). */
    lp_rect bead = { 0 };
    float bead_alpha = 0, bead_strength = -1;
    int flow = 0;
    for (int i = 0; i < n; i++) {
        lp_id sid = lp_id_index(id, i);
        if ((disabled_mask >> i) & 1u) continue;
        if (ctx->hot == sid) flow = 1;
        if (i == *index) continue;
        int m = 0;
        float opacity = lp_hover_progress(ctx, sid, LP_MOTION_FAST_MS, LP_MOTION_NORMAL_MS, &m);
        float shape = lp_hover_progress(ctx, sid, LP_MOTION_NORMAL_MS, LP_MOTION_NORMAL_MS, &m);
        if (opacity <= 0 && shape <= 0) continue;
        moving |= m;
        if (opacity <= bead_strength) continue;
        bead_strength = opacity;
        /* .hotBlob[data-hot]: scale(.6) → translateX(±goo.attract) scale(1.02, 1.1) */
        float lean = (*index > i ? 1.0f : -1.0f) * LP_GOO_ATTRACT * shape;
        float sx = 0.6f + (1.02f - 0.6f) * shape, sy = 0.6f + (1.1f - 0.6f) * shape;
        lp_rect b = LP_RECT(inner.x + i * seg_w, inner.y, seg_w, inner.h);
        float cx = b.x + b.w / 2 + lean, cy = b.y + b.h / 2;
        bead = LP_RECT(cx - b.w * sx / 2, cy - b.h * sy / 2, b.w * sx, b.h * sy);
        bead_alpha = BEAD_ALPHA * opacity;
    }
    cairo_save(cr);
    lp_path_rrect(cr, track, LP_RADIUS_PILL);
    cairo_clip(cr);
    static const lp_shadow_layer thumb_shadow[] = { { 0, 0, 1, 2, 0, { 0, 0, 0, 0.25f } } };
    lp_draw_outer_shadows(cr, thumb, LP_RADIUS_PILL, thumb_shadow, 1);
    paint_merged(cr, track, thumb, bead_alpha > 0 ? &bead : NULL, bead_alpha, flow);
    cairo_restore(cr);
    if (ctx->focus == id) lp_draw_focus_ring(cr, LP_RECT(inner.x + (*index) * seg_w, inner.y, seg_w, inner.h), LP_RADIUS_PILL, accent.focus_ring, 2);
    for (int i = 0; i < n; i++) {
        lp_rect s = LP_RECT(inner.x + i * seg_w, inner.y, seg_w, inner.h);
        lp_text_style st = seg_style(size);
        /* the ink follows the thumb: how much of this segment it covers right now */
        float lo = fmaxf(s.x, thumb.x), hi = fminf(s.x + s.w, thumb.x + thumb.w);
        float coverage = hi > lo ? (hi - lo) / seg_w : 0;
        st.color = lp_color_mix(LP_INK_SECONDARY, LP_INK_PRIMARY, coverage);
        st.emboss = coverage > 0.5f;
        if ((disabled_mask >> i) & 1u) { st.color = LP_INK_DISABLED; st.emboss = 0; }
        float icon = options[i].icon < LP_ICON_COUNT ? (size == LP_CONTROL_SM ? 13 : 14) : 0;
        float text = options[i].label ? lp_text_measure(cr, options[i].label, &st).w : 0;
        float total = icon + (icon && text ? LP_SPACE_1 : 0) + text;
        float cx = s.x + (s.w - total) / 2;
        if (icon) { lp_icon_draw(cr, options[i].icon, cx, s.y + (s.h - icon) / 2, icon, 0, st.color); cx += icon + (text ? LP_SPACE_1 : 0); }
        if (text) lp_text_draw(cr, options[i].label, LP_RECT(cx, s.y, text + 2, s.h), &st, LP_ALIGN_START);
    }
    if (moving) lp_want_frame_rect(ctx, reach);
    return changed;
}
