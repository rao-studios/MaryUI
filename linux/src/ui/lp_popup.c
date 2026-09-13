/* The pop-up button (lp_popup.h). */
#include <string.h>

#include "maryui/components/lp_button.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_popup.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define CHEVRON 10

lp_size lp_popup_button_measure(lp_ctx *ctx, const char *label) {
    lp_button_opts opts = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, 0 };
    lp_size size = lp_button_measure(ctx, label, opts);
    size.w += CHEVRON + LP_SPACE_2;
    return size;
}

int lp_popup_button(lp_ctx *ctx, lp_id id, lp_rect r, const char *label, int disabled) {
    lp_button_opts opts = { LP_BUTTON_DEFAULT, LP_CONTROL_SM, LP_ICON_COUNT, 0, disabled };
    int clicked = lp_button(ctx, id, r, "", opts);  /* the capsule, its press and its focus */
    if (ctx->pass != LP_PASS_DRAW || !ctx->cr) return clicked;
    float pad = LP_SPACE_3, y = r.y + (!disabled && lp_is_active(ctx, id) && lp_is_hot(ctx, id) ? 0.5f : 0);
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_SM;
    st.weight = LP_TEXT_WEIGHT_MEDIUM;
    st.emboss = 1;
    st.ellipsize = 1;
    st.color = disabled ? LP_INK_DISABLED : LP_INK_PRIMARY;
    lp_text_draw(ctx->cr, label ? label : "", LP_RECT(r.x + pad, y, r.w - 2 * pad - CHEVRON - LP_SPACE_1, r.h), &st, LP_ALIGN_START);
    lp_icon_draw(ctx->cr, LP_ICON_CHEVRON_DOWN, r.x + r.w - pad - CHEVRON, y + (r.h - CHEVRON) / 2, CHEVRON, 0,
                 disabled ? LP_INK_DISABLED : LP_INK_SECONDARY);
    return clicked;
}
