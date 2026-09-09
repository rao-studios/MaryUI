#include <math.h>
#include <stdlib.h>

#include "maryui/components/lp_goo_group.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_tokens.h"

lp_rect lp_goo_blob_rect(lp_rect area, const lp_goo_spec *spec, int i) {
    float w = spec->blob_size, h = spec->blob_size;
    if (spec->shape == LP_GOO_FILL) { w = (area.w - spec->gap * (spec->count - 1)) / spec->count; h = area.h; }
    float x = area.x + i * (w + spec->gap);
    float y = area.y + (area.h - h) / 2;
    return LP_RECT(x, y, w, h);
}

void lp_goo_group(lp_ctx *ctx, lp_rect area, const lp_goo_spec *spec) {
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    cairo_t *cr = ctx->cr;
    for (int i = 0; i < spec->count; i++) {
        lp_rect r = lp_goo_blob_rect(area, spec, i);
        float scale = 1;
        if (spec->hot == i) scale = 1.22f;
        else if (spec->hot >= 0 && abs(spec->hot - i) == 1) scale = 1.08f;
        float smear = ctx->vx * spec->smear * i;
        float cx = r.x + r.w / 2 + smear, cy = r.y + r.h / 2;
        lp_rect scaled = LP_RECT(cx - r.w * scale / 2, cy - r.h * scale / 2, r.w * scale, r.h * scale);
        float radius = spec->shape == LP_GOO_CIRCLE ? scaled.h / 2 : (spec->shape == LP_GOO_PILL ? LP_RADIUS_PILL : LP_RADIUS_SM);
        lp_fill_vgradient(cr, scaled, LP_SURFACE_RAISED_TOP, LP_PLATINUM_5, radius);
        lp_draw_inset_shadows(cr, scaled, radius, LP_SHADOW_EMBOSS_RAISED, LP_SHADOW_EMBOSS_RAISED_COUNT);
    }
}
