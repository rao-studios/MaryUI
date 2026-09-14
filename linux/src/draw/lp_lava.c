/* The lava wallpaper (lp_lava.h): the molten shader's field, lighter and live. Where the GLSL in lp_molten.c
 * evaluates a 27-octave warp five times a pixel at full resolution, this evaluates a 10-octave one once per
 * field pixel at a quarter resolution, takes the normal from the grid and the shadow from a bilinear sample of
 * it, and folds the grade's per-channel curve into a table. Coordinates follow the shader: q is 0..1 across the
 * output with y up, p is -1..1 on the short side. */
#include "maryui/lp_lava.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "maryui/lp_tokens.h"

/* The field. FREQ scales p into the noise (the molten shader uses 2 with seven octaves of detail; here a few large
 * blobs). FOLD is how far onoise's crossfade turns — the molten shader's full turn, twice a cell, is what creases
 * the metal; a third of one blends. SOFT and RELIEF shape the height the lights see: gentle slopes, so the light
 * never flips to black between two field pixels. BAND and SHADOW make the shadow term a soft rim round the wax
 * rather than a contour. RISE is how fast the wax climbs, DRIFT how the warp slides against it, TURN how fast it
 * turns, WARP how far it bends the domain — all slow: a frame moves the light by a level or so. Each can be
 * overridden with -D for a tuning build. */
#ifndef LAVA_FREQ
#define LAVA_FREQ 1.8f
#endif
#ifndef LAVA_FOLD
#define LAVA_FOLD 0.28f
#endif
#ifndef LAVA_SOFT
#define LAVA_SOFT 2.0f
#endif
#ifndef LAVA_RELIEF
#define LAVA_RELIEF 0.40f
#endif
#ifndef LAVA_BAND
#define LAVA_BAND 0.13f
#endif
#ifndef LAVA_SHADOW
#define LAVA_SHADOW 0.45f
#endif
#ifndef LAVA_RISE
#define LAVA_RISE 0.09f
#endif
#ifndef LAVA_DRIFT_A
#define LAVA_DRIFT_A 0.06f
#endif
#ifndef LAVA_DRIFT_B
#define LAVA_DRIFT_B 0.045f
#endif
#ifndef LAVA_TURN
#define LAVA_TURN 0.05f
#endif
#ifndef LAVA_WARP
#define LAVA_WARP 1.1f
#endif
#ifndef LAVA_WARP_OCTAVES
#define LAVA_WARP_OCTAVES 2
#endif
#ifndef LAVA_OCTAVES
#define LAVA_OCTAVES 3
#endif

#define TAU 6.283185307f

/* MARK: - Sine by table: the field spends most of its time in it */

#define SIN_BITS 12
#define SIN_N (1 << SIN_BITS)
static float sin_table[SIN_N + 1];
static int sin_ready;

static void sin_init(void) {
    if (sin_ready) return;
    for (int i = 0; i <= SIN_N; i++) sin_table[i] = (float)sin(2.0 * M_PI * i / SIN_N);
    sin_ready = 1;
}

static inline float fsin(float x) {
    float u = x * (SIN_N / TAU);
    float f = floorf(u);
    int i = (int)((long)f & (SIN_N - 1));
    float t = u - f;
    return sin_table[i] + (sin_table[i + 1] - sin_table[i]) * t;
}

static inline float tanh_approx(float x) {
    float x2 = x * x, v = x * (27.0f + x2) / (27.0f + 9.0f * x2);
    return v < -1 ? -1 : v > 1 ? 1 : v;
}

/* The molten shader's onoise: two sines crossfaded by a third function of their own product — turned only FOLD
 * of the way, so the crossfade blends instead of folding. */
static inline float onoise(float x, float y) {
    float a = fsin(x * 0.5f), b = fsin(y * 0.5f);
    float m = 0.5f + 0.5f * fsin(TAU * LAVA_FOLD * tanh_approx(a * b + a + b));
    return a + (b - a) * m;
}

/* Its fbm, same constants, without the value-noise sign flips that crease the metal. */
static float fbm(float x, float y, int octaves) {
    static const float AA = 0.45f, PP = 2.03f, OX = -1.23f, OY = -1.5f;
    static float rc, rs;
    if (rc == 0 && rs == 0) { rc = (float)cos(1.2); rs = (float)sin(1.2); }
    float h = 0, d = 0, a = 1;
    for (int i = 0; i < octaves; i++) {
        h += a * onoise(x, y);
        d += a;
        a *= AA;
        x = (x + OX) * PP;
        y = (y + OY) * PP;
        float nx = rc * x + rs * y, ny = -rs * x + rc * y;
        x = nx;
        y = ny;
    }
    return h / d;
}

