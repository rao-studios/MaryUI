/* The brushed-platinum tile: src/lib/brushSvg.ts rendered with the C
 * feTurbulence. Anisotropic fractal noise, desaturated, compressed around
 * mid-gray, laid on a lattice rotated by atan(rise/run) with a period chosen
 * so the square tile repeats seamlessly. Surfaces paint it with the OVERLAY
 * operator at brush.opacity. */
#ifndef MARYUI_LP_TEXTURE_H
#define MARYUI_LP_TEXTURE_H

#include <cairo.h>

typedef struct lp_brush_params {
    double freq_x, freq_y;   /* cycles per px along / across the stroke */
    int octaves;
    int seed;
    int tile;                /* raster tile edge in px */
    int rise, run;           /* tan θ = rise / run */
    double contrast;         /* slope of the mid-gray compression, 0..1 */
    double opacity;          /* how strongly surfaces paint it */
} lp_brush_params;

lp_brush_params lp_brush_params_from_tokens(void);
double lp_brush_angle_degrees(const lp_brush_params *p);

/* A new tile×tile RGB24 surface. The caller owns it. */
cairo_surface_t *lp_brush_tile_render(const lp_brush_params *p);

/* The shared tile for the current parameters (rendered once; lp_brush_live
 * may be edited by the Gallery's Motion tab, then lp_brush_tile_reset()). */
extern lp_brush_params lp_brush_live;
cairo_surface_t *lp_brush_tile(void);
void lp_brush_tile_reset(void);

#endif
