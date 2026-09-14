/* SegmentedControl — a well holding a sliding platinum thumb (web/src/components/SegmentedControl).
 * The thumb and the hover bead live in one merged layer, as SvgDefs' xxs goo filter renders them:
 * blurred together, the alpha cut back to one silhouette, a specular dome lit by the room light and
 * a shaded band inside the lower rim — so hovering the segment beside the selection necks a bead of
 * metal toward it before the thumb slides over. The thumb rides a spring that settles in about
 * motion.normal; the bead eases in over motion.fast and out over motion.normal (the web's .hotBlob
 * transitions). Immediate mode keeps no state between frames, so a small table keyed by the
 * control's id holds each spring. Everything moves in the DRAW pass and asks for its next frame
 * through lp_want_motion_rect, which the compositor serves every refresh; a control at rest, or outside
 * the damage, costs nothing. The EVENT pass
 * measures the same geometry the DRAW pass draws, and a hover change damages the whole track, since
 * the merge reshapes the thumb as well as the bead. */
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
#define SEGMENTS_MAX 16

/* Per control: the thumb's spring; whether the pointer is over the track (the EVENT pass sees the pointer,
 * the DRAW pass does not — it sees this); and the beads the last frame drew, for tests. */
/* The beads and the tension ease on critically damped springs, stepped in closed form: continuous in value
 * and speed however the hover moves (a quick sweep across three segments, a reversal halfway, a click on the
 * hovered segment) and exact for any gap between frames. ctx's hover history remembers one segment back,
 * so a curve read off it dropped the bead two segments back to nothing in a frame. 3.89/ω is the time to
 * 90%, set to motion.fast (opacity) and motion.normal (shape, tension). */
#define OMEGA_FAST (3.89f / (LP_MOTION_FAST_MS / 1000.0f))
#define OMEGA_NORMAL (3.89f / (LP_MOTION_NORMAL_MS / 1000.0f))
struct slot {
    lp_id id; int valid; int index; lp_spring thumb; double last_ms;
    int flow; lp_spring tension;
    lp_spring opacity[SEGMENTS_MAX], shape[SEGMENTS_MAX];
    float bead[SEGMENTS_MAX];
};
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

/* Where the thumb is this frame, stepping its spring by `dt` seconds (the time since the control's last
 * frame). Without a clock (a render, a test at 0) or with reduced motion it simply sits at the index. */
static float thumb_position(struct slot *s, int index, float dt, int snap, int *moving) {
    if (s->index < 0) { s->thumb = lp_spring_make((float)index, (float)index); s->index = index; }
    if (s->index != index) { s->thumb.target = (float)index; s->index = index; }
    if (snap) {
        lp_spring_snap(&s->thumb);
        return s->thumb.value;
    }
    if (dt < 0) dt = 0;
    if (dt > LP_MOTION_MAX_DT) dt = LP_MOTION_MAX_DT;
    if (!lp_spring_settled(&s->thumb, THUMB_TOLERANCE, THUMB_VELOCITY_TOLERANCE)) {
        lp_spring_step(&s->thumb, dt, THUMB_SPRING);
        if (lp_spring_settled(&s->thumb, THUMB_TOLERANCE, THUMB_VELOCITY_TOLERANCE)) lp_spring_snap(&s->thumb);
        else *moving = 1;
    }
    return s->thumb.value;
}

/* A critically damped step toward `target`, exact for any dt. 1 while it still moves. */
static int ease_to(lp_spring *sp, float target, float omega, float dt, int snap) {
    sp->target = target;
    if (snap) { lp_spring_snap(sp); return 0; }
    if (dt > 0) {
        float c1 = sp->value - target, c2 = sp->velocity + omega * c1, e = (float)exp(-(double)omega * dt);
        sp->value = target + (c1 + c2 * dt) * e;
        sp->velocity = (c2 - omega * (c1 + c2 * dt)) * e;
    }
    if (fabsf(sp->value - target) < 0.002f && fabsf(sp->velocity) < 0.05f) { lp_spring_snap(sp); return 0; }
    return 1;
}

