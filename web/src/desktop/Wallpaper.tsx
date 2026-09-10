/**
 * Wallpaper — molten platinum.
 *
 * `molten` is a live shader: a domain-warped height field lit by two lights
 * (`lib/moltenShader.ts`). **Its clock is window motion, not wall-clock time.**
 * It registers with the motion engine as a target that draws when stepped but
 * never asks for another frame of its own, so the metal flows while a window is
 * being dragged and freezes the instant everything settles — an idle desktop
 * still schedules no frames at all, which is the rule the whole system is built
 * on. It also makes that rule into the idea: the desk is one sheet of liquid
 * metal, and it only moves when you move something on it.
 *
 * `procedural` is the older still: an SVG lighting filter over fractal noise,
 * drawn into a canvas once. It is also the fallback when WebGL is unavailable.
 * `raster` uses public/wallpaper/platinum.jpg. Drift and vignette are CSS over
 * the top of whichever is showing.
 */

import { useEffect, useRef, useState } from 'react'
import { WALLPAPER_H, WALLPAPER_W, wallpaperSvg } from '@/lib/wallpaperSvg'
import { createMoltenRenderer, type MoltenRenderer } from '@/lib/moltenRenderer'
import { hexToRgb, type MoltenGrade } from '@/lib/moltenShader'
import { engine, type MotionTarget } from '@/lib/motionEngine'
import { prefersReducedMotion } from '@/lib/dom'
import { tokens } from '@/tokens/tokens'
import { useSettings, type MoltenTone } from './settings'
import styles from './Wallpaper.module.css'

const GRADES: Record<MoltenTone, MoltenGrade> = {
  platinum: {
    base: hexToRgb(tokens.molten.platinum.base),
    lift: tokens.molten.platinum.lift,
    gain: tokens.molten.platinum.gain,
    saturation: tokens.molten.platinum.saturation,
  },
  faithful: {
    base: hexToRgb(tokens.molten.faithful.base),
    lift: tokens.molten.faithful.lift,
    gain: tokens.molten.faithful.gain,
    saturation: tokens.molten.faithful.saturation,
  },
}

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
  const glRef = useRef<HTMLCanvasElement>(null)
  const settings = useSettings()
  const [rasterFailed, setRasterFailed] = useState(false)
  const [ready, setReady] = useState(false)
  const [glReady, setGlReady] = useState(false)
  const toneRef = useRef<MoltenTone>(settings.moltenTone)
  toneRef.current = settings.moltenTone

  const molten = settings.wallpaper === 'molten'

  /*
   * The shader's clock. It only advances on frames the engine was already
   * running, so `step` returns false: this target never keeps the loop alive,
   * it just paints whenever something else has.
   */
  useEffect(() => {
    const canvas = glRef.current
    if (!canvas || !molten) return
    let renderer: MoltenRenderer | null = createMoltenRenderer(canvas)
    if (!renderer) {
      setGlReady(false)
      return
    }

    let time = 0

    /*
     * Resolution follows what the desktop is doing. While something is moving
     * the shader draws below native to leave the GPU to the interface; once
     * everything settles it redraws once at native device resolution. The
     * wallpaper you sit and look at is therefore never upscaled — upscaling a
     * CSS-resolution buffer onto a retina display is what made this read as
     * grainy.
     */
    type Quality = 'motion' | 'still'
    let quality: Quality | null = null

    const scaleFor = (q: Quality) => {
      const dpr = window.devicePixelRatio || 1
      const wanted = (q === 'still' ? tokens.molten.scaleStill : tokens.molten.scale) * dpr
      const area = Math.max(window.innerWidth * window.innerHeight, 1)
      const ceiling = Math.sqrt(tokens.molten.maxPixels / area)
      return Math.min(wanted, ceiling)
    }

    const paint = () => renderer!.draw(time, tokens.molten.zoom, GRADES[toneRef.current])

    const setQuality = (q: Quality) => {
      const changed = renderer!.resize(window.innerWidth, window.innerHeight, scaleFor(q)) || quality !== q
      quality = q
      return changed
    }

    setQuality('still')
    paint()
    setGlReady(true)

    let settle: ReturnType<typeof setTimeout> | undefined
    const settleMs = parseFloat(tokens.molten.settleMs)

    const target: MotionTarget = {
      step(dt) {
        if (!renderer || renderer.lost) return false
        if (prefersReducedMotion()) return false
        time += dt * tokens.molten.flow
        setQuality('motion')
        paint()
        clearTimeout(settle)
        settle = setTimeout(() => {
          if (!renderer || renderer.lost) return
          setQuality('still')
          paint()
        }, settleMs)
        return false // never asks for a frame of its own
      },
    }
    engine.add(target)

    let timer: ReturnType<typeof setTimeout> | undefined
    const onResize = () => {
      clearTimeout(timer)
      timer = setTimeout(() => {
        quality = null // force the buffer to be re-sized for the new viewport
        setQuality('still')
        paint()
      }, 200)
    }
    window.addEventListener('resize', onResize)

    // A repaint after a tone change, since nothing may be moving.
    const repaint = () => paint()
    window.addEventListener('lp-molten-repaint', repaint)

    return () => {
      window.removeEventListener('resize', onResize)
      window.removeEventListener('lp-molten-repaint', repaint)
      clearTimeout(timer)
      clearTimeout(settle)
      engine.remove(target)
      renderer?.dispose()
      renderer = null
      setGlReady(false)
    }
  }, [molten])

  useEffect(() => {
    if (molten) window.dispatchEvent(new Event('lp-molten-repaint'))
  }, [settings.moltenTone, molten])

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
  // The procedural still is also the fallback when WebGL will not start.
  const showCanvas = ready && (!molten || !glReady)

  return (
    <div className={styles.wallpaper} aria-hidden="true">
      <canvas ref={canvasRef} className={styles.canvas} data-ready={showCanvas} />
      <canvas ref={glRef} className={styles.gl} data-ready={molten && glReady} />
      {raster ? (
        <img className={styles.raster} src="./wallpaper/platinum.jpg" alt="" onError={() => setRasterFailed(true)} />
      ) : null}
      <div className={styles.drift} />
      <div className={styles.vignette} />
    </div>
  )
}
