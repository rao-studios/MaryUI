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
import { brushedTextureSvg, type BrushParams } from './brushSvg'

export type { BrushParams }

export function defaultBrushParams(): BrushParams {
  return {
    freqX: tokens.brush.freqX,
    freqY: tokens.brush.freqY,
    octaves: tokens.brush.octaves,
    seed: tokens.brush.seed,
    tile: parseFloat(tokens.brush.tile),
    rise: tokens.brush.angle.rise,
    run: tokens.brush.angle.run,
    contrast: tokens.brush.contrast,
  }
}

export { brushedTextureSvg }

export function brushedTextureDataUri(p: BrushParams = defaultBrushParams()): string {
  return `data:image/svg+xml;utf8,${encodeURIComponent(brushedTextureSvg(p))}`
}

let defaultBrushUri: string | null = null

/**
 * The tile at its token defaults, built once. Several consumers want the same
 * bytes — every object icon's grain, the platinum monogram — and each used to
 * keep a cache of its own, so the SVG was serialised more than once.
 */
export function sharedBrushDataUri(): string {
  defaultBrushUri ??= brushedTextureDataUri()
  return defaultBrushUri
}

/** Publishes the tile as `--lp-brush-url` on the root so every surface can paint it. */
export function installBrushTexture(p: BrushParams = defaultBrushParams(), root: HTMLElement = document.documentElement): void {
  root.style.setProperty('--lp-brush-url', `url("${brushedTextureDataUri(p)}")`)
  root.style.setProperty('--lp-brush-tile', `${p.tile}px`)
}
