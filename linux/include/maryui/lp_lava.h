/* The lava wallpaper (Linux only, PARITY D33): a live, lighter variant of the molten shader (lp_molten.h) that
 * moves on its own, slowly, like a blurry wavy lava lamp. The same field — onoise folded into an fbm, a domain
 * warp turned by time, a soft tanh height lit by the molten shader's two lights, its shadow term, its grade and
 * vignette — with the creases and the fine octaves left out, and computed on the CPU at a quarter of the
 * output's size: the compositor's scene stretches each frame to the screen with bilinear filtering, which is the
 * blur. Normals come from the height grid itself rather than four more samples, and sine is a table, so a frame
 * costs milliseconds where the molten still costs seconds under a software rasteriser. No GL: the same code
 * runs in the VM, on a Pi and in the tests. */
#ifndef MARYUI_LP_LAVA_H
#define MARYUI_LP_LAVA_H

#include <cairo.h>

#include "maryui/lp_molten.h"

#define LP_LAVA_SCALE 4       /* output pixels per field pixel, each way */
#define LP_LAVA_MARGIN 1      /* field pixels past each edge, so the stretch never samples outside the frame */
#define LP_LAVA_FPS 15        /* frames a second: the field drifts a few output pixels between them */
#define LP_LAVA_SPEED 1.0f    /* field seconds per wall-clock second */

/* The frame's size for a w×h output: ceil(w/scale) + 2·margin by ceil(h/scale) + 2·margin. */
void lp_lava_frame_size(int w, int h, int *fw, int *fh);

/* Renders the field for a w×h output at `time` (field seconds) into a new ARGB32 surface of lp_lava_frame_size,
 * opaque. The interior — everything but the margin — maps onto the output. NULL on allocation failure. */
cairo_surface_t *lp_lava_frame(int w, int h, double time, enum lp_molten_tone tone);

/* The frame stretched to w×h with bilinear filtering, as the scene shows it: a still for the Launchpad's
 * backdrop, lp-render, or a desktop with reduced motion. */
cairo_surface_t *lp_lava_still(int w, int h, double time, enum lp_molten_tone tone);

#endif
