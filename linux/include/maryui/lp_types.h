/* Value types shared by every MaryUI header. Colors are sRGB, unpremultiplied,
 * 0..1; lengths are CSS pixels (the web app's units) as floats. */
#ifndef MARYUI_LP_TYPES_H
#define MARYUI_LP_TYPES_H

#include <stdint.h>

typedef struct lp_color { float r, g, b, a; } lp_color;
typedef struct lp_rect { float x, y, w, h; } lp_rect;
typedef struct lp_size { float w, h; } lp_size;
typedef struct lp_point { float x, y; } lp_point;

/* One layer of a CSS box-shadow: inset flag, offsets, blur radius, spread. */
typedef struct lp_shadow_layer { int inset; float x, y, blur, spread; lp_color color; } lp_shadow_layer;
/* A spring as tokens.json spells it: frequency in Hz, damping ratio. */
typedef struct lp_spring_params { float frequency, damping; } lp_spring_params;
typedef struct lp_cubic_bezier { float x1, y1, x2, y2; } lp_cubic_bezier;

/* Widget identity for the immediate-mode UI (see lp_ui.h). */
typedef uint64_t lp_id;

#define LP_RGBA(r, g, b, a) ((lp_color){ (r), (g), (b), (a) })
#define LP_RECT(x, y, w, h) ((lp_rect){ (x), (y), (w), (h) })

#endif
