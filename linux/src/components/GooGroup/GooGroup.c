#include <math.h>
#include <stdlib.h>

#include "maryui/components/lp_goo_group.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_goo.h"
#include "maryui/lp_tokens.h"

lp_rect lp_goo_blob_rect(lp_rect area, const lp_goo_spec *spec, int i) {
    float w = spec->blob_size, h = spec->blob_size;
    if (spec->shape == LP_GOO_FILL) { w = (area.w - spec->gap * (spec->count - 1)) / spec->count; h = area.h; }
    float x = area.x + i * (w + spec->gap);
    float y = area.y + (area.h - h) / 2;
    return LP_RECT(x, y, w, h);
}

static float blob_radius(const lp_goo_spec *spec, lp_rect r) {
    return spec->shape == LP_GOO_CIRCLE ? r.h / 2 : (spec->shape == LP_GOO_PILL ? LP_RADIUS_PILL : LP_RADIUS_SM);
}

/* The default blob: blank metal. It carries no emboss — the filter lights it. */
static void default_blob(cairo_t *cr, lp_rect r, float radius) {
    lp_fill_vgradient(cr, r, LP_SURFACE_RAISED_TOP, LP_PLATINUM_5, radius);
}

/* Where blob i actually lands once hover and the drag's shear have moved it. */
static lp_rect blob_placement(lp_ctx *ctx, lp_rect area, const lp_goo_spec *spec, int i) {
    lp_rect r = lp_goo_blob_rect(area, spec, i);
    float scale = 1;
    float pull = 0;
    if (spec->hot == i) {
        scale = 1.22f;
    } else if (spec->hot >= 0 && abs(spec->hot - i) == 1) {
        scale = 1.08f;
        /* Neighbours lean toward the hot blob, so the pair reads as one mass. */
        pull = (spec->hot > i ? 1.0f : -1.0f) * LP_GOO_ATTRACT;
    }
    /* The drag smear rides the shear (vx the liquid has not caught up with),
     * trailing against travel and elongating with it. */
    float lag = spec->still ? 0.0f : ctx->vx - ctx->vx_lag;
    float smear = lag * spec->smear * (i * 0.5f + 1.0f) * -1.0f;
    float stretch = 1.0f + fabsf(lag) * LP_GOO_STRETCH;
    float cx = r.x + r.w / 2 + smear + pull, cy = r.y + r.h / 2;
    float bw = r.w * scale * stretch, bh = r.h * scale;
    return LP_RECT(cx - bw / 2, cy - bh / 2, bw, bh);
}

void lp_goo_group(lp_ctx *ctx, lp_rect area, const lp_goo_spec *spec) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;

    /* The filter needs room to bleed into: SvgDefs' region is -25% / -45%. */
    int pad_x = (int)ceilf(area.w * 0.25f), pad_y = (int)ceilf(area.h * 0.45f);
    int lw = (int)ceilf(area.w) + 2 * pad_x, lh = (int)ceilf(area.h) + 2 * pad_y;
    cairo_surface_t *layer = lw > 0 && lh > 0 ? cairo_image_surface_create(CAIRO_FORMAT_ARGB32, lw, lh) : NULL;
    if (!layer || cairo_surface_status(layer) != CAIRO_STATUS_SUCCESS) {
        if (layer) cairo_surface_destroy(layer);
        return;
    }
    cairo_t *lc = cairo_create(layer);
    cairo_translate(lc, pad_x - area.x, pad_y - area.y);

    for (int i = 0; i < spec->count; i++) {
        lp_rect placed = blob_placement(ctx, area, spec, i);
        if (spec->render_blob) {
            cairo_save(lc);
            cairo_t *saved = ctx->cr;
            ctx->cr = lc;
            spec->render_blob(ctx, placed, i, spec->user);
            ctx->cr = saved;
            cairo_restore(lc);
        } else {
            default_blob(lc, placed, blob_radius(spec, placed));
        }
    }
    cairo_destroy(lc);

    enum lp_goo_tension tension = spec->flowing || spec->hot >= 0 ? LP_GOO_FLOW : LP_GOO_REST;
    lp_goo_filter(layer, lp_goo_blur((int)spec->size, tension), tension);

    cairo_set_source_surface(ctx->cr, layer, area.x - pad_x, area.y - pad_y);
    cairo_paint(ctx->cr);
    cairo_surface_destroy(layer);
}
