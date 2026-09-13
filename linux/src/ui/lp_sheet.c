/* The sheet (lp_sheet.h). */
#include <math.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_surface.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_icon.h"
#include "maryui/lp_sheet.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define PAD LP_SPACE_5
#define ICON 40
#define TITLE_H 18
#define MIN_BUTTON_W 76

typedef struct sheet_layout {
    lp_rect sheet, icon, title, message;
    lp_rect buttons[LP_SHEET_MAX_BUTTONS];
    lp_text_layout *text;   /* the wrapped message; the caller frees it */
} sheet_layout;

static lp_text_style message_style(void) {
    lp_text_style st = lp_text_style_default();
    st.size_px = LP_TEXT_SM;
    st.color = LP_INK_SECONDARY;
    return st;
}

static lp_button_opts button_opts(int i) {
    return (lp_button_opts){ i == 0 ? LP_BUTTON_PRIMARY : LP_BUTTON_DEFAULT, LP_CONTROL_MD, LP_ICON_COUNT, 0, 0 };
}

static void layout(lp_ctx *ctx, lp_rect body, const lp_sheet_spec *spec, sheet_layout *out) {
    memset(out, 0, sizeof *out);
    float w = fminf(440, body.w - 2 * LP_SPACE_6);
    if (w < 240) w = fminf(240, body.w - 2 * LP_SPACE_2);
    float x = body.x + roundf((body.w - w) / 2);
    int has_icon = spec->icon < LP_ICON_COUNT;
    float tx = x + PAD + (has_icon ? ICON + LP_SPACE_4 : 0), tw = x + w - PAD - tx;
    float y = body.y + PAD;
    out->icon = LP_RECT(x + PAD, y, ICON, ICON);
    out->title = LP_RECT(tx, y, tw, TITLE_H);
    float text_h = TITLE_H;
    if (spec->message && spec->message[0]) {
        lp_text_style ms = message_style();
        out->text = lp_text_layout_new(ctx->cr, spec->message, (int)strlen(spec->message), &ms, tw);
        float h = out->text ? lp_text_layout_size(out->text).h : 0;
        out->message = LP_RECT(tx, y + TITLE_H + LP_SPACE_2, tw, h);
        text_h += LP_SPACE_2 + h;
    }
    float by = y + fmaxf(has_icon ? ICON : 0, text_h) + LP_SPACE_5, bh = LP_SIZE_CONTROL_HEIGHT;
    float right = x + w - PAD;
    for (int i = 0; i < spec->count && i < LP_SHEET_MAX_BUTTONS; i++) {
        lp_size bs = lp_button_measure(ctx, spec->buttons[i], button_opts(i));
        float bw = fmaxf(bs.w, MIN_BUTTON_W);
        bh = bs.h;
        if (i < 2) {
            out->buttons[i] = LP_RECT(right - bw, by, bw, bs.h);
            right -= bw + LP_SPACE_2;
        } else {
            out->buttons[i] = LP_RECT(tx, by, bw, bs.h);
        }
    }
    out->sheet = LP_RECT(x, body.y, w, by + bh + PAD - body.y);
}

lp_rect lp_sheet_button_rect(lp_ctx *ctx, lp_rect body, const lp_sheet_spec *spec, int button) {
    sheet_layout l;
    layout(ctx, body, spec, &l);
    if (l.text) lp_text_layout_free(l.text);
    return button >= 0 && button < LP_SHEET_MAX_BUTTONS ? l.buttons[button] : l.sheet;
}

int lp_sheet_input(lp_ctx *ctx, lp_id id, lp_rect body, int *open, const lp_sheet_spec *spec) {
    if (!open || !*open || ctx->pass != LP_PASS_EVENT) return -1;
    sheet_layout l;
    layout(ctx, body, spec, &l);
    if (l.text) lp_text_layout_free(l.text);
    int chosen = -1;
    for (int i = 0; i < spec->count && i < LP_SHEET_MAX_BUTTONS; i++) {
        if (lp_button(ctx, lp_id_index(id, i), l.buttons[i], spec->buttons[i], button_opts(i))) chosen = i;
    }
    if (ctx->in.key_pressed) {
        uint32_t sym = ctx->in.keysym;
        if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) chosen = 0;
        else if (spec->count > 1 && (sym == XKB_KEY_Escape || ((ctx->in.mods & LP_MOD_LOGO) && sym == XKB_KEY_period))) chosen = 1;
    }
    /* The window beneath holds still until the question is answered. */
    ctx->in.pressed = ctx->in.released = 0;
    ctx->in.double_click = 0;
    ctx->in.key_pressed = 0;
    ctx->in.keysym = 0;
    ctx->in.utf8[0] = 0;
    ctx->in.scroll_x = ctx->in.scroll_y = 0;
    ctx->in.mx = ctx->in.my = NAN;
    if (chosen >= 0) {
        *open = 0;
        ctx->dirty = 1;
    }
    return chosen;
}

void lp_sheet_draw(lp_ctx *ctx, lp_id id, lp_rect body, int open, const lp_sheet_spec *spec) {
    if (!open || ctx->pass != LP_PASS_DRAW || !ctx->cr) return;
    cairo_t *cr = ctx->cr;
    sheet_layout l;
    layout(ctx, body, spec, &l);
    float r = LP_RADIUS_LG;
    cairo_save(cr);
    cairo_rectangle(cr, body.x, body.y, body.w, body.h);
    cairo_clip(cr);
    lp_fill_solid(cr, body, LP_RGBA(0, 0, 0, 0.08f), 0);
    /* The panel reaches above the body so its top corners tuck under the title bar: it hangs from it. */
    lp_rect panel = LP_RECT(l.sheet.x, l.sheet.y - r, l.sheet.w, l.sheet.h + r);
    for (int k = 4; k >= 1; k--) lp_fill_solid(cr, LP_RECT(panel.x - k, panel.y, panel.w + 2 * k, panel.h + 2 * k), LP_RGBA(0, 0, 0, 0.035f), r + k);
    lp_surface_paint(cr, panel, (lp_surface_opts){ .variant = LP_VARIANT_FLAT, .radius = r }, lp_surface_motion_of(ctx));
    lp_draw_focus_ring(cr, panel, r, LP_EDGE_HAIRLINE, 1);
    cairo_restore(cr);
    if (spec->icon < LP_ICON_COUNT) lp_icon_draw(cr, spec->icon, l.icon.x, l.icon.y, ICON, 1.6f, LP_INK_SECONDARY);
    lp_text_style ts = lp_text_style_default();
    ts.size_px = LP_TEXT_MD;
    ts.weight = LP_TEXT_WEIGHT_BOLD;
    ts.ellipsize = 1;
    lp_text_draw(cr, spec->title ? spec->title : "", l.title, &ts, LP_ALIGN_START);
    if (l.text) {
        lp_text_layout_draw(cr, l.text, l.message.x, l.message.y, LP_INK_SECONDARY);
        lp_text_layout_free(l.text);
    }
    for (int i = 0; i < spec->count && i < LP_SHEET_MAX_BUTTONS; i++) {
        lp_button(ctx, lp_id_index(id, i), l.buttons[i], spec->buttons[i], button_opts(i));
    }
}
