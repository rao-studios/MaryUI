#include <string.h>

#include "maryui/components/lp_goo_group.h"
#include "maryui/components/lp_liquid_bubble.h"
#include "maryui/components/lp_traffic_lights.h"
#include "maryui/lp_tokens.h"

#define BUBBLE LP_SIZE_TRAFFIC
#define RIM (LP_SIZE_TRAFFIC + 2)
#define GAP (LP_SIZE_TRAFFIC_GAP - 2)

lp_size lp_traffic_lights_size(void) { return (lp_size){ 3 * RIM + 2 * GAP, RIM }; }

void lp_traffic_lights(lp_ctx *ctx, float x, float y, int active, int shaded, int zoomed, lp_traffic_result *out) {
    lp_traffic_result res;
    memset(&res, 0, sizeof res);
    lp_size size = lp_traffic_lights_size();
    res.bounds = LP_RECT(x, y, size.w, size.h);
    lp_goo_spec goo = { .size = LP_GOO_XS, .count = 3, .blob_size = RIM, .shape = LP_GOO_CIRCLE, .smear = 2.5f, .gap = GAP, .hot = -1 };
    lp_id base = LP_ID("traffic");
    int hovered_light = -1;
    int clicked[3] = { 0, 0, 0 };
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

    goo.hot = hovered_light;
    lp_goo_group(ctx, res.bounds, &goo);
    int drained = !active && !res.hovered;
    static const enum lp_bubble_tint tints[3] = { LP_TINT_CLOSE, LP_TINT_MINIMIZE, LP_TINT_ZOOM };
    static const float phases[3] = { 0, 2.3f, 4.1f };
    const char *glyphs[3] = { "×", "–", zoomed ? "−" : "+" };
    for (int i = 0; i < 3; i++) {
        lp_rect r = lp_goo_blob_rect(res.bounds, &goo, i);
        float fill = i == 1 && shaded ? 0.38f : -1;
        lp_liquid_bubble(ctx, r.x + r.w / 2, r.y + r.h / 2, BUBBLE, drained ? LP_TINT_INACTIVE : tints[i], phases[i], fill,
            glyphs[i], res.hovered ? 1.0f : 0.0f);
    }
}
