/**
 * The shared lighting recipe: everything an object icon is, apart from its
 * geometry. objects.json carries no colour at all, so this module and the
 * `object` group in tokens.json are the only places the set's appearance is
 * decided — change a token and all of it regrades, the way the wallpaper
 * regrades from `molten`.
 */

import { tokens } from '@/tokens/tokens'

export type MaterialName = keyof typeof tokens.object.material
export type Facet = 'top' | 'front' | 'under' | 'flat'
export type Shape = 'flat' | 'sphere' | 'dome' | 'cylX' | 'cylY' | 'concave'
export type Finish = 'lit' | 'glossy' | 'matte'

const O = tokens.object

export const VIEWBOX = O.viewbox

/* ---- colour maths on the material ramp ---------------------------------- */

const channels = (hex: string) => [1, 3, 5].map((i) => Number.parseInt(hex.slice(i, i + 2), 16))
const toHex = (rgb: number[]) =>
  '#' + rgb.map((v) => Math.max(0, Math.min(255, Math.round(v))).toString(16).padStart(2, '0')).join('')

/** k > 0 lightens toward white, k < 0 darkens toward black. */
export function shade(hex: string, k: number): string {
  return toHex(channels(hex).map((v) => (k >= 0 ? v + (255 - v) * k : v * (1 + k))))
}

const mix = (a: string, b: string, t: number) => {
  const [x, y] = [channels(a), channels(b)]
  return toHex(x.map((v, i) => v + (y[i] - v) * t))
}

/**
 * Two materials have more than one appearance, so their ramps cannot be
 * computed here: base.css maps the chosen one onto CSS variables and they
 * resolve at paint time. `accent` follows the Blue / Graphite preference;
 * `folder` follows Manila / Slate. The concrete values under
 * object.material.accent and .folder are the defaults, kept so the C target and
 * the Sketch export still get a real ramp.
 */
const DYNAMIC: Partial<Record<MaterialName, readonly [string, string, string]>> = {
  accent: ['var(--lp-accent-light)', 'var(--lp-accent-base)', 'var(--lp-accent-deep)'],
  folder: ['var(--lp-object-folder-hi)', 'var(--lp-object-folder-mid)', 'var(--lp-object-folder-lo)'],
}

const dynamicAt = (stops: readonly [string, string, string], t: number) => {
  const [from, to] = t < 0.5 ? [stops[0], stops[1]] : [stops[1], stops[2]]
  const local = Math.round((t < 0.5 ? t / 0.5 : Math.min(1, (t - 0.5) / 0.5)) * 100)
  const base = `color-mix(in srgb, ${to} ${local}%, ${from})`
  return t > 1 ? `color-mix(in srgb, #000 ${Math.round((t - 1) * 100)}%, ${base})` : base
}

/**
 * A point on a material's ramp: 0 is `hi`, 0.5 is `mid`, 1 is `lo`, and past 1
 * it keeps darkening past the ramp for a face turned away from the light.
 */
export function rampAt(material: MaterialName, t: number): string {
  const dynamic = DYNAMIC[material]
  if (dynamic) return dynamicAt(dynamic, Math.max(0, t))
  const m = O.material[material]
  if (t <= 0) return m.hi
  if (t < 0.5) return mix(m.hi, m.mid, t / 0.5)
  if (t <= 1) return mix(m.mid, m.lo, (t - 0.5) / 0.5)
  return shade(m.lo, -(t - 1))
}

/**
 * Which stretch of the ramp a face occupies, by how it is turned to the key.
 * `headroom` slides both ends down the ramp so the key light has somewhere to
 * go: a white key screened over an already near-white body does nothing at all,
 * and the object reads as painted rather than lit.
 */
const FACET_RANGE: Record<Facet, [number, number]> = {
  top: [0.2, 0.72],
  flat: [0.12, 0.62],
  front: [0.5, 1.0],
  under: [0.88, 1.25],
}

export function facetStops(material: MaterialName, facet: Facet = 'flat'): [string, string] {
  const [from, to] = FACET_RANGE[facet]
  return [rampAt(material, from + O.headroom), rampAt(material, to + O.headroom)]
}

export const MATERIALS = Object.keys(O.material) as MaterialName[]

/**
 * Materials whose ramp is a CSS variable rather than a literal. Their gradients
 * cannot be shared from SvgDefs: a `var()` inside a gradient stop resolves
 * against the gradient ELEMENT's context, and SvgDefs sits at the app root — so
 * a shared gradient would always read the root's appearance and an icon inside
 * a subtree that set its own could never differ. These get a gradient per
 * instance instead, which is one extra node and buys both the global switch and
 * side-by-side comparison.
 */
export const isDynamic = (m: MaterialName): boolean => m in DYNAMIC

/** The materials SvgDefs can safely share. */
export const STATIC_MATERIALS = (Object.keys(O.material) as MaterialName[]).filter((m) => !isDynamic(m))
export const FACETS = Object.keys(FACET_RANGE) as Facet[]

/* ---- shared def ids ------------------------------------------------------ */

