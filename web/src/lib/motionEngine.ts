/**
 * The one animation loop behind everything that moves fluidly.
 *
 * `MotionEngine` owns a single requestAnimationFrame loop per page and steps
 * every registered target while any of them is still moving; an idle desktop
 * schedules no frames at all. `WindowMotion` is the per-window target: it
 * turns pointer velocity into the sheen position, the tilt, the jelly
 * deformation, the FLIP flight, and the liquid slosh, and writes the results
 * straight to the DOM as a `transform` and a handful of `--lp-*` variables.
 * React never sees a frame.
 *
 * The constants live in `motionParams`, a mutable copy of the motion tokens so
 * the Gallery's Motion tab can retune the feel live.
 */

import { createSpring, isSettled, snapSpring, stepSpring, type SpringParams } from './spring'
import { createSlosh, isSloshSettled, stepSlosh, type SloshParams, type SloshState } from './slosh'
import { PointerTracker } from './velocity'
import { clamp, flipTransform, type Rect } from './geometry'
import { prefersReducedMotion, setVars } from './dom'
import { tokens } from '@/tokens/tokens'

// MARK: - Parameters

export interface MotionParams {
  sheen: SpringParams
  tilt: SpringParams
  jelly: SpringParams
  fly: SpringParams
  slosh: SloshParams
  jellyMaxScale: number
  jellyMaxSkew: number
  tiltMax: number
  velocityRef: number
  /** Where the room light sits, as a fraction of viewport width. */
  lightX: number
}

export function motionParamsFromTokens(): MotionParams {
  const m = tokens.motion
  return {
    sheen: { ...m.springSheen },
    tilt: { ...m.springTilt },
    jelly: { ...m.springJelly },
    fly: { ...m.springFly },
    slosh: {
      frequencyHz: m.sloshFrequency,
      dampingRatio: m.sloshDamping,
      gain: m.sloshGain,
      maxAngle: m.sloshMax,
      maxAccel: m.sloshMaxAccel,
    },
    jellyMaxScale: m.jellyMaxScale,
    jellyMaxSkew: m.jellyMaxSkew,
    tiltMax: m.tiltMax,
    velocityRef: m.velocityRef,
    lightX: tokens.sheen.lightX,
  }
}

/** Live, mutable motion constants. Mutate fields in place; targets read them every frame. */
export const motionParams: MotionParams = motionParamsFromTokens()

export function resetMotionParams(): void {
  Object.assign(motionParams, motionParamsFromTokens())
}

// MARK: - Engine

export interface MotionTarget {
  /** Advance by `dt` seconds. Return true while still moving. */
  step(dt: number, now: number): boolean
}

export class MotionEngine {
  private targets = new Set<MotionTarget>()
  private frame: number | null = null
  private last = 0

  add(target: MotionTarget): void {
    this.targets.add(target)
  }

  remove(target: MotionTarget): void {
    this.targets.delete(target)
  }

  /** Starts the loop if it is idle. Safe to call every pointer event. */
  wake(): void {
    if (this.frame !== null || typeof requestAnimationFrame === 'undefined') return
    this.last = performance.now()
    this.frame = requestAnimationFrame(this.tick)
  }

  get running(): boolean {
    return this.frame !== null
  }

  private tick = (now: number): void => {
    const dt = Math.min(Math.max(now - this.last, 0) / 1000, 1 / 30)
    this.last = now
    let active = false
    for (const target of this.targets) active = target.step(dt, now) || active
    this.frame = active ? requestAnimationFrame(this.tick) : null
  }
}

/** The page's engine. One desktop per page, one loop per desktop. */
export const engine = new MotionEngine()

// MARK: - Window motion

export interface WindowMotionElements {
  /** React-owned: layout, z-index, and the `--lp-*` motion variables. */
  frame: HTMLElement
  /** Engine-owned: `transform` only. */
  chrome: HTMLElement
}

export class WindowMotion implements MotionTarget {
  private elements: WindowMotionElements | null = null
  private tracker = new PointerTracker()

  private sheen = createSpring(0.5)
  private tilt = createSpring(0)
  private skew = createSpring(0)
  private sx = createSpring(1)
  private sy = createSpring(1)
  private flyX = createSpring(0)
  private flyY = createSpring(0)
  private flySx = createSpring(1)
  private flySy = createSpring(1)
  private slosh: SloshState = createSlosh()
  private sloshY: SloshState = createSlosh()

  private dragging = false
  private resizing = false
  private flying = false
  private dragDx = 0
  private dragDy = 0
  private lastVx = 0
  private wroteIdentity = false

  attach(elements: WindowMotionElements): void {
    this.elements = elements
    engine.add(this)
    this.nudge()
  }

  detach(): void {
    engine.remove(this)
    this.elements = null
  }

  /** Wakes the loop so the sheen re-settles after a layout change (resize, zoom, viewport). */
  nudge(): void {
    engine.wake()
  }

  // MARK: Drag

  beginDrag(grabX: number, grabY: number): void {
    this.dragging = true
    this.dragDx = 0
    this.dragDy = 0
    this.tracker.reset()
    if (this.elements) {
      this.elements.chrome.style.transformOrigin = `${grabX}px ${grabY}px`
      this.elements.chrome.style.willChange = 'transform'
    }
    engine.wake()
  }

  moveDrag(dx: number, dy: number, t: number, x: number, y: number): void {
    this.dragDx = dx
    this.dragDy = dy
    this.tracker.push(t, x, y)
    engine.wake()
  }

  /** Ends the drag. The caller has already committed the final layout, so the delta returns to zero at once. */
  endDrag(): void {
    this.dragging = false
    this.dragDx = 0
    this.dragDy = 0
    this.tracker.reset()
    this.writeTransform()
    engine.wake()
  }

