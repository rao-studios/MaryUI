/* The molten-platinum wallpaper: src/lib/wallpaperSvg.ts as C. A diagonal
 * platinum gradient displaced into folds by low-frequency fractal noise,
 * softened, then kissed by specular lighting from a second noise, composited
 * arithmetically — the same filter chain, rendered once per screen size and
 * cached as PNG. */
#ifndef MARYUI_LP_WALLPAPER_H
#define MARYUI_LP_WALLPAPER_H

#include <cairo.h>

#define LP_WALLPAPER_W 1600
#define LP_WALLPAPER_H 1000

/* Renders the 1600×1000 SVG "cover"-fitted to w×h (Wallpaper.tsx). Slow (seconds). */
cairo_surface_t *lp_wallpaper_render(int w, int h);

/* $MARYUI_DATA_DIR/wallpaper-WxH.png, else $XDG_CACHE_HOME/maryui/, else render and cache. */
cairo_surface_t *lp_wallpaper_cached(int w, int h);

/* The vignette Wallpaper.module.css paints over the metal, into an existing surface. */
void lp_wallpaper_vignette(cairo_t *cr, int w, int h);

#endif