static float height(float px, float py, float t) {
    float x = px * LAVA_FREQ + 13.0f, y = py * LAVA_FREQ + 13.0f - t * LAVA_RISE;
    float vx = fbm(x, y + t * LAVA_DRIFT_A, LAVA_WARP_OCTAVES);
    float vy = fbm(x + 0.7f - t * LAVA_DRIFT_B, y + 0.7f, LAVA_WARP_OCTAVES);
    float a = 1.0f + t * LAVA_TURN, c = (float)cos(a), s = (float)sin(a);
    float wx = c * vx + s * vy, wy = -s * vx + c * vy;
    float h = fbm(x + LAVA_WARP * wx, y + LAVA_WARP * wy, LAVA_OCTAVES);
    return LAVA_RELIEF * tanh_approx(LAVA_SOFT * h) / LAVA_SOFT;
}

/* MARK: - The grade */

/* postProcess's per-channel part: pow(c, 0.75), then the contrast curve. */
#define CURVE_N 1024
static float curve[CURVE_N + 1];
static int curve_ready;

static void curve_init(void) {
    if (curve_ready) return;
    for (int i = 0; i <= CURVE_N; i++) {
        float c = (float)pow((double)i / CURVE_N, 0.75);
        curve[i] = c * 0.6f + 0.4f * c * c * (3.0f - 2.0f * c);
    }
    curve_ready = 1;
}

static inline float curve_at(float c) {
    if (c <= 0) return curve[0];
    if (c >= 1) return curve[CURVE_N];
    float u = c * CURVE_N;
    int i = (int)u;
    return curve[i] + (curve[i + 1] - curve[i]) * (u - (float)i);
}

static inline float smoothstep_band(float x) {
    float t = x / LAVA_BAND;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return t * t * (3.0f - 2.0f * t);
}

static inline unsigned char byte_of(float c) {
    int v = (int)(c * 255.0f + 0.5f);
    return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
}

/* MARK: - Frames */

void lp_lava_frame_size(int w, int h, int *fw, int *fh) {
    int cw = (w + LP_LAVA_SCALE - 1) / LP_LAVA_SCALE, ch = (h + LP_LAVA_SCALE - 1) / LP_LAVA_SCALE;
    if (cw < 1) cw = 1;
    if (ch < 1) ch = 1;
    *fw = cw + 2 * LP_LAVA_MARGIN;
    *fh = ch + 2 * LP_LAVA_MARGIN;
}

static float grid_at(const float *hg, int fw, int fh, float i, float j) {
    if (i < 0) i = 0;
    if (j < 0) j = 0;
    if (i > fw - 1) i = (float)(fw - 1);
    if (j > fh - 1) j = (float)(fh - 1);
    int i0 = (int)i, j0 = (int)j, i1 = i0 + 1 < fw ? i0 + 1 : i0, j1 = j0 + 1 < fh ? j0 + 1 : j0;
    float u = i - (float)i0, v = j - (float)j0;
    float a = hg[j0 * fw + i0], b = hg[j0 * fw + i1], c = hg[j1 * fw + i0], d = hg[j1 * fw + i1];
    return a + (b - a) * u + (c - a) * v + (d - c + a - b) * u * v;
}

