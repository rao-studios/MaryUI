/**
 * Wallpaper — molten platinum, rendered once. An SVG lighting filter over
 * fractal noise is drawn into a canvas at a capped resolution after first
 * paint; a base gradient shows until then. Drift is a CSS-only highlight layer
 * so the filter never re-runs. Drop a raster into public/wallpaper/ and choose
 * View › Wallpaper › Raster to use it instead.
 */

import { useEffect, useRef, useState } from 'react'
import { useSettings } from './settings'
import styles from './Wallpaper.module.css'

const SVG_W = 1600
const SVG_H = 1000

function wallpaperSvg(): string {
  // Soft diagonal bands of platinum, displaced by low-frequency noise into
  // drapery folds, then kissed by a gentle specular pass. Displacement (not
  // lighting) makes the folds, so there is no contour banding.
  const stops = [
    [0, '#dfe1e6'], [0.1, '#6d727c'], [0.2, '#d3d6dc'], [0.29, '#4a4e57'], [0.4, '#c4c8cf'],
    [0.51, '#5f636c'], [0.6, '#e6e8ec'], [0.7, '#3f434b'], [0.81, '#b8bcc4'], [0.9, '#5a5e67'], [1, '#d5d8de'],
  ]
    .map(([o, c]) => `<stop offset="${o}" stop-color="${c}"/>`)
    .join('')
  return (
    `<svg xmlns="http://www.w3.org/2000/svg" width="${SVG_W}" height="${SVG_H}" viewBox="0 0 ${SVG_W} ${SVG_H}">` +
    `<defs>` +
    `<linearGradient id="folds" x1="0" y1="0" x2="1" y2="1">${stops}</linearGradient>` +
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
    `<rect x="${-SVG_W * 0.2}" y="${-SVG_H * 0.2}" width="${SVG_W * 1.4}" height="${SVG_H * 1.4}" fill="url(#folds)" filter="url(#liquid)"/>` +
    `</svg>`
  )
}

function loadSvg(svg: string): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const url = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }))
    const img = new Image()
    img.onload = () => {
      URL.revokeObjectURL(url)
      resolve(img)
    }
    img.onerror = () => {
      URL.revokeObjectURL(url)
      reject(new Error('wallpaper render failed'))
    }
    img.src = url
  })
}

export function Wallpaper() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const settings = useSettings()
  const [rasterFailed, setRasterFailed] = useState(false)
  const [ready, setReady] = useState(false)

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    let cancelled = false
    let timer: ReturnType<typeof setTimeout> | undefined
    const image = loadSvg(wallpaperSvg())

    const paint = async () => {
      const img = await image
      if (cancelled) return
      const dpr = Math.min(window.devicePixelRatio || 1, 1.5)
      const w = Math.ceil(window.innerWidth * dpr)
      const h = Math.ceil(window.innerHeight * dpr)
      canvas.width = w
      canvas.height = h
      const ctx = canvas.getContext('2d')
      if (!ctx) return
      // Cover: scale so the SVG fills the canvas, then center the overflow.
      const scale = Math.max(w / SVG_W, h / SVG_H)
      const dw = SVG_W * scale
      const dh = SVG_H * scale
      ctx.drawImage(img, (w - dw) / 2, (h - dh) / 2, dw, dh)
      setReady(true)
    }

    const schedule = () => {
      clearTimeout(timer)
      timer = setTimeout(() => void paint().catch(() => setReady(false)), 400)
    }

    void paint().catch(() => setReady(false))
    window.addEventListener('resize', schedule)
    return () => {
      cancelled = true
      clearTimeout(timer)
      window.removeEventListener('resize', schedule)
    }
  }, [])

  const raster = settings.wallpaper === 'raster' && !rasterFailed

  return (
    <div className={styles.wallpaper} aria-hidden="true">
      <canvas ref={canvasRef} className={styles.canvas} data-ready={ready} />
      {raster ? (
        <img className={styles.raster} src="./wallpaper/platinum.jpg" alt="" onError={() => setRasterFailed(true)} />
      ) : null}
      <div className={styles.drift} />
      <div className={styles.vignette} />
    </div>
  )
}
