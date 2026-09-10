/**
 * Live tuning of the motion and brush constants. Values are written into the
 * engine's mutable `motionParams` (and the brush onto the root), so every
 * window responds at once. "Copy JSON patch" hands back a tokens.json snippet.
 */

import { useCallback, useState } from 'react'
import { Button } from '@/components/Button'
import { Slider } from '@/components/Slider'
import { motionParams, resetMotionParams } from '@/lib/motionEngine'
import { defaultBrushParams, installBrushTexture, type BrushParams } from '@/lib/textures'
import { tokens } from '@/tokens/tokens'
import styles from '../GalleryApp.module.css'

interface Knob {
  label: string
  min: number
  max: number
  step: number
  get(): number
  set(v: number): void
  path: string
}

function brushOpacity(): number {
  const v = getComputedStyle(document.documentElement).getPropertyValue('--lp-brush-opacity')
  return v ? parseFloat(v) : tokens.brush.opacity
}

/** A knob over a CSS custom property, for the values only stylesheets read. */
function rootVar(name: string, fallback: number): Pick<Knob, 'get' | 'set'> {
  return {
    get() {
      const v = getComputedStyle(document.documentElement).getPropertyValue(name)
      return v ? parseFloat(v) : fallback
    },
    set(v) {
      document.documentElement.style.setProperty(name, String(v))
    },
  }
}

