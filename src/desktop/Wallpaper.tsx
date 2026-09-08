/**
 * Wallpaper — molten platinum, rendered once. An SVG lighting filter over
 * fractal noise is drawn into a canvas at a capped resolution after first
 * paint; a base gradient shows until then. Drift is a CSS-only highlight layer
 * so the filter never re-runs. Drop a raster into public/wallpaper/ and choose
 * View › Wallpaper › Raster to use it instead.
 */

import { useEffect, useRef, useState } from 'react'
import { WALLPAPER_H, WALLPAPER_W, wallpaperSvg } from '@/lib/wallpaperSvg'
import { useSettings } from './settings'
import styles from './Wallpaper.module.css'

const SVG_W = WALLPAPER_W
const SVG_H = WALLPAPER_H

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
