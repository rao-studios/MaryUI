/* TrafficLights — close / shade / zoom, each a LiquidBubble set in a platinum
 * rim. The rims are GooGroup blobs. Inactive windows drain the colour out
 * until you hover them, as Aqua did. */
#ifndef MARYUI_LP_TRAFFIC_LIGHTS_H
#define MARYUI_LP_TRAFFIC_LIGHTS_H

#include "maryui/lp_ui.h"

typedef struct lp_traffic_result {
    int close, shade, zoom;   /* clicked this event */
    int hovered;              /* the group is under the pointer */
    lp_rect bounds;           /* the group's rect (for the title bar's drag exclusion) */
} lp_traffic_result;

/* The group's size: three 14px rims at a 20px pitch. */
lp_size lp_traffic_lights_size(void);
/* Draws at (x, y) (top-left of the group). */
void lp_traffic_lights(lp_ctx *ctx, float x, float y, int active, int shaded, int zoomed, lp_traffic_result *out);

#endif
