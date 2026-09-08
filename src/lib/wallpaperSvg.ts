/**
 * The molten-platinum wallpaper as an SVG string. Import-free so the Sketch
 * builder can render the same drapery the desktop shows.
 *
 * Soft diagonal bands of platinum are displaced by low-frequency noise into
 * folds, then kissed by a gentle specular pass. Displacement (not lighting)
 * makes the folds, so there is no contour banding.
 */

export const WALLPAPER_W = 1600
export const WALLPAPER_H = 1000

export function wallpaperSvg(w = WALLPAPER_W, h = WALLPAPER_H): string {
  const stops: Array<[number, string]> = [
    [0, '#dfe1e6'], [0.1, '#6d727c'], [0.2, '#d3d6dc'], [0.29, '#4a4e57'], [0.4, '#c4c8cf'],
    [0.51, '#5f636c'], [0.6, '#e6e8ec'], [0.7, '#3f434b'], [0.81, '#b8bcc4'], [0.9, '#5a5e67'], [1, '#d5d8de'],
  ]
  const gradient = stops.map(([o, c]) => `<stop offset="${o}" stop-color="${c}"/>`).join('')
  return (
    `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">` +
    `<defs>` +
    `<linearGradient id="folds" x1="0" y1="0" x2="1" y2="1">${gradient}</linearGradient>` +
    `<filter id="liquid" x="-20%" y="-20%" width="140%" height="140%" color-interpolation-filters="sRGB">` +
    `<feTurbulence type="fractalNoise" baseFrequency="0.0016 0.0026" numOctaves="3" seed="11" result="noise"/>` +
    `<feDisplacementMap in="SourceGraphic" in2="noise" scale="420" xChannelSelector="R" yChannelSelector="G" result="folds"/>` +
    `<feGaussianBlur in="folds" stdDeviation="1.5" result="soft"/>` +
    `<feTurbulence type="fractalNoise" baseFrequency="0.004 0.006" numOctaves="2" seed="4" result="n2"/>` +
    `<feGaussianBlur in="n2" stdDeviation="3" result="n2s"/>` +
    `<feSpecularLighting in="n2s" lighting-color="#ffffff" surfaceScale="3" specularConstant="0.7" specularExponent="26" result="spec">` +
    `<feDistantLight azimuth="230" elevation="50"/>` +
    `</feSpecularLighting>` +
    `<feComposite in="spec" in2="soft" operator="arithmetic" k1="0" k2="0.35" k3="1" k4="0" result="out"/>` +
    `<feComponentTransfer in="out"><feFuncA type="linear" slope="0" intercept="1"/></feComponentTransfer>` +
    `</filter>` +
    `</defs>` +
    `<rect x="${-w * 0.2}" y="${-h * 0.2}" width="${w * 1.4}" height="${h * 1.4}" fill="url(#folds)" filter="url(#liquid)"/>` +
    `</svg>`
  )
}
