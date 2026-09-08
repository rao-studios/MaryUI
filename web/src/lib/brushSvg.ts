/**
 * The brushed-platinum tile as an SVG string. Import-free on purpose: the web
 * app (`textures.ts`) and the Sketch document builder (`scripts/build-sketch.mjs`)
 * both render exactly this, so the metal matches across the two.
 *
 * Anisotropic turbulence does the brushing: a low x-frequency gives long
 * streaks while a high y-frequency keeps the grain fine. The streaks run
 * diagonally, at atan(rise/run). Rotated noise normally cannot tile, so the
 * noise period is chosen on the rotated lattice: with tan θ = rise/run and a
 * square tile of side S, a noise period of S / sqrt(rise² + run²) makes the
 * lattice contain (S, 0) and (0, S), and the raster tile repeats seamlessly.
 */

export interface BrushParams {
  /** Low frequency along the stroke (cycles/px). */
  freqX: number
  /** High frequency across the stroke (cycles/px). */
  freqY: number
  octaves: number
  seed: number
  /** Raster tile edge in px. */
  tile: number
  /** Stroke direction as a lattice slope: tan θ = rise / run (small integers). */
  rise: number
  run: number
  /** Grain contrast 0..1 (slope of the mid-gray compression). */
  contrast: number
}

export function brushAngleDegrees(p: Pick<BrushParams, 'rise' | 'run'>): number {
  return (Math.atan2(p.rise, p.run) * 180) / Math.PI
}

export function brushedTextureSvg(p: BrushParams): string {
  const period = p.tile / Math.sqrt(p.rise * p.rise + p.run * p.run)
  const angle = -brushAngleDegrees(p)
  const slope = p.contrast
  const intercept = (1 - slope) / 2
  return (
    `<svg xmlns="http://www.w3.org/2000/svg" width="${p.tile}" height="${p.tile}">` +
    `<defs>` +
    `<filter id="b" x="0" y="0" width="100%" height="100%" color-interpolation-filters="sRGB">` +
    `<feTurbulence type="fractalNoise" baseFrequency="${p.freqX} ${p.freqY}" numOctaves="${p.octaves}" seed="${p.seed}" stitchTiles="stitch"/>` +
    `<feColorMatrix type="saturate" values="0"/>` +
    `<feComponentTransfer>` +
    `<feFuncR type="linear" slope="${slope}" intercept="${intercept}"/>` +
    `<feFuncG type="linear" slope="${slope}" intercept="${intercept}"/>` +
    `<feFuncB type="linear" slope="${slope}" intercept="${intercept}"/>` +
    `<feFuncA type="linear" slope="0" intercept="1"/>` +
    `</feComponentTransfer>` +
    `</filter>` +
    `<pattern id="p" patternUnits="userSpaceOnUse" width="${period}" height="${period}" patternTransform="rotate(${angle})">` +
    `<rect width="${period}" height="${period}" filter="url(#b)"/>` +
    `</pattern>` +
    `</defs>` +
    `<rect width="${p.tile}" height="${p.tile}" fill="#808080"/>` +
    `<rect width="${p.tile}" height="${p.tile}" fill="url(#p)"/>` +
    `</svg>`
  )
}
