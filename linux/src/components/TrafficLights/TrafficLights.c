#include <string.h>

#include "maryui/components/lp_goo_group.h"
#include "maryui/components/lp_liquid_bubble.h"
#include "maryui/components/lp_traffic_lights.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_tokens.h"

/* The bead is the whole light now: no rim, so the blob behind it is the same
 * size. Both layers share one gap, and their pitches only line up when the
 * items are the same width. */
#define BEAD LP_SIZE_TRAFFIC
#define GAP LP_SIZE_TRAFFIC_GAP

/* Shading drains the well, so a collapsed window reads at a glance. */
#define SHADED_FILL (LP_LIQUID_FILL_TRAFFIC * 0.45f)

/* Phases keep the three surfaces from rolling in lockstep. */
static const float PHASES[3] = { 0, 2.3f, 4.1f };
static const enum lp_bubble_tint TINTS[3] = { LP_TINT_CLOSE, LP_TINT_MINIMIZE, LP_TINT_ZOOM };

lp_size lp_traffic_lights_size(void) { return (lp_size){ 3 * BEAD + 2 * GAP, BEAD }; }

void lp_traffic_lights(lp_ctx *ctx, float x, float y, int active, int shaded, int zoomed, lp_traffic_result *out) {
    lp_traffic_result res;
    memset(&res, 0, sizeof res);
    lp_size size = lp_traffic_lights_size();
    res.bounds = LP_RECT(x, y, size.w, size.h);
    /* respondsToMotion: false — the beads never smear, never scale, never fade. */
    lp_goo_spec goo = { .size = LP_GOO_XS, .count = 3, .blob_size = BEAD, .shape = LP_GOO_CIRCLE, .gap = GAP, .hot = -1, .still = 1 };
    lp_id base = LP_ID("traffic");
    int clicked[3] = { 0, 0, 0 };
    int hovered_light = -1;
    for (int i = 0; i < 3; i++) {
        lp_rect r = lp_goo_blob_rect(res.bounds, &goo, i);
        lp_id id = lp_id_index(base, i);
        clicked[i] = lp_clicked(ctx, id, r);
        if (lp_is_hot(ctx, id) || (ctx->pass == LP_PASS_EVENT && ctx->next_hot == id)) hovered_light = i;
    }
    res.close = clicked[0];
    res.shade = clicked[1];
    res.zoom = clicked[2];
    res.hovered = lp_hit(ctx, res.bounds);
    if (out) *out = res;
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return;

    int drained = !active && !res.hovered;
    /* Nothing is painted behind the beads: the web's hidden merge layer is not
     * ported (PARITY.md D8). hovered_light stays in the hit-test above so the
     * ids and the click routing match the web's. */
    (void)hovered_light;

    const char *glyphs[3] = { "×", "–", zoomed ? "−" : "+" };
    static const lp_shadow_layer bead_shadow[] = { { 0, 0, 1, 2, 0, { 0, 0, 0, 0.35f } } };
    for (int i = 0; i < 3; i++) {
        lp_rect r = lp_goo_blob_rect(res.bounds, &goo, i);
        lp_bubble_spec spec = lp_liquid_bubble_spec(ctx, BEAD, drained ? LP_TINT_INACTIVE : TINTS[i], PHASES[i],
                                                    i == 1 && shaded ? SHADED_FILL : LP_LIQUID_FILL_TRAFFIC);
        spec.glyph = glyphs[i];
        spec.glyph_alpha = res.hovered ? 1.0f : 0.0f;
        spec.glyph_scale = 0.6f;
        spec.glyph_ink = 0.5f;
        /* The same drop shadow that sits a Toggle's knob proud of its track. */
        lp_draw_outer_shadows(ctx->cr, r, r.h / 2, bead_shadow, 1);
        lp_bubble_paint(ctx->cr, r.x + r.w / 2, r.y + r.h / 2, &spec);
    }
}