export function MotionTab() {
  const [, bump] = useState(0)
  const [brush, setBrush] = useState<BrushParams>(defaultBrushParams)
  const [copied, setCopied] = useState(false)
  const rerender = useCallback(() => bump((n) => n + 1), [])

  const applyBrush = (next: BrushParams) => {
    setBrush(next)
    installBrushTexture(next)
  }

  const p = motionParams
  const knobs: Knob[] = [
    { label: 'Sheen frequency (Hz)', min: 0.3, max: 4, step: 0.1, get: () => p.sheen.frequency, set: (v) => (p.sheen.frequency = v), path: 'motion.spring-sheen.frequency' },
    { label: 'Sheen damping', min: 0.05, max: 1.2, step: 0.05, get: () => p.sheen.damping, set: (v) => (p.sheen.damping = v), path: 'motion.spring-sheen.damping' },
    { label: 'Light x (viewport)', min: 0, max: 1, step: 0.05, get: () => p.lightX, set: (v) => (p.lightX = v), path: 'sheen.light-x' },
    { label: 'Jelly frequency (Hz)', min: 0.5, max: 5, step: 0.1, get: () => p.jelly.frequency, set: (v) => (p.jelly.frequency = v), path: 'motion.spring-jelly.frequency' },
    { label: 'Jelly damping', min: 0.05, max: 1.2, step: 0.05, get: () => p.jelly.damping, set: (v) => (p.jelly.damping = v), path: 'motion.spring-jelly.damping' },
    { label: 'Jelly max scale', min: 0, max: 0.12, step: 0.005, get: () => p.jellyMaxScale, set: (v) => (p.jellyMaxScale = v), path: 'motion.jelly-max-scale' },
    { label: 'Jelly max skew (°)', min: 0, max: 8, step: 0.25, get: () => p.jellyMaxSkew, set: (v) => (p.jellyMaxSkew = v), path: 'motion.jelly-max-skew' },
    { label: 'Tilt max (°)', min: 0, max: 10, step: 0.5, get: () => p.tiltMax, set: (v) => (p.tiltMax = v), path: 'motion.tilt-max' },
    { label: 'Velocity reference (px/s)', min: 500, max: 6000, step: 100, get: () => p.velocityRef, set: (v) => (p.velocityRef = v), path: 'motion.velocity-ref' },
    { label: 'Slosh frequency (Hz)', min: 0.4, max: 4, step: 0.1, get: () => p.slosh.frequencyHz, set: (v) => (p.slosh.frequencyHz = v), path: 'motion.slosh-frequency' },
    { label: 'Slosh damping', min: 0.02, max: 1, step: 0.02, get: () => p.slosh.dampingRatio, set: (v) => (p.slosh.dampingRatio = v), path: 'motion.slosh-damping' },
    { label: 'Slosh gain (×1e-5)', min: 0, max: 20, step: 0.5, get: () => p.slosh.gain * 1e5, set: (v) => (p.slosh.gain = v / 1e5), path: 'motion.slosh-gain' },
    { label: 'Slosh max angle (rad)', min: 0.1, max: 1.2, step: 0.05, get: () => p.slosh.maxAngle, set: (v) => (p.slosh.maxAngle = v), path: 'motion.slosh-max' },
  ]

  const cornerKnobs: Knob[] = [
    { label: 'Corner rest (px)', min: 4, max: 20, step: 1, get: () => p.corners.rest, set: (v) => (p.corners.rest = v), path: 'radius.window' },
    { label: 'Corner min (px)', min: 0, max: 16, step: 1, get: () => p.corners.min, set: (v) => (p.corners.min = v), path: 'radius.window-min' },
    { label: 'Corner max (px)', min: 4, max: 28, step: 1, get: () => p.corners.max, set: (v) => (p.corners.max = v), path: 'radius.window-max' },
    { label: 'Corner spread (px)', min: 0, max: 12, step: 0.5, get: () => p.corners.spread, set: (v) => (p.corners.spread = v), path: 'radius-flex.spread' },
    { label: 'Corner frequency (Hz)', min: 0.5, max: 6, step: 0.1, get: () => p.radius.frequency, set: (v) => (p.radius.frequency = v), path: 'motion.spring-radius.frequency' },
    { label: 'Corner damping', min: 0.1, max: 1.2, step: 0.02, get: () => p.radius.damping, set: (v) => (p.radius.damping = v), path: 'motion.spring-radius.damping' },
    { label: 'Corner detune', min: 0, max: 0.3, step: 0.01, get: () => p.radiusDetune, set: (v) => (p.radiusDetune = v), path: 'motion.radius-detune' },
    { label: 'Grain lag (px)', min: 0, max: 40, step: 1, get: () => p.grainLag, set: (v) => (p.grainLag = v), path: 'brush.lag' },
    { label: 'Grain follow (ms)', min: 0, max: 400, step: 10, get: () => p.grainFollow, set: (v) => (p.grainFollow = v), path: 'motion.grain-follow' },
    { label: 'Smear lag frequency (Hz)', min: 0.2, max: 4, step: 0.1, get: () => p.vxLag.frequency, set: (v) => (p.vxLag.frequency = v), path: 'motion.spring-vx-lag.frequency' },
  ]

  /* These are read straight out of CSS, since only stylesheets consume them. */
  const rootKnobs: Knob[] = [
    { label: 'Merge stretch', min: 0, max: 1.6, step: 0.05, ...rootVar('--lp-goo-stretch', tokens.goo.stretch), path: 'goo.stretch' },
    { label: 'Merge attract (px)', min: 0, max: 10, step: 0.5, ...rootVar('--lp-goo-attract', tokens.goo.attract), path: 'goo.attract' },
    { label: 'Slosh scale (bubbles)', min: 0.5, max: 4, step: 0.05, ...rootVar('--lp-liquid-slosh-scale', tokens.liquid.sloshScale), path: 'liquid.slosh-scale' },
    { label: 'Brush glint', min: 0, max: 2, step: 0.05, ...rootVar('--lp-brush-glint', tokens.brush.glint), path: 'brush.glint' },
  ]

  const brushKnobs: Knob[] = [
    { label: 'Brush opacity', min: 0, max: 0.8, step: 0.02, get: brushOpacity, set: (v) => document.documentElement.style.setProperty('--lp-brush-opacity', String(v)), path: 'brush.opacity' },
    { label: 'Brush frequency x', min: 0.002, max: 0.08, step: 0.002, get: () => brush.freqX, set: (v) => applyBrush({ ...brush, freqX: v }), path: 'brush.freq-x' },
    { label: 'Brush frequency y', min: 0.05, max: 1.2, step: 0.05, get: () => brush.freqY, set: (v) => applyBrush({ ...brush, freqY: v }), path: 'brush.freq-y' },
    { label: 'Brush octaves', min: 1, max: 5, step: 1, get: () => brush.octaves, set: (v) => applyBrush({ ...brush, octaves: v }), path: 'brush.octaves' },
    { label: 'Brush contrast', min: 0.1, max: 1, step: 0.02, get: () => brush.contrast, set: (v) => applyBrush({ ...brush, contrast: v }), path: 'brush.contrast' },
    { label: 'Brush angle rise (run 2)', min: 0, max: 3, step: 1, get: () => brush.rise, set: (v) => applyBrush({ ...brush, rise: v }), path: 'brush.angle.rise' },
  ]

  const patch = () => {
    const out: Record<string, number> = {}
    for (const k of [...knobs, ...cornerKnobs, ...rootKnobs, ...brushKnobs]) {
      const v = k.get()
      out[k.path] = k.path === 'motion.slosh-gain' ? v / 1e5 : v
    }
    return JSON.stringify(out, null, 2)
  }

  const copyPatch = () => {
    void navigator.clipboard?.writeText(patch()).catch(() => {})
    setCopied(true)
    setTimeout(() => setCopied(false), 1200)
  }

  const reset = () => {
    resetMotionParams()
    for (const name of ['--lp-brush-opacity', '--lp-goo-stretch', '--lp-goo-attract', '--lp-liquid-slosh-scale', '--lp-brush-glint']) {
      document.documentElement.style.removeProperty(name)
    }
    applyBrush(defaultBrushParams())
    rerender()
  }

  const render = (k: Knob) => (
    <Slider
      key={k.path}
      label={k.label}
      min={k.min}
      max={k.max}
      step={k.step}
      value={k.get()}
      onChange={(v) => {
        k.set(v)
        rerender()
      }}
      showValue
      format={(v) => (Number.isInteger(k.step) ? String(v) : v.toFixed(k.step < 0.01 ? 3 : 2))}
    />
  )

  return (
    <>
      <section className={styles.section}>
        <h2 className={styles.heading}>Motion</h2>
        <p className={styles.note}>
          Drag any window while you move these. Nothing here re-renders React; the engine reads the values each frame.
          The liquid answers to two things: acceleration, which only exists at the start and end of a gesture, and{' '}
          <em>shear</em> — how far the liquid&apos;s own speed lags the well&apos;s — which is what keeps it leaning all
          the way through a drag.
        </p>
        <div className={styles.motionGrid}>{knobs.map(render)}</div>
      </section>
      <section className={styles.section}>
        <h2 className={styles.heading}>Corners and grain</h2>
        <p className={styles.note}>
          Each corner has its own spring, detuned off the others, so a moving window&apos;s leading corners flatten while
          its trailing corners round — and they never settle in step. The velocity reference is deliberately far below
          the jelly&apos;s: a careful drag peaks near 400 px/s, and the corners should be well off rest by then. The
          impulse is kicked in on grab and on release. The grain is the metal skin lagging behind the frame.
        </p>
        <div className={styles.motionGrid}>{cornerKnobs.map(render)}</div>
      </section>
      <section className={styles.section}>
        <h2 className={styles.heading}>Liquid merge</h2>
        <p className={styles.note}>
          Stretch is what closes the gaps between drops; the filter bridges the rest. Blur and threshold live in the SVG
          filter definitions and need a reload to change.
        </p>
        <div className={styles.motionGrid}>{rootKnobs.map(render)}</div>
      </section>
      <section className={styles.section}>
        <h2 className={styles.heading}>Brush</h2>
        <div className={styles.motionGrid}>{brushKnobs.map(render)}</div>
      </section>
      <section className={styles.section}>
        <div className={styles.row}>
          <Button variant="primary" onClick={copyPatch}>
            {copied ? 'Copied' : 'Copy JSON patch'}
          </Button>
          <Button onClick={reset}>Reset to tokens</Button>
        </div>
        <pre className={styles.code}>{patch()}</pre>
      </section>
    </>
  )
}
