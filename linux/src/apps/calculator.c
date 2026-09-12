/* Calculator — the four functions, percent, sign, a memory register and
 * repeated equals, clicked or typed. The arithmetic is lp_calc
 * (src/core/lp_calc.c); this file is its display and keypad. ⌘/Ctrl+C copies
 * the display, ⌘/Ctrl+V types the clipboard in. Linux only (PARITY D15). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "maryui/components/lp_button.h"
#include "maryui/components/lp_surface.h"
#include "maryui/components/lp_text_area.h"
#include "maryui/lp_calc.h"
#include "maryui/lp_desktop.h"
#include "maryui/lp_text.h"
#include "maryui/lp_tokens.h"

#define COLS 4
#define ROWS 6
#define DISPLAY_H 72

struct calculator {
    lp_calc calc;
};

static const struct pad_key {
    enum lp_calc_key key;
    const char *label;          /* NULL: AC or C, whichever clear means now */
    enum lp_button_variant variant;
    int col, row, span;
} KEYPAD[] = {
    { LP_CALC_MC, "mc", LP_BUTTON_QUIET, 0, 0, 1 },   { LP_CALC_MR, "mr", LP_BUTTON_QUIET, 1, 0, 1 },
    { LP_CALC_MMINUS, "m−", LP_BUTTON_QUIET, 2, 0, 1 }, { LP_CALC_MPLUS, "m+", LP_BUTTON_QUIET, 3, 0, 1 },
    { LP_CALC_CLEAR, NULL, LP_BUTTON_DEFAULT, 0, 1, 1 }, { LP_CALC_SIGN, "±", LP_BUTTON_DEFAULT, 1, 1, 1 },
    { LP_CALC_PERCENT, "%", LP_BUTTON_DEFAULT, 2, 1, 1 }, { LP_CALC_DIV, "÷", LP_BUTTON_DEFAULT, 3, 1, 1 },
    { LP_CALC_7, "7", LP_BUTTON_DEFAULT, 0, 2, 1 }, { LP_CALC_8, "8", LP_BUTTON_DEFAULT, 1, 2, 1 },
    { LP_CALC_9, "9", LP_BUTTON_DEFAULT, 2, 2, 1 }, { LP_CALC_MUL, "×", LP_BUTTON_DEFAULT, 3, 2, 1 },
    { LP_CALC_4, "4", LP_BUTTON_DEFAULT, 0, 3, 1 }, { LP_CALC_5, "5", LP_BUTTON_DEFAULT, 1, 3, 1 },
    { LP_CALC_6, "6", LP_BUTTON_DEFAULT, 2, 3, 1 }, { LP_CALC_SUB, "−", LP_BUTTON_DEFAULT, 3, 3, 1 },
    { LP_CALC_1, "1", LP_BUTTON_DEFAULT, 0, 4, 1 }, { LP_CALC_2, "2", LP_BUTTON_DEFAULT, 1, 4, 1 },
    { LP_CALC_3, "3", LP_BUTTON_DEFAULT, 2, 4, 1 }, { LP_CALC_ADD, "+", LP_BUTTON_DEFAULT, 3, 4, 1 },
    { LP_CALC_0, "0", LP_BUTTON_DEFAULT, 0, 5, 2 }, { LP_CALC_POINT, ".", LP_BUTTON_DEFAULT, 2, 5, 1 },
    { LP_CALC_EQUALS, "=", LP_BUTTON_PRIMARY, 3, 5, 1 },
};

static void calculator_command(void *state, lp_desktop *d, int cmd) {
    struct calculator *k = state;
    if (!k) return;
    switch ((enum lp_calculator_command)cmd) {
    case LP_CALCULATOR_COPY: {
        char text[64];
        lp_calc_copy_text(&k->calc, text, sizeof text);
        if (text[0]) lp_text_clipboard_set(lp_text_clipboard_shared(), text, (int)strlen(text));
        break;
    }
    case LP_CALCULATOR_PASTE: {
        lp_text_clipboard *clip = lp_text_clipboard_shared();
        if (clip->text) lp_calc_paste(&k->calc, clip->text, clip->len);
        break;
    }
    }
}

static void paint_display(lp_ctx *ctx, lp_rect r, const lp_calc *calc) {
    cairo_t *cr = ctx->cr;
    lp_surface_paint(cr, r, (lp_surface_opts){ .variant = LP_VARIANT_WELL, .radius = LP_RADIUS_MD }, lp_surface_motion_of(ctx));
    char expr[200], shown[80];
    lp_calc_expression(calc, expr, sizeof expr);
    lp_calc_display(calc, shown, sizeof shown);
    lp_rect inner = lp_rect_inset(r, LP_SPACE_3, LP_SPACE_2);
    lp_rect line = lp_rect_cut_top(&inner, 16);
    lp_text_style small = lp_text_style_default();
    small.size_px = LP_TEXT_SM;
    small.color = LP_INK_TERTIARY;
    small.tabular_nums = 1;
    small.ellipsize = 1;
    if (calc->has_memory) {
        lp_text_draw(cr, "M", line, &small, LP_ALIGN_START);
        lp_rect_cut_left(&line, 16);
    }
    lp_text_draw(cr, expr, line, &small, LP_ALIGN_END);
    /* The value shrinks to fit rather than being cut: a calculator never hides a digit. */
    lp_text_style value = lp_text_style_default();
    value.font = LP_FONT_DISPLAY;
    value.size_px = LP_TEXT_XXL;
    value.weight = LP_TEXT_WEIGHT_MEDIUM;
    value.tabular_nums = 1;
    value.emboss = 1;
    while (value.size_px > LP_TEXT_MD && lp_text_measure(cr, shown, &value).w > inner.w) value.size_px -= 1;
    lp_text_draw(cr, shown, inner, &value, LP_ALIGN_END);
}

