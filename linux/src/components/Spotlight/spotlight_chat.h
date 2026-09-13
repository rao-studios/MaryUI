/* Spotlight's conversation with Mary (Linux, PARITY D18): the Ask Mary orb inside the
 * bar and the dialogue under it. Private to Spotlight.c, which lays the panel out. */
#ifndef MARYUI_SPOTLIGHT_CHAT_H
#define MARYUI_SPOTLIGHT_CHAT_H

#include "maryui/components/lp_spotlight_panel.h"

/* The orb at the bar's right end (size.spotlight-accessory); sets res->ask_pressed. */
void lp_spotlight_ask_orb(lp_ctx *ctx, lp_rect bar, const lp_spotlight_view *view, lp_spotlight_result *res);
/* What the bar says while the conversation is up: it follows Mary's state. */
const char *lp_spotlight_chat_placeholder(const lp_spotlight_view *view);
/* The status row and the well of dialogue, from y (under the bar's divider) to the panel's bottom padding. */
void lp_spotlight_chat(lp_ctx *ctx, const lp_spotlight_view *view, lp_rect panel, float y, lp_spotlight_result *res);

#endif
