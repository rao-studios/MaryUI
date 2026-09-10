/* The molten-platinum wallpaper (web/src/lib/moltenShader.ts + moltenRenderer.ts):
 * a domain-warped fbm height field lit by two lights, after Mårten Rånge's
 * shader. `docs/design-direction.md` calls it the reference the rest of the
 * system is measured against — detail from folding rather than stacking, and
 * colour that comes from lighting the field rather than painting it.
 *
 * The web runs it in WebGL. Here it runs in an offscreen GLES2 context on a
 * surfaceless EGL display and is read back into a Cairo surface, so it needs no
 * GL from the compositor's scene renderer: the same code path works under
 * pixman in a VM (mesa's software rasteriser) and on a Pi's v3d.
 *
 * The whole thing compiles to stubs when EGL and GLES2 are absent (macOS, a
 * minimal image), and lp_wallpaper falls back to the procedural filter chain. */
#ifndef MARYUI_LP_MOLTEN_H
#define MARYUI_LP_MOLTEN_H

#include <cairo.h>

#include "maryui/lp_types.h"

/* The two grades: the platinum ramp's cool cast, and the shader's dark original. */
enum lp_molten_tone { LP_MOLTEN_PLATINUM, LP_MOLTEN_FAITHFUL };

typedef struct lp_molten_grade {
    lp_color base;  /* base reflectance */
    float lift, gain, saturation;
} lp_molten_grade;

lp_molten_grade lp_molten_grade_for(enum lp_molten_tone tone);

/* Whether a GL context could be created at all. Cached after the first call. */
int lp_molten_available(void);
/* Non-zero when the GL renderer is a software rasteriser (llvmpipe, swrast),
 * i.e. rendering costs far too much to animate. Only meaningful once available. */
int lp_molten_is_software(void);

/* Renders w×h at `time` (shader units; the web advances it by molten.flow per
 * second of window motion, never by wall-clock). Returns a new ARGB32 surface,
 * or NULL when GL is unavailable. The caller owns it. */
cairo_surface_t *lp_molten_render(int w, int h, float time, float zoom, enum lp_molten_tone tone);

/* Releases the shared context. Safe to call when nothing was created. */
void lp_molten_shutdown(void);

#endif
