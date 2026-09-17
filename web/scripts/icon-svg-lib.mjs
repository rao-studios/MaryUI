/**
 * The object recipe in plain ESM, for the .svg export and the Sketch sheet.
 *
 * Sketch has no CSS variables, so every colour resolves to a literal here and
 * the two materials with appearances (`accent`, `folder`) are baked to their
 * default. That is the one thing this cannot share with recipe.ts, which must
 * leave them as var() for the runtime switch — so the ramp maths is mirrored
 * rather than imported, and scripts/icon-svg.test.ts asserts the two agree for
 * every material and facet. If that test fails, this file drifted.
 */

import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const at = (url) => fileURLToPath(new URL(url, import.meta.url))
export const tokensTree = JSON.parse(readFileSync(at('../tokens/tokens.json'), 'utf8'))
export const objects = JSON.parse(readFileSync(at('../src/components/ObjectIcon/objects.json'), 'utf8'))
export const glyphs = JSON.parse(readFileSync(at('../src/components/Icon/icons.json'), 'utf8'))

/** Resolve a dotted tokens.json path, following {aliases} to a literal. */
export function token(path) {
  const node = path.split('.').reduce((n, k) => (n == null ? n : n[k]), tokensTree)
  const v = node?.$value
  return typeof v === 'string' && v.startsWith('{') ? token(v.slice(1, -1)) : v
}

const O = (k) => token(`object.${k}`)
export const HEADROOM = O('headroom')
export const VIEWBOX = O('viewbox')
export const GRAIN_TILE = O('grain-tile')

/** The appearance each switchable material is baked to when there are no variables. */
export const BAKED = { accent: 'accent', folder: token('object.folder-appearance') === 'manila' ? 'manila' : 'slate' }

const chan = (hex) => [1, 3, 5].map((i) => Number.parseInt(hex.slice(i, i + 2), 16))
const toHex = (rgb) =>
  '#' + rgb.map((v) => Math.max(0, Math.min(255, Math.round(v))).toString(16).padStart(2, '0')).join('')
export const shade = (hex, k) => toHex(chan(hex).map((v) => (k >= 0 ? v + (255 - v) * k : v * (1 + k))))
const mix = (a, b, t) => {
  const [x, y] = [chan(a), chan(b)]
  return toHex(x.map((v, i) => v + (y[i] - v) * t))
}

export function rampAt(material, t) {
  const m = BAKED[material] ?? material
  const hi = token(`object.material.${m}.hi`)
  const mid = token(`object.material.${m}.mid`)
  const lo = token(`object.material.${m}.lo`)
  if (t <= 0) return hi
  if (t < 0.5) return mix(hi, mid, t / 0.5)
  if (t <= 1) return mix(mid, lo, (t - 0.5) / 0.5)
  return shade(lo, -(t - 1))
}

export const FACET_RANGE = {
  top: [0.2, 0.72],
  flat: [0.12, 0.62],
  front: [0.5, 1.0],
  under: [0.88, 1.25],
}

export function facetStops(material, facet = 'flat') {
  const [from, to] = FACET_RANGE[facet]
  return [rampAt(material, from + HEADROOM), rampAt(material, to + HEADROOM)]
}

/**
 * How light travels ACROSS a face, as positions along the material ramp — the
 * twin of SHAPE_STOPS in src/components/ObjectIcon/recipe.ts. Without it a
 * ball, a barrel and a flat card all export with the same diagonal ramp.
 */
const LIGHT = (token('object.light-azimuth') * Math.PI) / 180
const SHAPE_STOPS = {
  sphere: [[0, 0.04], [0.42, 0.3], [0.74, 0.64], [0.92, 0.99], [1, 1.2]],
  dome: [[0, 0.1], [0.55, 0.38], [1, 0.86]],
  concave: [[0, 1.12], [0.45, 0.74], [1, 0.22]],
}

function cylinderStops(shape) {
  const t = 0.5 + (shape === 'cylY' ? Math.cos(LIGHT) : Math.sin(LIGHT)) * 0.28
  return [[0, 1.02], ...(t > 0.22 ? [[t - 0.2, 0.4]] : []), [t, 0.05], [t + (1 - t) * 0.45, 0.52], [1, 1.1]]
}

/** A matte body slides every bright stop down its ramp rather than peaking. */
export function shapeStops(shape, finish) {
  const lift = finish === 'matte' ? token('object.matte-form-lift') : 0
  const base = shape === 'cylX' || shape === 'cylY' ? cylinderStops(shape) : SHAPE_STOPS[shape]
  return base.map(([offset, t]) => [offset, t + lift * (t > 0.7 ? 0.5 : 1)])
}

