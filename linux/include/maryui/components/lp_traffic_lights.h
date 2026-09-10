/* TrafficLights — close / shade / zoom, each a LiquidBubble. The bead is the
 * Toggle's knob: the same glass well, the same drop shadow sitting it proud of
 * the title bar. There is no platinum rim any more.
 *
 * Behind the beads sits a second, hidden copy of the same three liquids at full
 * fill, rendered as bare liquid and put through the merge filter. At rest the
 * beads cover it exactly; on hover the filter loosens and that hidden mass
 * bridges, so the three lights flow into one another without the beads
 * themselves moving or fading. Inactive windows drain the colour out until you
 * hover them, as Aqua did. */
#ifndef MARYUI_LP_TRAFFIC_LIGHTS_H
#define MARYUI_LP_TRAFFIC_LIGHTS_H

#include "maryui/lp_ui.h"

typedef struct lp_traffic_result {
    int close, shade, zoom;   /* clicked this event */
    int hovered;              /* the group is under the pointer */
    lp_rect bounds;           /* the group's rect (for the title bar's drag exclusion) */
} lp_traffic_result;

/* The group's size: three 18px beads at a 28px pitch. */
lp_size lp_traffic_lights_size(void);
/* Draws at (x, y) (top-left of the group). */
void lp_traffic_lights(lp_ctx *ctx, float x, float y, int active, int shaded, int zoomed, lp_traffic_result *out);

#endif
