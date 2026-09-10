/**
 * The molten-platinum wallpaper, as GLSL.
 *
 * A domain-warped fbm height field lit by two lights, after Mårten Rånge's
 * shader. Three things were added to make it belong to this system rather than
 * merely sit behind it:
 *
 *  - the grade is a uniform (`u_base`, `u_lift`, `u_gain`, `u_saturation`), so
 *    the same field can be shown in the platinum ramp's cool cast or in the
 *    shader's own dark original;
 *  - `u_time` is *window motion*, not wall-clock time — the Wallpaper advances
 *    it only while the desktop is already animating (see `Wallpaper.tsx`);
 *  - the vignette matches the CSS one that already sits over the wallpaper.
 *
 * Framework-free, like `brushSvg.ts` and `wallpaperSvg.ts`.
 */

export const MOLTEN_VERTEX = `
attribute vec2 a_position;
void main() {
  gl_Position = vec4(a_position, 0.0, 1.0);
}
`

/**
 * `onoise` is what gives this its character: two sines crossfaded by a third
 * function of their own product, which folds rather than blurs and leaves the
 * creases that read as hammered metal. `vnoise` only breaks up the fbm's sign.
 */
export const MOLTEN_FRAGMENT = `
precision highp float;

uniform vec2 u_resolution;
uniform float u_time;
uniform float u_zoom;
uniform vec3 u_base;
uniform float u_lift;
uniform float u_gain;
uniform float u_saturation;

#define PI  3.141592654
#define TAU (2.0*PI)

void rot(inout vec2 p, float a) {
  float c = cos(a);
  float s = sin(a);
  p = vec2(c*p.x + s*p.y, -s*p.x + c*p.y);
}

float hash(in vec2 co) {
  return fract(sin(dot(co.xy, vec2(12.9898, 58.233))) * 13758.5453);
}

float psin(float a) { return 0.5 + 0.5*sin(a); }

float tanh_approx(float x) {
  float x2 = x*x;
  return clamp(x*(27.0 + x2)/(27.0 + 9.0*x2), -1.0, 1.0);
}

float onoise(vec2 x) {
  x *= 0.5;
  float a = sin(x.x);
  float b = sin(x.y);
  return mix(a, b, psin(TAU*tanh_approx(a*b + a + b)));
}

float vnoise(vec2 x) {
  vec2 i = floor(x);
  vec2 w = fract(x);
  vec2 u = w*w*w*(w*(w*6.0 - 15.0) + 10.0);
  float a = hash(i + vec2(0.0, 0.0));
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));
  return a + (b - a)*u.x + (c - a)*u.y + (d - c + a - b)*u.x*u.y;
}

float fbm(vec2 p, const int octaves) {
  vec2 op = p;
  const float aa = 0.45;
  const float pp = 2.03;
  const vec2 oo = -vec2(1.23, 1.5);
  const float rr = 1.2;
  float h = 0.0;
  float d = 0.0;
  float a = 1.0;
  for (int i = 0; i < 7; ++i) {
    if (i >= octaves) break;
    h += a*onoise(p);
    d += a;
    a *= aa;
    p += oo;
    p *= pp;
    rot(p, rr);
  }
  return mix((h/d), -0.5*(h/d), pow(vnoise(0.9*op), 0.25));
}

float warp(vec2 p) {
  vec2 v = vec2(fbm(p, 5), fbm(p + 0.7*vec2(1.0, 1.0), 5));
  rot(v, 1.0 + u_time*0.1);
  vec2 vv = vec2(fbm(p + 3.7*v, 7), fbm(p - 2.7*v.yx + 0.7*vec2(1.0, 1.0), 7));
  rot(vv, -1.0 + u_time*0.21315);
  return fbm(p + 1.4*vv, 3);
}

float height(vec2 p) {
  float a = 0.005*u_time;
  p += 5.0*vec2(cos(a), sin(a));
  p *= 2.0;
  p += 13.0;
  float rs = 3.0;
  return 0.35*tanh_approx(rs*warp(p))/rs;
}

vec3 normal(vec2 p) {
  vec2 eps = -vec2(1.0/u_resolution.y, 0.0);
  vec3 n;
  n.x = height(p + eps.xy) - height(p - eps.xy);
  n.y = 2.0*eps.x;
  n.z = height(p + eps.yx) - height(p - eps.yx);
  return normalize(n);
}

/** Contrast, then the grade: saturation, black point and gain, then vignette. */
vec3 postProcess(vec3 col, vec2 q) {
  col = pow(clamp(col, 0.0, 1.0), vec3(0.75));
  col = col*0.6 + 0.4*col*col*(3.0 - 2.0*col);
  col = mix(col, vec3(dot(col, vec3(0.13))), u_saturation);
  col = clamp(u_lift + u_gain*col, 0.0, 1.0);
  col *= 0.5 + 0.5*pow(19.0*q.x*q.y*(1.0 - q.x)*(1.0 - q.y), 0.1);
  return col;
}

void main() {
  vec2 q = gl_FragCoord.xy/u_resolution.xy;
  vec2 p = -1.0 + 2.0*q;
  p.x *= u_resolution.x/u_resolution.y;
  p /= u_zoom;

  const vec3 lp1 = vec3(0.4, -0.5, 0.5);
  const vec3 lp2 = vec3(-0.1, -0.5, 0.5);

  float h = height(p);
  vec3 pp = vec3(p.x, h, p.y);
  float ll1 = length(lp1.xz - pp.xz);
  vec3 ld1 = normalize(lp1 - pp);
  vec3 ld2 = normalize(lp2 - pp);

  vec3 n = normal(p);
  float diff1 = max(dot(ld1, n), 0.0);
  float diff2 = max(dot(ld2, n), 0.0);

  vec3 baseCol = u_base;

  // A shadow by sampling the height again toward the light: cheaper than
  // marching, and it is what gives the ridges their cast edge.
  float oh = height(p + ll1*0.05*normalize(ld1.xz));
  vec3 scol = baseCol*(smoothstep(0.0, 0.15, h) - smoothstep(0.0, 0.15, oh));

  vec3 col = vec3(0.0);
  col += baseCol*pow(diff1, 1.5);
  col += 0.5*baseCol*pow(diff1, 0.5);
  col += 0.5*baseCol.zyx*pow(diff2, 7.0);
  col += 0.015*baseCol.zyx*pow(diff2, 2.0);
  col += scol*0.7;

  gl_FragColor = vec4(postProcess(col, q), 1.0);
}
`

export interface MoltenGrade {
  /** Base reflectance, 0..1 per channel. */
  base: [number, number, number]
  lift: number
  gain: number
  saturation: number
}

/** #rrggbb → 0..1 triple. */
export function hexToRgb(hex: string): [number, number, number] {
  const n = parseInt(hex.replace('#', ''), 16)
  return [((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255]
}