cairo_surface_t *lp_lava_frame(int w, int h, double time, enum lp_molten_tone tone) {
    if (w <= 0 || h <= 0) return NULL;
    sin_init();
    curve_init();
    int fw, fh;
    lp_lava_frame_size(w, h, &fw, &fh);
    int cw = fw - 2 * LP_LAVA_MARGIN, ch = fh - 2 * LP_LAVA_MARGIN;
    cairo_surface_t *out = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, fw, fh);
    float *hg = malloc((size_t)fw * fh * sizeof *hg), *vx = malloc((size_t)fw * sizeof *vx), *vy = malloc((size_t)fh * sizeof *vy);
    float *px = malloc((size_t)fw * sizeof *px), *py = malloc((size_t)fh * sizeof *py);
    if (cairo_surface_status(out) != CAIRO_STATUS_SUCCESS || !hg || !vx || !vy || !px || !py) {
        cairo_surface_destroy(out);
        free(hg); free(vx); free(vy); free(px); free(py);
        return NULL;
    }
    float aspect = (float)w / (float)h, t = (float)time;
    /* where each field column and row sits in p, and the vignette's two halves (pow(a·b, .1) = pow(a, .1)·pow(b, .1)) */
    /* the margin takes the nearest interior pixel's vignette: past the edge the shader's falls to nothing, and the
     * stretch would pull that dark into the output's outermost pixels */
    float qlo_x = 0.5f / (float)cw, qlo_y = 0.5f / (float)ch;
    for (int i = 0; i < fw; i++) {
        float q = ((float)(i - LP_LAVA_MARGIN) + 0.5f) / (float)cw;
        px[i] = (-1.0f + 2.0f * q) * aspect;
        float qv = q < qlo_x ? qlo_x : q > 1.0f - qlo_x ? 1.0f - qlo_x : q;
        float k = qv * (1.0f - qv);
        vx[i] = k > 0 ? (float)pow(k, 0.1) : 0;
    }
    for (int j = 0; j < fh; j++) {
        float q = 1.0f - ((float)(j - LP_LAVA_MARGIN) + 0.5f) / (float)ch;   /* y up, as gl_FragCoord */
        py[j] = -1.0f + 2.0f * q;
        float qv = q < qlo_y ? qlo_y : q > 1.0f - qlo_y ? 1.0f - qlo_y : q;
        float k = qv * (1.0f - qv);
        vy[j] = k > 0 ? (float)pow(k, 0.1) : 0;
    }
    for (int j = 0; j < fh; j++)
        for (int i = 0; i < fw; i++) hg[j * fw + i] = height(px[i], py[j], t);

    lp_molten_grade g = lp_molten_grade_for(tone);
    float br = g.base.r, bg = g.base.g, bb = g.base.b;
    float dxp = 2.0f * aspect / (float)cw, dyp = 2.0f / (float)ch;
    float vig19 = (float)pow(19.0, 0.1);
    cairo_surface_flush(out);
    unsigned char *data = cairo_image_surface_get_data(out);
    int stride = cairo_image_surface_get_stride(out);
    for (int j = 0; j < fh; j++) {
        uint32_t *row = (uint32_t *)(void *)(data + (size_t)j * stride);
        int ju = j > 0 ? j - 1 : j, jd = j + 1 < fh ? j + 1 : j;
        for (int i = 0; i < fw; i++) {
            int il = i > 0 ? i - 1 : i, ir = i + 1 < fw ? i + 1 : i;
            float hh = hg[j * fw + i];
            /* the shader's normal, from the grid: -(dh/dx, 1, dh/dz) normalised */
            float gx = (hg[j * fw + ir] - hg[j * fw + il]) / ((float)(ir - il) * dxp);
            float gz = (hg[ju * fw + i] - hg[jd * fw + i]) / ((float)(jd - ju) * dyp);
            float inv = 1.0f / sqrtf(gx * gx + 1.0f + gz * gz);
            float nx = -gx * inv, ny = -inv, nz = -gz * inv;
            float X = px[i], Z = py[j];
            /* the two lights: lp1 (0.4, -0.5, 0.5), lp2 (-0.1, -0.5, 0.5) */
            float l1x = 0.4f - X, l1y = -0.5f - hh, l1z = 0.5f - Z;
            float l2x = -0.1f - X, l2y = -0.5f - hh, l2z = 0.5f - Z;
            float n1 = 1.0f / sqrtf(l1x * l1x + l1y * l1y + l1z * l1z), n2 = 1.0f / sqrtf(l2x * l2x + l2y * l2y + l2z * l2z);
            float d1 = (l1x * nx + l1y * ny + l1z * nz) * n1, d2 = (l2x * nx + l2y * ny + l2z * nz) * n2;
            d1 = d1 > 0 ? d1 : 0;
            d2 = d2 > 0 ? d2 : 0;
            /* the shadow: the height a little toward light one, 0.05·(lp1.xz − p) away */
            float oh = grid_at(hg, fw, fh, (float)i + 0.05f * l1x / dxp, (float)j - 0.05f * l1z / dyp);
            float shade = (smoothstep_band(hh) - smoothstep_band(oh)) * LAVA_SHADOW;
            float k1 = d1 * sqrtf(d1) + 0.5f * sqrtf(d1);
            float d22 = d2 * d2, d27 = d22 * d22 * d22 * d2;
            float k2 = 0.5f * d27 + 0.015f * d22;
            float r = br * k1 + bb * k2 + br * shade;   /* baseCol.zyx for the second light */
            float gg = bg * k1 + bg * k2 + bg * shade;
            float b = bb * k1 + br * k2 + bb * shade;
            r = curve_at(r);
            gg = curve_at(gg);
            b = curve_at(b);
            float lum = 0.13f * (r + gg + b);
            r += (lum - r) * g.saturation;
            gg += (lum - gg) * g.saturation;
            b += (lum - b) * g.saturation;
            float v = 0.5f + 0.5f * vig19 * vx[i] * vy[j];
            r = (g.lift + g.gain * r);
            gg = (g.lift + g.gain * gg);
            b = (g.lift + g.gain * b);
            r = (r < 0 ? 0 : r > 1 ? 1 : r) * v;
            gg = (gg < 0 ? 0 : gg > 1 ? 1 : gg) * v;
            b = (b < 0 ? 0 : b > 1 ? 1 : b) * v;
            row[i] = 0xff000000u | (uint32_t)byte_of(r) << 16 | (uint32_t)byte_of(gg) << 8 | byte_of(b);
        }
    }
    cairo_surface_mark_dirty(out);
    free(hg); free(vx); free(vy); free(px); free(py);
    return out;
}

cairo_surface_t *lp_lava_still(int w, int h, double time, enum lp_molten_tone tone) {
    cairo_surface_t *frame = lp_lava_frame(w, h, time, tone);
    if (!frame) return NULL;
    int fw = cairo_image_surface_get_width(frame), fh = cairo_image_surface_get_height(frame);
    int cw = fw - 2 * LP_LAVA_MARGIN, ch = fh - 2 * LP_LAVA_MARGIN;
    cairo_surface_t *out = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(out);
    cairo_scale(cr, (double)w / cw, (double)h / ch);
    cairo_set_source_surface(cr, frame, -LP_LAVA_MARGIN, -LP_LAVA_MARGIN);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(frame);
    return out;
}