static void calculator_paint(void *state, lp_ctx *ctx, lp_rect body, lp_desktop *d) {
    static struct calculator preview;
    static int seeded;
    struct calculator *k = state;
    if (!k) {
        if (!seeded) { lp_calc_init(&preview.calc); lp_calc_paste(&preview.calc, "1234.5*2", -1); seeded = 1; }
        k = &preview;
    }

    if (ctx->pass == LP_PASS_EVENT && ctx->in.key_pressed && ctx->in.keysym) {
        uint32_t sym = ctx->in.keysym;
        if (ctx->in.mods & (LP_MOD_CTRL | LP_MOD_LOGO)) {
            if (sym == XKB_KEY_c || sym == XKB_KEY_C) calculator_command(k, d, LP_CALCULATOR_COPY);
            if (sym == XKB_KEY_v || sym == XKB_KEY_V) { calculator_command(k, d, LP_CALCULATOR_PASTE); ctx->dirty = 1; }
        } else {
            enum lp_calc_key key = lp_calc_key_for(sym, ctx->in.utf8);
            if (key != LP_CALC_KEY_COUNT) { lp_calc_press(&k->calc, key); ctx->dirty = 1; }
        }
    }

    lp_rect area = lp_rect_inset(body, LP_SPACE_3, LP_SPACE_3);
    lp_rect display = lp_rect_cut_top(&area, DISPLAY_H);
    lp_rect_cut_top(&area, LP_SPACE_3);
    if (ctx->pass == LP_PASS_DRAW && ctx->cr) paint_display(ctx, display, &k->calc);

    float gap = LP_SPACE_2;
    float cw = (area.w - gap * (COLS - 1)) / COLS, ch = (area.h - gap * (ROWS - 1)) / ROWS;
    lp_id base = LP_ID("calculator");
    for (int i = 0; i < (int)(sizeof KEYPAD / sizeof KEYPAD[0]); i++) {
        const struct pad_key *p = &KEYPAD[i];
        lp_rect r = LP_RECT(area.x + p->col * (cw + gap), area.y + p->row * (ch + gap), cw * p->span + gap * (p->span - 1), ch);
        const char *label = p->label ? p->label : lp_calc_clear_is_all(&k->calc) ? "AC" : "C";
        lp_button_opts opts = { p->variant, LP_CONTROL_MD, LP_ICON_COUNT, 0, p->key == LP_CALC_MR && !k->calc.has_memory };
        if (lp_button(ctx, lp_id_index(base, i), r, label, opts)) {
            lp_calc_press(&k->calc, p->key);
            ctx->dirty = 1;
        }
    }
}

static void calculator_menu_entries(void *state, lp_desktop *d, int menu, lp_menu_model *m) {
    if (menu != LP_MENU_EDIT || m->count + 2 > LP_MENU_MAX_ENTRIES) return;
    lp_menu_entry *e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "Copy");
    e->shortcut = "⌘C"; e->command = LP_CMD_APP; e->arg = LP_CALCULATOR_COPY;
    e = &m->entries[m->count++];
    memset(e, 0, sizeof *e);
    snprintf(e->label, sizeof e->label, "Paste");
    e->shortcut = "⌘V"; e->command = LP_CMD_APP; e->arg = LP_CALCULATOR_PASTE;
}

static void *calculator_create(lp_desktop *d, const char *window_id) {
    struct calculator *k = calloc(1, sizeof *k);
    if (k) lp_calc_init(&k->calc);
    return k;
}

static void calculator_destroy(void *state) { free(state); }

const lp_app lp_app_calculator = {
    .id = "calculator", .title = "Calculator", .name = "Calculator", .icon = LP_ICON_GRID, .object = "appCalculator",
    /* 300 wide: the title bar keeps 84px either side of its title, and "Calculator" needs the rest */
    .default_rect = { NAN, NAN, 300, 420 }, .min_size = { 300, 420 }, .singleton = 1, .resizable = 0,
    .create = calculator_create, .paint = calculator_paint, .destroy = calculator_destroy,
    .command = calculator_command, .menu_entries = calculator_menu_entries,
};

/* MARK: - Introspection (tests) */

const lp_calc *lp_calculator_calc(const void *state) { return &((const struct calculator *)state)->calc; }
void lp_calculator_type(void *state, const char *keys) { if (state) lp_calc_paste(&((struct calculator *)state)->calc, keys, -1); }