static float unit(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

float lp_segmented_thumb(lp_id id) {
    for (int i = 0; i < SLOTS; i++) if (slots[i].valid && slots[i].id == id) return slots[i].thumb.value;
    return -1;
}

float lp_segmented_bead(lp_id id, int segment) {
    if (segment < 0 || segment >= SEGMENTS_MAX) return 0;
    for (int i = 0; i < SLOTS; i++) if (slots[i].valid && slots[i].id == id) return slots[i].bead[segment];
    return 0;
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
    /* measured the same way in both passes (lp_text_measure keeps a scratch context for the EVENT pass): an
     * estimate here put the hit rects and the damage a few pixels off the thumb that was drawn */
    float text = s->label ? lp_text_measure(ctx->cr, s->label, &st).w : 0;
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
 * inside the damage, which is what D8's first attempt lacked. Every bead goes in (the one rising and
 * the one falling at a crossing merge through the same filter, as the web's per-segment .hotBlobs do).
 * `flow` is the tension between rest (0) and flow (1), eased: switching filters outright made the whole
 * silhouette jump when the pointer crossed the track's edge.
 *
 * The cut is anti-aliased. At the tokens' slope the silhouette's edge is a tenth of a pixel wide: a
 * thumb sliding by fractions of a pixel pops whole edge pixels on and off (the jitter), and a fading bead
 * appears at full size the frame its alpha crosses the cut (the flash). The slope is therefore held to
 * what keeps the edge one device pixel wide — a blurred step's alpha climbs at most 1/(σ√2π) a pixel —
 * with the intercept moved so the cut stays where the tokens put it. */
static void paint_merged(cairo_t *cr, lp_rect track, lp_rect thumb, const lp_rect *beads, const float *bead_alpha, int nbeads, float flow) {
    double dsx = 1, dsy = 1;
    cairo_surface_get_device_scale(cairo_get_target(cr), &dsx, &dsy);
    float s = dsx > 0 ? (float)dsx : 1;
    flow = unit(flow);
    float blur = (LP_GOO_BLUR_REST + (LP_GOO_BLUR_FLOW - LP_GOO_BLUR_REST) * flow) * GOO_XXS;
    float slope = LP_GOO_SLOPE_REST + (LP_GOO_SLOPE_FLOW - LP_GOO_SLOPE_REST) * flow;
    float intercept = LP_GOO_INTERCEPT_REST + (LP_GOO_INTERCEPT_FLOW - LP_GOO_INTERCEPT_REST) * flow;
    float cut = (0.5f - intercept) / slope;                 /* the alpha the tokens cut at (about 0.44) */
    float edge_slope = 2.5f * blur * s;                     /* σ√2π in device pixels: a one-pixel edge */
    if (slope > edge_slope) { slope = edge_slope; intercept = 0.5f - cut * slope; }
    float rim = fmaxf(1, blur * 0.5f);
    int pad = (int)ceilf((3 * blur + rim + 2) * s);
    int w = (int)ceilf(track.w * s) + 2 * pad, h = (int)ceilf(track.h * s) + 2 * pad;
    cairo_surface_t *layer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(layer) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(layer); return; }
    cairo_t *lc = cairo_create(layer);
    cairo_scale(lc, s, s);
    cairo_translate(lc, pad / s - track.x, pad / s - track.y);
    for (int i = 0; i < nbeads; i++)
        if (bead_alpha[i] > 0) lp_fill_solid(lc, beads[i], lp_color_with_alpha(LP_PLATINUM_1, bead_alpha[i]), LP_RADIUS_PILL);
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
    lp_rect reach = LP_RECT(track.x - 4, track.y - 4, track.w + 8, track.h + 8);
    float seg_w = inner.w / n;
    int changed = 0, hot_changed = 0;
    struct slot *slot = slot_for(id, ctx->now_ms);
    for (int i = 0; i < n; i++) {
        lp_rect s = LP_RECT(inner.x + i * seg_w, inner.y, seg_w, inner.h);
        lp_id sid = lp_id_index(id, i);
        int disabled = (disabled_mask >> i) & 1u;
        int was = ctx->hot == sid;
        if (!disabled && lp_hot(ctx, sid, s) != was) hot_changed = 1;
        if (lp_clicked(ctx, sid, s) && !disabled && *index != i) { *index = i; changed = 1; ctx->dirty = 1; }
    }
    if (ctx->pass == LP_PASS_EVENT) {
        /* The merge reaches past the hovered segment — a bead necking into the thumb reshapes the thumb's
         * cap — so a hover change repaints the whole track, not the two segments the host would damage.
         * The tension follows the pointer over the track (.track:hover → flow), remembered here because the
         * DRAW pass has no pointer. */
        int flow = lp_hit(ctx, track);
        if (flow != slot->flow) { slot->flow = flow; hot_changed = 1; }
        if (hot_changed) lp_damage(ctx, reach);
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
    if (!lp_clip_intersects(cr, reach)) return changed;
    lp_accent accent = lp_settings_accent(ctx->settings);
    lp_fill_solid(cr, track, LP_PLATINUM_3, LP_RADIUS_PILL);
    lp_draw_inset_shadows(cr, track, LP_RADIUS_PILL, LP_SHADOW_EMBOSS_WELL, LP_SHADOW_EMBOSS_WELL_COUNT);

    int moving = 0;
    int snap = ctx->now_ms <= 0 || (ctx->settings && ctx->settings->reduced_motion);
    float dt = (float)((ctx->now_ms - slot->last_ms) / 1000.0);
    slot->last_ms = ctx->now_ms;
    if (dt < 0) dt = 0;
    float at = thumb_position(slot, *index, dt, snap, &moving);
    lp_rect thumb = LP_RECT(inner.x + at * seg_w, inner.y, seg_w, inner.h);
    moving |= ease_to(&slot->tension, slot->flow ? 1.0f : 0.0f, OMEGA_NORMAL, dt, snap);
    /* The beads: the hovered segment (never the selected one) rising, any other falling from wherever it
     * had got to — all in the layer, so a crossing is one bead flowing into the next. */
    lp_rect beads[SEGMENTS_MAX];
    float bead_alpha[SEGMENTS_MAX];
    int nbeads = n < SEGMENTS_MAX ? n : SEGMENTS_MAX;
    memset(slot->bead, 0, sizeof slot->bead);
    for (int i = 0; i < nbeads; i++) {
        lp_id sid = lp_id_index(id, i);
        bead_alpha[i] = 0;
        beads[i] = LP_RECT(0, 0, 0, 0);
        float on = !((disabled_mask >> i) & 1u) && i != *index && ctx->hot == sid ? 1.0f : 0.0f;
        moving |= ease_to(&slot->opacity[i], on, OMEGA_FAST, dt, snap);
        moving |= ease_to(&slot->shape[i], on, OMEGA_NORMAL, dt, snap);
        float opacity = unit(slot->opacity[i].value), shape = unit(slot->shape[i].value);
        if (opacity <= 0.001f) continue;
        /* .hotBlob[data-hot]: scale(.6) → translateX(±goo.attract) scale(1.02, 1.1), leaning toward where the
         * thumb is now (the web's clamp(index − i, −1, 1), followed through the slide rather than jumping) */
        float toward = at - (float)i;
        float lean = (toward > 1 ? 1 : toward < -1 ? -1 : toward) * LP_GOO_ATTRACT * shape;
        float sx = 0.6f + (1.02f - 0.6f) * shape, sy = 0.6f + (1.1f - 0.6f) * shape;
        lp_rect b = LP_RECT(inner.x + i * seg_w, inner.y, seg_w, inner.h);
        float cx = b.x + b.w / 2 + lean, cy = b.y + b.h / 2;
        beads[i] = LP_RECT(cx - b.w * sx / 2, cy - b.h * sy / 2, b.w * sx, b.h * sy);
        bead_alpha[i] = slot->bead[i] = BEAD_ALPHA * opacity;
    }
    cairo_save(cr);
    lp_path_rrect(cr, track, LP_RADIUS_PILL);
    cairo_clip(cr);
    static const lp_shadow_layer thumb_shadow[] = { { 0, 0, 1, 2, 0, { 0, 0, 0, 0.25f } } };
    lp_draw_outer_shadows(cr, thumb, LP_RADIUS_PILL, thumb_shadow, 1);
    paint_merged(cr, track, thumb, beads, bead_alpha, nbeads, slot->tension.value);
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
    if (moving) lp_want_motion_rect(ctx, reach);   /* every refresh: 30 Hz shows a slide as steps */
    return changed;
}
