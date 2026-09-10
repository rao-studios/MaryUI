/* The liquid merge filter (web/src/components/SvgDefs/SvgDefs.tsx).
 *
 * Blur-and-threshold on its own blurs the drops' colour and emboss along with
 * their alpha, and the merged result reads as flat putty with an aliased edge.
 * So the drops carry no lighting at all: this thresholds them into a
 * silhouette, then lights that silhouette — a specular dome from the same room
 * light as the sheen, and a shaded lower rim — which is what gives a merged
 * bead its volume. It is the design direction's rule at component scale: light
 * the surface, do not paint it. */
#ifndef MARYUI_LP_GOO_H
#define MARYUI_LP_GOO_H

#include <cairo.h>

/* `rest` blurs little and thresholds steeply, so beads stay distinct; `flow`
 * blurs more and thresholds shallowly, so they bridge readily. A group swaps
 * between them when hovered or when its window moves — a state change per
 * interaction, not per frame. */
enum lp_goo_tension { LP_GOO_REST, LP_GOO_FLOW };

/* Blur scales with the group's size; what matters is blur against the *gap*. */
float lp_goo_blur(int size_index, enum lp_goo_tension tension);

/* Filters a premultiplied ARGB32 surface in place: silhouette, then light.
 * `blur` is the feGaussianBlur stdDeviation for the group's size and tension. */
void lp_goo_filter(cairo_surface_t *surface, float blur, enum lp_goo_tension tension);

#endif
