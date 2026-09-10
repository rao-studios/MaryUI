/* GooGroup — blobs behind a row of controls that swell on hover and merge like
 * drops of mercury. The blobs themselves carry no lighting; lp_goo_filter
 * thresholds them into one silhouette and lights that, which is what stops a
 * merged pair reading as putty. */
#ifndef MARYUI_LP_GOO_GROUP_H
#define MARYUI_LP_GOO_GROUP_H

#include "maryui/lp_goo.h"
#include "maryui/lp_ui.h"

enum lp_goo_size { LP_GOO_XXS, LP_GOO_XS, LP_GOO_SM, LP_GOO_MD };
enum lp_goo_shape { LP_GOO_CIRCLE, LP_GOO_PILL, LP_GOO_FILL };

typedef struct lp_goo_spec lp_goo_spec;
struct lp_goo_spec {
    enum lp_goo_size size;
    int count;
    float blob_size;    /* diameter / height */
    enum lp_goo_shape shape;
    float smear;        /* px of horizontal smear per unit of shear, along the row */
    float gap;
    int hot;            /* index of the hovered child, -1 for none */
    int flowing;        /* the group is in the flow tension: it merges readily */
    int still;          /* the group ignores the window's motion (the traffic lights do) */
    /* Paints blob i into `r`. NULL paints the default raised platinum blob. */
    void (*render_blob)(lp_ctx *ctx, lp_rect r, int i, void *user);
    void *user;
};

/* Paints the blob layer for a row laid out in `area` (left-aligned, vertically centred). */
void lp_goo_group(lp_ctx *ctx, lp_rect area, const lp_goo_spec *spec);
/* Where blob i sits (before hover scaling), for hit-testing the children. */
lp_rect lp_goo_blob_rect(lp_rect area, const lp_goo_spec *spec, int i);

#endif