/** The gradient a shape needs, in objectBoundingBox units. */
export function formGradient(shape) {
  const ux = Math.cos(LIGHT)
  const uy = Math.sin(LIGHT)
  const half = Math.SQRT2 / 2
  if (shape === 'sphere' || shape === 'dome')
    return { kind: 'radial', cx: 0.5 + ux * 0.17, cy: 0.5 + uy * 0.17, r: half * (shape === 'dome' ? 1.15 : 0.98) }
  if (shape === 'cylY') return { kind: 'linear', x1: 0, y1: 0, x2: 1, y2: 0 }
  if (shape === 'cylX') return { kind: 'linear', x1: 0, y1: 0, x2: 0, y2: 1 }
  return { kind: 'linear', x1: 0.5 + ux * 0.5, y1: 0.5 + uy * 0.5, x2: 0.5 - ux * 0.5, y2: 0.5 - uy * 0.5 }
}

/* A rim is a specular line; on a matte edge it runs at object.matte-rim. */
const rimFor = (finish) =>
  finish === 'matte'
    ? String(token('object.rim')).replace(/[\d.]+\)$/, `${token('object.matte-rim')})`)
    : token('object.rim')

export const MATERIALS = Object.keys(tokensTree.object.material).filter((k) => !k.startsWith('$'))
export const FACETS = Object.keys(FACET_RANGE)
export const objectNames = Object.keys(objects).filter((k) => !k.startsWith('$'))

/* ---- the SVG builders ----------------------------------------------------
 * Shared so the standalone .svg files and the Sketch contact sheet are the
 * same drawing; building the sheet from the emitted files instead would put a
 * build-order dependency between two scripts for no gain.
 */

const esc = (s) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
/** Sketch names a layer from its id, which must be an XML name — no spaces. */
const slug = (s) => s.replace(/[^A-Za-z0-9]+/g, '-').replace(/^-|-$/g, '')

export const GLYPH_STROKE = 1.7
export const GLYPH_VIEWBOX = 24

