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

/* ---- tints --------------------------------------------------------------- */

/**
 * A dotted tokens.json path to a paintable value. Anything under accent.blue.*
 * is redirected to the live --lp-accent-*, so a tinted detail follows the
 * appearance preference rather than pinning itself to blue.
 */
export function tintColor(path: string): string {
  const accent = /^accent\.(?:blue|graphite)\.(.+)$/.exec(path)
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
