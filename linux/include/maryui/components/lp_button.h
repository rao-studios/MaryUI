/* Button — a raised platinum capsule with an embossed label. `primary` fills
 * with the accent, `quiet` shows its metal only on hover. Pressing sinks the
 * gradient and the emboss, like Aqua's buttons did. */
#ifndef MARYUI_LP_BUTTON_H
#define MARYUI_LP_BUTTON_H

#include "maryui/lp_icons.h"
#include "maryui/lp_ui.h"

enum lp_button_variant { LP_BUTTON_DEFAULT, LP_BUTTON_PRIMARY, LP_BUTTON_QUIET };
enum lp_control_size { LP_CONTROL_MD, LP_CONTROL_SM };

typedef struct lp_button_opts {
    enum lp_button_variant variant;
    enum lp_control_size size;
    lp_icon icon;        /* LP_ICON_COUNT for none */
    int icon_only;
    int disabled;
} lp_button_opts;

lp_size lp_button_measure(lp_ctx *ctx, const char *label, lp_button_opts opts);
/* Returns 1 when clicked (EVENT pass only). */
int lp_button(lp_ctx *ctx, lp_id id, lp_rect r, const char *label, lp_button_opts opts);

#endif
