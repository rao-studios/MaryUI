#include <math.h>
#include <stdlib.h>

#include "maryui/lp_blur.h"
#include "maryui/lp_draw.h"
#include "maryui/lp_goo.h"
#include "maryui/lp_tokens.h"

/* xxs is for shapes that are already touching and only need their join fused —
 * a segmented control's thumb and the segment beside it. Blur there is pure
 * cost: it rounds off the pill's own caps until they taper. */
static const float SIZE_SCALE[] = { 0.55f, 1.2f, 1.9f, 3.75f };
#define SIZE_COUNT ((int)(sizeof SIZE_SCALE / sizeof SIZE_SCALE[0]))

float lp_goo_blur(int size_index, enum lp_goo_tension tension) {
    if (size_index < 0) size_index = 0;
    if (size_index >= SIZE_COUNT) size_index = SIZE_COUNT - 1;
    return (tension == LP_GOO_REST ? LP_GOO_BLUR_REST : LP_GOO_BLUR_FLOW) * SIZE_SCALE[size_index];
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void lp_goo_filter(cairo_surface_t *surface, float blur, enum lp_goo_tension tension) {
    if (cairo_image_surface_get_format(surface) != CAIRO_FORMAT_ARGB32) return;
    int w = cairo_image_surface_get_width(surface), h = cairo_image_surface_get_height(surface);
    if (w <= 0 || h <= 0) return;
    float slope = tension == LP_GOO_REST ? LP_GOO_SLOPE_REST : LP_GOO_SLOPE_FLOW;
    float intercept = tension == LP_GOO_REST ? LP_GOO_INTERCEPT_REST : LP_GOO_INTERCEPT_FLOW;
    float rim = fmaxf(1.0f, blur * 0.5f);

    /* 1. feGaussianBlur(SourceGraphic): premultiplied, which is what the spec blurs. */
    lp_blur_surface(surface, blur);

    cairo_surface_flush(surface);
    unsigned char *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    size_t n = (size_t)w * h;
    /* The silhouette, kept premultiplied; feComposite's algebra assumes it. */
    float *sr = malloc(n * sizeof *sr), *sg = malloc(n * sizeof *sg), *sb = malloc(n * sizeof *sb);
    float *sa = malloc(n * sizeof *sa), *dome = malloc(n * sizeof *dome), *scratch = malloc(n * sizeof *scratch);
    if (!sr || !sg || !sb || !sa || !dome || !scratch) {
        free(sr); free(sg); free(sb); free(sa); free(dome); free(scratch);
        return;
    }

    /* 2. feColorMatrix on the alpha row. The matrix runs on unpremultiplied
     * colour, and its RGB rows are the identity, so only alpha moves. */
    for (int y = 0; y < h; y++) {
        const unsigned char *row = data + (size_t)y * stride;
        for (int x = 0; x < w; x++) {
            size_t i = (size_t)y * w + x;
            float b = row[x * 4 + 0] / 255.0f, g = row[x * 4 + 1] / 255.0f;
            float r = row[x * 4 + 2] / 255.0f, a = row[x * 4 + 3] / 255.0f;
            float a2 = clampf(slope * a + intercept, 0.0f, 1.0f);
            float k = a > 0 ? a2 / a : 0.0f;  /* rescale the premultiplied colour to the new alpha */
            sr[i] = r * k; sg[i] = g * k; sb[i] = b * k; sa[i] = a2;
            dome[i] = a2;
        }
    }

    /* 3. A specular dome raised off the silhouette's own alpha. */
    lp_blur_plane(dome, w, h, rim, scratch);
    lp_distant_light light = lp_distant_light_make(LP_GOO_SPECULAR_AZIMUTH, LP_GOO_SPECULAR_ELEVATION);
    lp_color lc = LP_SHEEN_COLOR;
    int shift = (int)lroundf(rim);

    for (int y = 0; y < h; y++) {
        unsigned char *row = data + (size_t)y * stride;
        for (int x = 0; x < w; x++) {
            size_t i = (size_t)y * w + x;
            double spec = lp_specular_at(dome, w, h, x, y, LP_GOO_SPECULAR_SCALE, LP_GOO_SPECULAR_CONSTANT,
                                         LP_GOO_SPECULAR_EXPONENT, light);
            /* feComposite(specular in shape) then arithmetic k2 = k3 = 1: the
             * highlight only where the silhouette is, added on top of it. */
            float hi = (float)spec * sa[i];
            float lr = clampf(hi * lc.r + sr[i], 0, 1), lg = clampf(hi * lc.g + sg[i], 0, 1);
            float lb = clampf(hi * lc.b + sb[i], 0, 1), la = clampf(hi + sa[i], 0, 1);

            /* A shaded band inside the lower rim: the silhouette minus itself,
             * shifted up (feOffset dy = -rim, then feComposite "out"). */
            int ly = y + shift;
            float lifted = ly < h ? sa[(size_t)ly * w + x] : 0.0f;
            float band = sa[i] * (1.0f - lifted);
            float shade_a = band * LP_GOO_RIM_SHADE;

            /* feMerge: the shade sits over the lit silhouette. */
            float inv = 1.0f - shade_a;
            float outr = lr * inv, outg = lg * inv, outb = lb * inv;  /* the flood is black */
            float outa = shade_a + la * inv;
            row[x * 4 + 0] = (unsigned char)(clampf(outb, 0, 1) * 255 + 0.5f);
            row[x * 4 + 1] = (unsigned char)(clampf(outg, 0, 1) * 255 + 0.5f);
            row[x * 4 + 2] = (unsigned char)(clampf(outr, 0, 1) * 255 + 0.5f);
            row[x * 4 + 3] = (unsigned char)(clampf(outa, 0, 1) * 255 + 0.5f);
        }
    }
    cairo_surface_mark_dirty(surface);
    free(sr); free(sg); free(sb); free(sa); free(dome); free(scratch);
}