export function glyphSvg(name, paths) {
  const body = paths
    .map((d, i) => `    <path id="${slug(name)}-${i}" d="${d}"/>`)
    .join('\n')
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${GLYPH_VIEWBOX} ${GLYPH_VIEWBOX}" width="96" height="96">
  <title>${esc(name)}</title>
  <g id="${slug(name)}" fill="none" stroke="${token('ink.secondary')}" stroke-width="${GLYPH_STROKE}"
     stroke-linecap="round" stroke-linejoin="round">
${body}
  </g>
</svg>
`
}

export function objectSvg(name, def) {
  const defs = []
  const layers = []
  const material = (part) => part.material ?? def.material
  const sil = def.parts.find((p) => p.id === def.silhouette) ?? def.parts[0]
  const toneStep = token('object.tone-step')
  /* Referenced ids are scoped to the object: the Sketch sheet inlines every object into one page. */
  const ns = (id) => `${slug(name)}-${id}`

  for (const part of def.parts) {
    const role = part.role ?? 'body'
    if (role !== 'body') {
      const tintPath = part.tint ? token(part.tint) : null
      const label = part.tint ? `${part.id} · ${part.tint}` : `${part.id} · edge/hairline`
      layers.push(
        `    <g id="${slug(label)}"><title>${esc(label)}</title>` +
          (tintPath
            ? `<path d="${part.d}" fill="none" stroke="${tintPath}" stroke-width="1.4" stroke-linecap="round"/>`
            : `<path d="${part.d}" fill="none" stroke="${token('object.rim')}" stroke-width="1" stroke-linecap="round" transform="translate(0 0.85)"/>` +
              `<path d="${part.d}" fill="none" stroke="${token('object.keyline')}" stroke-width="1" stroke-linecap="round"/>`) +
          `</g>`,
      )
      continue
    }

    const m = material(part)
    const facet = part.facet ?? 'flat'
    const clip = ns(`clip-${slug(part.id)}`)
    defs.push(`    <clipPath id="${clip}"><path d="${part.d}"/></clipPath>`)

    let fill
    let label
    if (part.tint) {
      fill = token(part.tint)
      label = `${part.id} · ${part.tint}`
    } else {
      const gid = ns(`grad-${slug(part.id)}`)
      const shape = part.shape && part.shape !== 'flat' ? part.shape : null
      if (shape) {
        const g = formGradient(shape)
        const stops = shapeStops(shape, def.finish ?? 'lit')
          .map(([offset, t]) => `      <stop offset="${offset}" stop-color="${rampAt(m, t + HEADROOM)}"/>`)
          .join('\n')
        defs.push(
          g.kind === 'radial'
            ? `    <radialGradient id="${gid}" cx="${g.cx.toFixed(4)}" cy="${g.cy.toFixed(4)}" r="${g.r.toFixed(4)}">\n${stops}\n    </radialGradient>`
            : `    <linearGradient id="${gid}" x1="${g.x1.toFixed(4)}" y1="${g.y1.toFixed(4)}" x2="${g.x2.toFixed(4)}" y2="${g.y2.toFixed(4)}">\n${stops}\n    </linearGradient>`,
        )
        label = `${part.id} · ${m}/${shape}`
      } else {
        const [from, to] = facetStops(m, facet)
        defs.push(
          `    <linearGradient id="${gid}" x1="0" y1="0" x2="0.85" y2="1">\n` +
            `      <stop offset="0" stop-color="${from}"/>\n` +
            `      <stop offset="1" stop-color="${to}"/>\n` +
            `    </linearGradient>`,
        )
        label = `${part.id} · ${m}/${facet}`
      }
      fill = `url(#${gid})`
    }

    const inner = [`<path d="${part.d}" fill="${fill}"${part.fillRule ? ` fill-rule="${part.fillRule}"` : ''}/>`]
    if (part.tone)
      inner.push(
        `<path d="${part.d}" fill="${part.tone > 0 ? '#ffffff' : '#000000'}" opacity="${(Math.abs(part.tone) * toneStep).toFixed(3)}"/>`,
      )
    const bevel = part.bevel ?? 'none'
    if (bevel !== 'none') {
      const f = bevel === 'well' ? -1 : 1
      const d = 0.8 * f
      inner.push(`<path d="${part.d}" fill="none" stroke="${rimFor(def.finish ?? 'lit')}" stroke-width="2" transform="translate(${d} ${d})"/>`)
      inner.push(`<path d="${part.d}" fill="none" stroke="${token('object.occlusion')}" stroke-width="2" transform="translate(${-d} ${-d})"/>`)
    }
    layers.push(
      `    <g id="${slug(label)}" clip-path="url(#${clip})"><title>${esc(label)}</title>${inner.join('')}</g>`,
    )
  }

  /*
   * The broad key, over the silhouette. A matte finish takes one wide wash down
   * from the top instead of the specular diagonal, and its material's gloss is
   * capped rather than scaled — asking for matte is asking to override how
   * specular ruby or amber claims to be.
   */
  const matte = (def.finish ?? 'lit') === 'matte'
  const rawGloss = token(`object.material.${def.material}.gloss`) ?? 1
  const gloss = matte ? Math.min(rawGloss, token('object.matte-gloss-max')) : rawGloss
  const keyStops = matte
    ? [
        [0, token('object.matte-key-alpha')],
        [token('object.matte-key-mid-at'), token('object.matte-key-mid')],
        [1, 0],
      ]
    : [
        [0, token('object.key-alpha')],
        [token('object.key-mid-at'), token('object.key-mid')],
        [token('object.key-spread'), 0],
      ]
  defs.push(
    `    <clipPath id="${ns('clip-silhouette')}"><path d="${sil.d}"/></clipPath>`,
    `    <linearGradient id="${ns('grad-key')}" x1="0" y1="0" x2="${matte ? '0.35' : '0.9'}" y2="1">\n` +
      keyStops
        .map(([o, a]) => `      <stop offset="${o}" stop-color="${token('sheen.color')}" stop-opacity="${a}"/>`)
        .join('\n') +
      `\n    </linearGradient>`,
  )
  layers.push(
    `    <g id="key-sheen-color" clip-path="url(#${ns('clip-silhouette')})" opacity="${gloss}" style="mix-blend-mode:screen">` +
      `<title>key · sheen/color</title><path d="${sil.d}" fill="url(#${ns('grad-key')})"/></g>`,
  )

  if (def.finish === 'glossy') {
    defs.push(
      `    <linearGradient id="${ns('grad-gloss')}" x1="0" y1="0" x2="0" y2="1">\n` +
        [
          [0, token('object.gloss-alpha')],
          [token('object.gloss-break'), token('object.gloss-shoulder')],
          [token('object.gloss-break') + 0.001, 0],
          [0.82, 0],
          [1, token('object.gloss-bounce')],
        ]
          .map(([o, a]) => `      <stop offset="${o}" stop-color="${token('sheen.color')}" stop-opacity="${a}"/>`)
          .join('\n') +
        `\n    </linearGradient>`,
    )
    layers.push(
      `    <g id="gloss-sheen-color" clip-path="url(#${ns('clip-silhouette')})" style="mix-blend-mode:screen">` +
        `<title>gloss · sheen/color</title><path d="${sil.d}" fill="url(#${ns('grad-gloss')})"/></g>`,
    )
  }

  layers.push(
    `    <path id="keyline-edge-hairline" d="${sil.d}" fill="none" stroke="${token('object.keyline')}" stroke-width="1"/>`,
  )

  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${VIEWBOX} ${VIEWBOX}" width="128" height="128">
  <title>${esc(name)}</title>
  <defs>
${defs.join('\n')}
  </defs>
  <g id="${slug(name)}">
${layers.join('\n')}
  </g>
</svg>
`
}

