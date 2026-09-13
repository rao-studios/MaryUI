/* A pop-up button: a raised capsule showing the current choice, start-aligned, with a chevron at its end, as
 * Aqua's pop-up menus looked. A click returns 1 (EVENT pass), and the caller opens the choices under it with
 * lp_desktop_open_popup. C only: the web's <select> is the browser's, with no component to mirror, so this lives
 * in src/ui rather than src/components (PARITY D23). */
#ifndef MARYUI_LP_POPUP_H
#define MARYUI_LP_POPUP_H

#include "maryui/lp_ui.h"

/* The width that shows `label` whole with its chevron, at the small control height. */
lp_size lp_popup_button_measure(lp_ctx *ctx, const char *label);
int lp_popup_button(lp_ctx *ctx, lp_id id, lp_rect r, const char *label, int disabled);

#endif