export const bodyGradientId = (m: MaterialName, f: Facet) => `lp-obj-body-${m}-${f}`
export const KEY_GRADIENT_ID = 'lp-obj-key'
export const GLOSS_GRADIENT_ID = 'lp-obj-gloss'
export const BRUSH_PATTERN_ID = 'lp-obj-brush'
export const CONTACT_FILTER_ID = 'lp-obj-contact'
export const shapeGradientId = (m: MaterialName, sh: Shape) => `lp-obj-shape-${m}-${sh}`
export const MATTE_KEY_GRADIENT_ID = 'lp-obj-matte-key'
export const CAST_FILTER_ID = 'lp-obj-cast'
export const AO_FILTER_ID = 'lp-obj-ao'

/* ---- tints --------------------------------------------------------------- */

/**
 * A dotted tokens.json path to a paintable value. Anything under accent.blue.*
 * is redirected to the live --lp-accent-*, so a tinted detail follows the
 * appearance preference rather than pinning itself to blue.
 */
export function tintColor(path: string): string {
  const accent = /^accent\.(?:blue|graphite|verdigris)\.(.+)$/.exec(path)
  if (accent) return `var(--lp-accent-${accent[1]})`
  return `var(--lp-${path.replace(/\./g, '-')})`
}

/* ---- the passes ---------------------------------------------------------- */

export const KEY_STOPS = [
  { offset: 0, opacity: O.keyAlpha },
  { offset: O.keyMidAt, opacity: O.keyMid },
  { offset: O.keySpread, opacity: 0 },
]

/**
 * The glossy finish: Aqua's sweep over the top of the object, ending on a hard
 * waterline, with light bounced back along the bottom edge. It runs ON TOP of
 * the lit passes rather than instead of them, which is the whole point — the
 * body underneath is still a lit material, so a glossy icon still regrades and
 * is still lit by the same lamp as everything else. Reserved for application
 * icons; system objects stay on the plain lit finish.
 */
export const GLOSS_STOPS = [
  { offset: 0, opacity: O.glossAlpha },
  { offset: O.glossBreak, opacity: O.glossShoulder },
  { offset: O.glossBreak + 0.001, opacity: 0 },
  { offset: 0.82, opacity: 0 },
  { offset: 1, opacity: O.glossBounce },
]

/* ---- form ---------------------------------------------------------------- */

/**
 * Where light sits on a curved face, as positions along the material ramp.
 * `facet` says which stretch of the ramp a face occupies; `shape` says how the
 * light travels ACROSS it. Without this every part takes the same diagonal
 * ramp, and a ball, a barrel, a dished screen and a flat card are all shaded
 * identically — which is what makes a set of icons read as diagrams with a
 * gloss on them rather than as objects.
 *
 * Held in two places, like FACET_RANGE: these numbers are also in
 * form_pattern() in src/draw/lp_object_icon.c.
 */
export const SHAPE_STOPS: Record<Exclude<Shape, 'flat'>, { offset: number; t: number }[]> = {
  /* A ball: hot near the lamp, falling away to a limb darker than the ramp's own low end. */
  sphere: [
    { offset: 0, t: 0.04 },
    { offset: 0.42, t: 0.3 },
    { offset: 0.74, t: 0.64 },
    { offset: 0.92, t: 0.99 },
    { offset: 1, t: 1.2 },
  ],
  /* A shallow cap — a bead, a button, a watch glass. */
  dome: [
    { offset: 0, t: 0.1 },
    { offset: 0.55, t: 0.38 },
    { offset: 1, t: 0.86 },
  ],
  /* A barrel keeps one bright line down its length and darkens to both edges.
   * cylX lies across the icon and shades top to bottom; cylY stands up. */
  cylX: [],
  cylY: [],
  /* A dish: the wall facing the lamp is the one in shadow. */
  concave: [
    { offset: 0, t: 1.12 },
    { offset: 0.45, t: 0.74 },
    { offset: 1, t: 0.22 },
  ],
}

/** Where a barrel's bright line sits across its width, from object.light-azimuth. */
export function cylinderHighlight(shape: 'cylX' | 'cylY'): number {
  const a = (Number(O.lightAzimuth) * Math.PI) / 180
  return 0.5 + (shape === 'cylY' ? Math.cos(a) : Math.sin(a)) * 0.28
}

export function cylinderStops(shape: 'cylX' | 'cylY'): { offset: number; t: number }[] {
  const t = cylinderHighlight(shape)
  return [
    { offset: 0, t: 1.02 },
    ...(t > 0.22 ? [{ offset: t - 0.2, t: 0.4 }] : []),
    { offset: t, t: 0.05 },
    { offset: t + (1 - t) * 0.45, t: 0.52 },
    { offset: 1, t: 1.1 },
  ]
}

/** Every stop of a shape, with the matte lift already applied where it applies. */
export function shapeStops(shape: Exclude<Shape, 'flat'>, finish: Finish): { offset: number; t: number }[] {
  const lift = finish === 'matte' ? O.matteFormLift : 0
  const base = shape === 'cylX' || shape === 'cylY' ? cylinderStops(shape) : SHAPE_STOPS[shape]
  /* The dark end of a curve is already turned away from the lamp; lifting it as
   * far as the bright end would flatten the form instead of un-polishing it. */
  return base.map((st) => ({ offset: st.offset, t: st.t + lift * (st.t > 0.7 ? 0.5 : 1) }))
}