  get isDragging(): boolean {
    return this.dragging
  }

  // MARK: Resize

  beginResize(): void {
    this.resizing = true
    engine.wake()
  }

  endResize(): void {
    this.resizing = false
    engine.wake()
  }

  // MARK: Flight

  /** The element is now laid out at `to`; make it appear at `from` and spring home. */
  flyFrom(from: Rect, to: Rect): void {
    const t = flipTransform(from, to)
    if (prefersReducedMotion()) return
    this.flyX.value = t.tx
    this.flyY.value = t.ty
    this.flySx.value = t.sx
    this.flySy.value = t.sy
    this.flyX.velocity = this.flyY.velocity = this.flySx.velocity = this.flySy.velocity = 0
    this.flying = true
    if (this.elements) {
      this.elements.chrome.style.transformOrigin = '0 0'
      this.elements.chrome.style.willChange = 'transform'
    }
    this.writeTransform()
    engine.wake()
  }

  // MARK: Step

  step(dt: number, now: number): boolean {
    const el = this.elements
    if (!el) return false
    const p = motionParams
    const reduced = prefersReducedMotion()

    const { vx, vy, ax, ay } = this.dragging ? this.tracker.sample(now) : { vx: 0, vy: 0, ax: 0, ay: 0 }
    const rect = this.liveRect()
    const vw = window.innerWidth || 1

    this.sheen.target = clamp((p.lightX * vw - (rect.x + this.dragDx)) / Math.max(rect.w, 1), -0.3, 1.3)
    const n = clamp(vx / p.velocityRef, -1, 1)
    const m = clamp(Math.hypot(vx, vy) / p.velocityRef, 0, 1)
    const deform = this.dragging && !this.resizing && !reduced
    this.tilt.target = deform ? n * p.tiltMax : 0
    this.skew.target = deform ? n * p.jellyMaxSkew : 0
    this.sx.target = deform ? 1 + m * p.jellyMaxScale : 1
    this.sy.target = deform ? 1 - 0.6 * m * p.jellyMaxScale : 1

    if (reduced) {
      snapSpring(this.sheen)
      snapSpring(this.tilt)
      snapSpring(this.skew)
      snapSpring(this.sx)
      snapSpring(this.sy)
      snapSpring(this.flyX)
      snapSpring(this.flyY)
      snapSpring(this.flySx)
      snapSpring(this.flySy)
      this.slosh = createSlosh()
      this.sloshY = createSlosh()
      this.flying = false
    } else {
      stepSpring(this.sheen, dt, p.sheen)
      stepSpring(this.tilt, dt, p.tilt)
      stepSpring(this.skew, dt, p.jelly)
      stepSpring(this.sx, dt, p.jelly)
      stepSpring(this.sy, dt, p.jelly)
      stepSpring(this.flyX, dt, p.fly)
      stepSpring(this.flyY, dt, p.fly)
      stepSpring(this.flySx, dt, p.fly)
      stepSpring(this.flySy, dt, p.fly)
      this.slosh = stepSlosh(this.slosh, ax, dt, p.slosh)
      this.sloshY = stepSlosh(this.sloshY, ay, dt, p.slosh)
    }

    setVars(el.frame, {
      '--lp-sheen-x': this.sheen.value.toFixed(4),
      '--lp-tilt': this.tilt.value.toFixed(3),
      '--lp-vx': n.toFixed(3),
      '--lp-slosh': `${((-this.slosh.theta * 180) / Math.PI).toFixed(2)}deg`,
      '--lp-slosh-y': `${(this.sloshY.theta * 6).toFixed(2)}px`,
    })
    this.lastVx = n

    const flightSettled =
      isSettled(this.flyX, 0.05, 0.5) &&
      isSettled(this.flyY, 0.05, 0.5) &&
      isSettled(this.flySx, 0.0005, 0.005) &&
      isSettled(this.flySy, 0.0005, 0.005)
    if (this.flying && flightSettled) {
      this.flying = false
      snapSpring(this.flyX)
      snapSpring(this.flyY)
      snapSpring(this.flySx)
      snapSpring(this.flySy)
    }

    const deformSettled =
      isSettled(this.skew, 0.005, 0.05) && isSettled(this.sx, 0.0005, 0.005) && isSettled(this.sy, 0.0005, 0.005)
    const settled =
      !this.dragging &&
      !this.flying &&
      !this.resizing &&
      deformSettled &&
      isSettled(this.tilt, 0.005, 0.05) &&
      isSettled(this.sheen, 0.0005, 0.005) &&
      isSloshSettled(this.slosh) &&
      isSloshSettled(this.sloshY)

    if (settled) {
      if (!this.wroteIdentity) {
        el.chrome.style.transform = ''
        el.chrome.style.willChange = ''
        this.wroteIdentity = true
      }
      return false
    }

    this.wroteIdentity = false
    this.writeTransform()
    return true
  }

  private writeTransform(): void {
    const el = this.elements
    if (!el) return
    const tx = this.dragDx + this.flyX.value
    const ty = this.dragDy + this.flyY.value
    const sx = this.sx.value * this.flySx.value
    const sy = this.sy.value * this.flySy.value
    el.chrome.style.transform = `translate3d(${tx.toFixed(2)}px, ${ty.toFixed(2)}px, 0) skewX(${this.skew.value.toFixed(3)}deg) scale(${sx.toFixed(4)}, ${sy.toFixed(4)})`
  }

  private liveRect(): Rect {
    const f = this.elements!.frame
    return { x: f.offsetLeft, y: f.offsetTop, w: f.offsetWidth, h: f.offsetHeight }
  }

  /** Normalized horizontal velocity from the last frame; the traffic lights smear with it. */
  get velocityX(): number {
    return this.lastVx
  }
}
