/* A sheet: the Aqua dialog that drops from under a window's title bar and holds
 * that window until it is answered — for a question that must be answered
 * before anything else happens (Quit Process…, Eject…). The caller owns
 * `open`, calls lp_sheet_input first thing in its paint (the EVENT pass: the
 * buttons and keys, with the window's own input swallowed while the sheet is
 * up) and lp_sheet_draw last (the DRAW pass). Return is the default button, Esc
 * or ⌘. the cancel one. Linux only (PARITY D15): the web has no dialogs, so
 * this lives in src/ui rather than among the mirrored components. */
#ifndef MARYUI_LP_SHEET_H
#define MARYUI_LP_SHEET_H

#include "maryui/lp_icons.h"
#include "maryui/lp_ui.h"

#define LP_SHEET_MAX_BUTTONS 3

typedef struct lp_sheet_spec {
    const char *title;       /* the question, in bold */
    const char *message;     /* what each answer does; wraps */
    lp_icon icon;            /* LP_ICON_WARNING and the like; LP_ICON_COUNT for none */
    /* buttons[0] is the default, rightmost and accent-filled; buttons[1] the cancel,
     * to its left; buttons[2], when there is one, stands apart at the left edge. */
    const char *buttons[LP_SHEET_MAX_BUTTONS];
    int count;
} lp_sheet_spec;

/* The EVENT pass: the button chosen (0 … count-1), which also closes the sheet, or -1. */
int lp_sheet_input(lp_ctx *ctx, lp_id id, lp_rect body, int *open, const lp_sheet_spec *spec);
/* The DRAW pass: shades the body and draws the sheet hanging from its top edge. */
void lp_sheet_draw(lp_ctx *ctx, lp_id id, lp_rect body, int open, const lp_sheet_spec *spec);
/* Where the sheet and its buttons fall in body (tests click them). Returns the sheet. */
lp_rect lp_sheet_button_rect(lp_ctx *ctx, lp_rect body, const lp_sheet_spec *spec, int button);

#endif
