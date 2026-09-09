/* GooGroup — blank metal blobs behind a row of controls that swell on hover
 * and, when the group is filtered, merge like drops of mercury. The
 * threshold filter (blur + alpha contrast) lands with the motion milestone;
 * the blobs, their hover scale and the drag smear are here. */
#ifndef MARYUI_LP_GOO_GROUP_H
#define MARYUI_LP_GOO_GROUP_H

#include "maryui/lp_ui.h"

enum lp_goo_size { LP_GOO_XS, LP_GOO_SM, LP_GOO_MD };
enum lp_goo_shape { LP_GOO_CIRCLE, LP_GOO_PILL, LP_GOO_FILL };

typedef struct lp_goo_spec {
    enum lp_goo_size size;
    int count;
    float blob_size;    /* diameter / height */
    enum lp_goo_shape shape;
    float smear;        /* px of horizontal smear per unit of vx, along the row */
    float gap;
    int hot;            /* index of the hovered child, -1 for none */
} lp_goo_spec;

/* Paints the blob layer for a row laid out in `area` (left-aligned, vertically centred). */
void lp_goo_group(lp_ctx *ctx, lp_rect area, const lp_goo_spec *spec);
/* Where blob i sits (before hover scaling), for hit-testing the children. */
lp_rect lp_goo_blob_rect(lp_rect area, const lp_goo_spec *spec, int i);

#endif
