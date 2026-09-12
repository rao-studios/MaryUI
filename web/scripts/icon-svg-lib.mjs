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
    const clip = `clip-${slug(part.id)}`
    defs.push(`    <clipPath id="${clip}"><path d="${part.d}"/></clipPath>`)

    let fill
    let label
    if (part.tint) {
      fill = token(part.tint)
      label = `${part.id} · ${part.tint}`
    } else {
      const [from, to] = facetStops(m, facet)
      const gid = `grad-${slug(part.id)}`
      defs.push(
        `    <linearGradient id="${gid}" x1="0" y1="0" x2="0.85" y2="1">\n` +
          `      <stop offset="0" stop-color="${from}"/>\n` +
          `      <stop offset="1" stop-color="${to}"/>\n` +
          `    </linearGradient>`,
      )
      fill = `url(#${gid})`
      label = `${part.id} · ${m}/${facet}`
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
      inner.push(`<path d="${part.d}" fill="none" stroke="${token('object.rim')}" stroke-width="2" transform="translate(${d} ${d})"/>`)
      inner.push(`<path d="${part.d}" fill="none" stroke="${token('object.occlusion')}" stroke-width="2" transform="translate(${-d} ${-d})"/>`)
    }
    layers.push(
      `    <g id="${slug(label)}" clip-path="url(#${clip})"><title>${esc(label)}</title>${inner.join('')}</g>`,
    )
  }

  /* The broad key, over the silhouette, scaled by how specular the material is. */
  const gloss = token(`object.material.${def.material}.gloss`) ?? 1
  defs.push(
    `    <clipPath id="clip-silhouette"><path d="${sil.d}"/></clipPath>`,
    `    <linearGradient id="grad-key" x1="0" y1="0" x2="0.9" y2="1">\n` +
      [
        [0, token('object.key-alpha')],
        [token('object.key-mid-at'), token('object.key-mid')],
        [token('object.key-spread'), 0],
      ]
        .map(([o, a]) => `      <stop offset="${o}" stop-color="${token('sheen.color')}" stop-opacity="${a}"/>`)
        .join('\n') +
      `\n    </linearGradient>`,
  )
  layers.push(
    `    <g id="key-sheen-color" clip-path="url(#clip-silhouette)" opacity="${gloss}" style="mix-blend-mode:screen">` +
      `<title>key · sheen/color</title><path d="${sil.d}" fill="url(#grad-key)"/></g>`,
  )

  if (def.finish === 'glossy') {
    defs.push(
      `    <linearGradient id="grad-gloss" x1="0" y1="0" x2="0" y2="1">\n` +
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
      `    <g id="gloss-sheen-color" clip-path="url(#clip-silhouette)" style="mix-blend-mode:screen">` +
        `<title>gloss · sheen/color</title><path d="${sil.d}" fill="url(#grad-gloss)"/></g>`,
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