/**
 * The gradient geometry a shape needs, in objectBoundingBox units — a radial
 * for the two caps, a linear across the axis for the barrels, and for a dish a
 * linear along the light axis pointing back at the lamp, because the near wall
 * is the dark one.
 */
export function formGradient(shape: Exclude<Shape, 'flat'>):
  | { kind: 'radial'; cx: number; cy: number; r: number }
  | { kind: 'linear'; x1: number; y1: number; x2: number; y2: number } {
  const a = (Number(O.lightAzimuth) * Math.PI) / 180
  const ux = Math.cos(a)
  const uy = Math.sin(a)
  const half = Math.SQRT2 / 2
  if (shape === 'sphere' || shape === 'dome')
    return { kind: 'radial', cx: 0.5 + ux * 0.17, cy: 0.5 + uy * 0.17, r: half * (shape === 'dome' ? 1.15 : 0.98) }
  if (shape === 'cylY') return { kind: 'linear', x1: 0, y1: 0, x2: 1, y2: 0 }
  if (shape === 'cylX') return { kind: 'linear', x1: 0, y1: 0, x2: 0, y2: 1 }
  return { kind: 'linear', x1: 0.5 + ux * 0.5, y1: 0.5 + uy * 0.5, x2: 0.5 - ux * 0.5, y2: 0.5 - uy * 0.5 }
}

/** Where the lamp sits inside a sphere or a dome, in objectBoundingBox units. */
export function formLamp(): { cx: number; cy: number } {
  const a = (Number(O.lightAzimuth) * Math.PI) / 180
  return { cx: 0.5 + Math.cos(a) * 0.17, cy: 0.5 + Math.sin(a) * 0.17 }
}

/* ---- matte --------------------------------------------------------------- */

/**
 * A matte surface scatters what falls on it. There is no hot corner to put a
 * glint in, so it gets one broad wash down from the top — close to vertical
 * rather than the specular diagonal, because a diffuse surface shows you where
 * the light is, not where you are. It shows nothing of the room, and it
 * overrides how specular its material claims to be rather than being scaled by
 * it: ruby and amber are 0.9 by default, and asking for matte is asking to
 * override that.
 */
export const MATTE_KEY_STOPS = [
  { offset: 0, opacity: O.matteKeyAlpha },
  { offset: O.matteKeyMidAt, opacity: O.matteKeyMid },
  { offset: 1, opacity: 0 },
]

export const MATTE = {
  glossMax: O.matteGlossMax,
  rim: O.matteRim,
  formLift: O.matteFormLift,
}

/** A rim is a specular line; on a matte edge it runs at matte-rim instead. */
export const rimFor = (finish: Finish): string =>
  finish === 'matte' ? O.rim.replace(/[\d.]+\)$/, `${O.matteRim})`) : O.rim

/* ---- shadows between parts ----------------------------------------------- */

export const CAST = { blur: O.castBlur, dist: O.castDist, color: O.castColor }
export const AO = { blur: O.aoBlur, dist: O.aoDist, color: O.aoColor }

/** Away from the lamp for a cast shadow, toward it for occlusion. */
export function shadowOffset(kind: 'cast' | 'ao'): { dx: number; dy: number } {
  const a = (Number(O.lightAzimuth) * Math.PI) / 180
  const s = kind === 'cast' ? -CAST.dist : AO.dist
  return { dx: Math.cos(a) * s, dy: Math.sin(a) * s }
}

/* ---- the keyline --------------------------------------------------------- */

/**
 * The keyline is what keeps an object legible when it is small, and only that.
 * Past the plain tier the material's own dark end and the contact shadow carry
 * the edge, so it fades out — at full strength on a large icon it reads as a
 * die-cut outline round a sticker rather than the edge of a thing.
 */
export function keylineAlpha(size: number): number {
    const plain = Number.parseFloat(O.tierPlainMax)
  const fadeAt = Number.parseFloat(O.keylineFadeAt)
  const t = Math.min(1, Math.max(0, (size - plain) / (fadeAt - plain)))
  const base = Number.parseFloat(/[\d.]+\)$/.exec(O.keyline)?.[0] ?? '0.32')
  return base * (1 - t) + O.keylineFaded * t
}

export const RECIPE = {
  toneStep: O.toneStep,
  grainOpacity: O.grainOpacity,
  grainTile: O.grainTile,
  rim: O.rim,
  occlusion: O.occlusion,
  keyline: O.keyline,
  contactDy: Number.parseFloat(O.contactDy),
  contactBlur: Number.parseFloat(O.contactBlur),
  contactColor: O.contactColor,
  /** How far the 1px pair is pushed along the light axis, in grid units. */
  bevelOffset: 0.8,
  gloss: (m: MaterialName) => O.material[m].gloss,
  grain: (m: MaterialName) => O.material[m].grain,
}
