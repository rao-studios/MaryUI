/**
 * The brushed-platinum grain, baked once into a tileable SVG data URI and
 * published as `--lp-brush-url`. Surfaces paint it with `mix-blend-mode:
 * overlay`; because the noise is compressed around mid-gray it is neutral
 * under overlay and only adds directional grain, never a color shift.
 *
 * Anisotropic turbulence does the brushing: a low x-frequency gives long
 * horizontal streaks while a high y-frequency keeps the grain fine.
 */

import { tokens } from '@/tokens/tokens'

export interface BrushParams {
  freqX: number
  freqY: number
  octaves: number
  seed: number
  /** Tile edge in px. */
  tile: number
}

export function defaultBrushParams(): BrushParams {
  return {
    freqX: tokens.brush.freqX,
    freqY: tokens.brush.freqY,
    octaves: tokens.brush.octaves,
    seed: tokens.brush.seed,
    tile: parseFloat(tokens.brush.tile),
  }
}

export function brushedTextureSvg(p: BrushParams = defaultBrushParams()): string {
  return (
    `<svg xmlns="http://www.w3.org/2000/svg" width="${p.tile}" height="${p.tile}">` +
    `<filter id="b" x="0" y="0" width="100%" height="100%" color-interpolation-filters="sRGB">` +
    `<feTurbulence type="fractalNoise" baseFrequency="${p.freqX} ${p.freqY}" numOctaves="${p.octaves}" seed="${p.seed}" stitchTiles="stitch"/>` +
    `<feColorMatrix type="saturate" values="0"/>` +
    `<feComponentTransfer>` +
    `<feFuncR type="linear" slope="0.55" intercept="0.22"/>` +
    `<feFuncG type="linear" slope="0.55" intercept="0.22"/>` +
    `<feFuncB type="linear" slope="0.55" intercept="0.22"/>` +
    `<feFuncA type="linear" slope="0" intercept="1"/>` +
    `</feComponentTransfer>` +
    `</filter>` +
    `<rect width="100%" height="100%" filter="url(#b)"/>` +
    `</svg>`
  )
}

export function brushedTextureDataUri(p: BrushParams = defaultBrushParams()): string {
  return `data:image/svg+xml;utf8,${encodeURIComponent(brushedTextureSvg(p))}`
}

/** Publishes the tile as `--lp-brush-url` on the root so every surface can paint it. */
export function installBrushTexture(p: BrushParams = defaultBrushParams(), root: HTMLElement = document.documentElement): void {
  root.style.setProperty('--lp-brush-url', `url("${brushedTextureDataUri(p)}")`)
  root.style.setProperty('--lp-brush-tile', `${p.tile}px`)
}
