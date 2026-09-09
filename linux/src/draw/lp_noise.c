/* A transliteration of the reference code in the SVG 1.1 specification,
 * §15.17 feTurbulence (the code there is public). Variable names follow it
 * so the two can be compared line by line. */
#include <math.h>
#include <stddef.h>

#include "maryui/lp_noise.h"

#define BSize LP_NOISE_BSIZE
#define BM 0xff
#define PerlinN 0x1000
#define RAND_m 2147483647 /* 2**31 - 1 */
#define RAND_a 16807      /* 7**5; primitive root of m */
#define RAND_q 127773     /* m / a */
#define RAND_r 2836       /* m % a */

static long setup_seed(long lSeed) {
    if (lSeed <= 0) lSeed = -(lSeed % (RAND_m - 1)) + 1;
    if (lSeed > RAND_m - 1) lSeed = RAND_m - 1;
    return lSeed;
}

static long random_next(long lSeed) {
    long result = RAND_a * (lSeed % RAND_q) - RAND_r * (lSeed / RAND_q);
    if (result <= 0) result += RAND_m;
    return result;
}

void lp_turbulence_init(lp_turbulence *t, int seed) {
    double s;
    int i, j, k;
    long lSeed = setup_seed(seed);
    for (k = 0; k < 4; k++) {
        for (i = 0; i < BSize; i++) {
            t->lattice[i] = (uint32_t)i;
            for (j = 0; j < 2; j++) {
                t->gradient[k][i][j] = (double)(((lSeed = random_next(lSeed)) % (BSize + BSize)) - BSize) / BSize;
            }
            s = sqrt(t->gradient[k][i][0] * t->gradient[k][i][0] + t->gradient[k][i][1] * t->gradient[k][i][1]);
            t->gradient[k][i][0] /= s;
            t->gradient[k][i][1] /= s;
        }
    }
    while (--i) {
        k = (int)t->lattice[i];
        t->lattice[i] = t->lattice[j = (int)((lSeed = random_next(lSeed)) % BSize)];
        t->lattice[j] = (uint32_t)k;
    }
    for (i = 0; i < BSize + 2; i++) {
        t->lattice[BSize + i] = t->lattice[i];
        for (k = 0; k < 4; k++) {
            for (j = 0; j < 2; j++) t->gradient[k][BSize + i][j] = t->gradient[k][i][j];
        }
    }
}

#define s_curve(t) ((t) * (t) * (3. - 2. * (t)))
#define lerp(t, a, b) ((a) + (t) * ((b) - (a)))

struct stitch_info {
    int nWidth, nHeight, nWrapX, nWrapY;
};

static double noise2(const lp_turbulence *t, int nColorChannel, const double vec[2], const struct stitch_info *pStitchInfo) {
    int bx0, bx1, by0, by1, b00, b10, b01, b11;
    double rx0, rx1, ry0, ry1, sx, sy, a, b, u, v, tt;
    const double *q;
    int i, j;
    /* The specification's listing masks bx0/by0 with BM before the stitch
     * comparison, which makes stitching a no-op; browsers (WebKit, Skia)
     * compare the unmasked lattice index and mask afterwards. Do the same. */
    tt = vec[0] + PerlinN;
    bx0 = (int)tt;
    bx1 = bx0 + 1;
    rx0 = tt - (int)tt;
    rx1 = rx0 - 1.0;
    tt = vec[1] + PerlinN;
    by0 = (int)tt;
    by1 = by0 + 1;
    ry0 = tt - (int)tt;
    ry1 = ry0 - 1.0;
    if (pStitchInfo != NULL) {
        if (bx0 >= pStitchInfo->nWrapX) bx0 -= pStitchInfo->nWidth;
        if (bx1 >= pStitchInfo->nWrapX) bx1 -= pStitchInfo->nWidth;
        if (by0 >= pStitchInfo->nWrapY) by0 -= pStitchInfo->nHeight;
        if (by1 >= pStitchInfo->nWrapY) by1 -= pStitchInfo->nHeight;
    }
    bx0 &= BM;
    bx1 &= BM;
    by0 &= BM;
    by1 &= BM;
    i = (int)t->lattice[bx0];
    j = (int)t->lattice[bx1];
    b00 = (int)t->lattice[i + by0];
    b10 = (int)t->lattice[j + by0];
    b01 = (int)t->lattice[i + by1];
    b11 = (int)t->lattice[j + by1];
    sx = s_curve(rx0);
    sy = s_curve(ry0);
    q = t->gradient[nColorChannel][b00]; u = rx0 * q[0] + ry0 * q[1];
    q = t->gradient[nColorChannel][b10]; v = rx1 * q[0] + ry0 * q[1];
    a = lerp(sx, u, v);
    q = t->gradient[nColorChannel][b01]; u = rx0 * q[0] + ry1 * q[1];
    q = t->gradient[nColorChannel][b11]; v = rx1 * q[0] + ry1 * q[1];
    b = lerp(sx, u, v);
    return lerp(sy, a, b);
}

double lp_turbulence_sum(const lp_turbulence *t, int channel, double x, double y,
                         double fBaseFreqX, double fBaseFreqY, int nNumOctaves, int bFractalSum, const lp_stitch *tile) {
    struct stitch_info stitch;
    struct stitch_info *pStitchInfo = NULL;
    if (tile != NULL) {
        /* Adjust the base frequencies so the noise repeats across the tile. */
        if (fBaseFreqX != 0.0) {
            double fLoFreq = floor(tile->w * fBaseFreqX) / tile->w;
            double fHiFreq = ceil(tile->w * fBaseFreqX) / tile->w;
            if (fBaseFreqX / fLoFreq < fHiFreq / fBaseFreqX) fBaseFreqX = fLoFreq; else fBaseFreqX = fHiFreq;
        }
        if (fBaseFreqY != 0.0) {
            double fLoFreq = floor(tile->h * fBaseFreqY) / tile->h;
            double fHiFreq = ceil(tile->h * fBaseFreqY) / tile->h;
            if (fBaseFreqY / fLoFreq < fHiFreq / fBaseFreqY) fBaseFreqY = fLoFreq; else fBaseFreqY = fHiFreq;
        }
        pStitchInfo = &stitch;
        stitch.nWidth = (int)(tile->w * fBaseFreqX + 0.5);
        stitch.nWrapX = (int)(tile->x * fBaseFreqX + PerlinN + stitch.nWidth);
        stitch.nHeight = (int)(tile->h * fBaseFreqY + 0.5);
        stitch.nWrapY = (int)(tile->y * fBaseFreqY + PerlinN + stitch.nHeight);
    }
    double fSum = 0.0;
    double vec[2] = { x * fBaseFreqX, y * fBaseFreqY };
    double ratio = 1;
    for (int nOctave = 0; nOctave < nNumOctaves; nOctave++) {
        if (bFractalSum) fSum += noise2(t, channel, vec, pStitchInfo) / ratio;
        else fSum += fabs(noise2(t, channel, vec, pStitchInfo)) / ratio;
        vec[0] *= 2;
        vec[1] *= 2;
        ratio *= 2;
        if (pStitchInfo != NULL) {
            stitch.nWidth *= 2;
            stitch.nWrapX = 2 * stitch.nWrapX - PerlinN;
            stitch.nHeight *= 2;
            stitch.nWrapY = 2 * stitch.nWrapY - PerlinN;
        }
    }
    return fSum;
}

static double clamp01(double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
double lp_noise_fractal_value(double sum) { return clamp01((sum + 1) / 2); }
double lp_noise_turbulence_value(double sum) { return clamp01(sum); }
