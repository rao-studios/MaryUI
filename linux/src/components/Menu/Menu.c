#include <string.h>

#include "maryui/components/lp_menu.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_settings.h"
#include "maryui/lp_shadow.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

static lp_text_style label_style(void) { return lp_text_style_default(); }
static lp_text_style shortcut_style(void) {
    lp_text_style s = lp_text_style_default();
    s.size_px = LP_TEXT_SM;
    s.color = LP_INK_TERTIARY;
    s.letter_spacing = LP_TEXT_SM * 0.04f;
    return s;
}

static float row_height(const lp_menu_entry *e) { return e->separator ? 1 + 2 * LP_SPACE_1 : LP_SIZE_CONTROL_HEIGHT; }

lp_size lp_menu_measure(cairo_t *cr, const lp_menu_model *m) {
    float h = 2 * LP_SPACE_1, w = LP_MENU_MIN_WIDTH;
    lp_text_style ls = label_style(), ss = shortcut_style();
    for (int i = 0; i < m->count; i++) {
        const lp_menu_entry *e = &m->entries[i];
        h += row_height(e);
        if (e->separator) continue;
        float tw = cr ? lp_text_measure(cr, e->label, &ls).w : strlen(e->label) * 7.0f;
        float sw = e->shortcut ? (cr ? lp_text_measure(cr, e->shortcut, &ss).w : strlen(e->shortcut) * 7.0f) + LP_SPACE_2 : 0;
        float row = 2 * LP_SPACE_1 + LP_SPACE_1 + 18 + LP_SPACE_2 + tw + sw + LP_SPACE_3;
        if (row > w) w = row;
    }
    return (lp_size){ w, h };
}

void lp_menu(lp_ctx *ctx, lp_rect r, const lp_menu_model *m, int active, lp_menu_result *out) {
    lp_menu_result res = { .selected = -1, .hovered = -1 };
    lp_size size = lp_menu_measure(ctx->cr, m);
    res.width = (int)size.w;
    res.height = (int)size.h;
    lp_rect panel = LP_RECT(r.x, r.y, size.w, size.h);
    lp_accent accent = lp_settings_accent(ctx->settings);

    if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
        cairo_t *cr = ctx->cr;
        lp_draw_shadow_9slice(cr, panel, LP_RADIUS_MD, LP_SHADOW_MENU, LP_SHADOW_MENU_COUNT);
        lp_fill_solid(cr, panel, LP_SURFACE_MENU, LP_RADIUS_MD);
        lp_draw_inset_shadows(cr, panel, LP_RADIUS_MD, LP_SHADOW_EMBOSS_RAISED, LP_SHADOW_EMBOSS_RAISED_COUNT);
    }
    float y = panel.y + LP_SPACE_1;
    for (int i = 0; i < m->count; i++) {
        const lp_menu_entry *e = &m->entries[i];
        float h = row_height(e);
        lp_rect row = LP_RECT(panel.x + LP_SPACE_1, y, panel.w - 2 * LP_SPACE_1, h);
        y += h;
        if (e->separator) {
            if (ctx->pass == LP_PASS_DRAW && ctx->cr) {
                lp_fill_solid(ctx->cr, LP_RECT(panel.x + LP_SPACE_2, row.y + LP_SPACE_1, panel.w - 2 * LP_SPACE_2, 1), LP_EDGE_DIVIDER, 0);
            }
            continue;
        }
        if (lp_hit(ctx, row)) {
            res.hovered = i;
            if (ctx->pass == LP_PASS_EVENT && !e->disabled && (ctx->in.released & LP_BUTTON_LEFT)) res.selected = i;
        }
        if (ctx->pass != LP_PASS_DRAW || !ctx->cr) continue;
        cairo_t *cr = ctx->cr;
        int highlighted = active == i && !e->disabled;
        if (highlighted) lp_fill_vgradient(cr, row, accent.light, accent.base, LP_RADIUS_XS);
        lp_text_style ls = label_style(), ss = shortcut_style(), cs = label_style();
        cs.size_px = LP_TEXT_SM;
        cs.weight = LP_TEXT_WEIGHT_BOLD;
        if (highlighted) {
            ls.color = LP_INK_ON_ACCENT; ls.emboss = 1; ls.emboss_color = LP_RGBA(0, 0, 0, 0.2f);
            ss.color = LP_RGBA(1, 1, 1, 0.85f); cs.color = LP_INK_ON_ACCENT;
        } else if (e->disabled) {
            ls.color = LP_INK_DISABLED; cs.color = LP_INK_DISABLED;
        }
        lp_rect check = LP_RECT(row.x + LP_SPACE_1, row.y, 18, row.h);
        if (e->checked) lp_text_draw(cr, "✓", check, &cs, LP_ALIGN_CENTER);
        float sw = 0;
        if (e->shortcut) {
            sw = lp_text_measure(cr, e->shortcut, &ss).w;
            lp_text_draw(cr, e->shortcut, LP_RECT(row.x + row.w - LP_SPACE_3 - sw, row.y, sw + 2, row.h), &ss, LP_ALIGN_START);
        }
        ls.ellipsize = 1;
        float lx = check.x + check.w + LP_SPACE_2;
        lp_text_draw(cr, e->label, LP_RECT(lx, row.y, row.x + row.w - LP_SPACE_3 - sw - (sw ? LP_SPACE_2 : 0) - lx, row.h), &ls, LP_ALIGN_START);
    }
    if (out) *out = res;
}
