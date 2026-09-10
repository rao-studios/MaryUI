/**
 * The one animation loop behind everything that moves fluidly.
 *
 * `MotionEngine` owns a single requestAnimationFrame loop per page and steps
 * every registered target while any of them is still moving; an idle desktop
 * schedules no frames at all. `WindowMotion` is the per-window target: it
 * turns pointer velocity into the sheen position, the tilt, the jelly
 * deformation, the FLIP flight, the four liquid corners, the lag of the brushed
 * grain, and the liquid slosh, and writes the results straight to the DOM as a
 * `transform` and a handful of `--lp-*` variables. React never sees a frame.
 *
 * Everything it writes is composited (`transform`, `opacity`) except the four
 * corner radii, which repaint — those are quantized to a quarter-pixel.
 *
 * The constants live in `motionParams`, a mutable copy of the motion tokens so
 * the Gallery's Motion tab can retune the feel live.
 */

import { createSpring, follow, isSettled, snapSpring, stepSpring, type SpringParams } from './spring'
import { createSlosh, isSloshSettled, stepSlosh, type SloshParams, type SloshState } from './slosh'
import { PointerTracker } from './velocity'
import { CORNER_KEYS, cornerTargets, detunedFrequency, liquidity, type Corners, type RadiusParams } from './radius'
import { clamp, flipTransform, type Rect } from './geometry'
import { prefersReducedMotion, setVars } from './dom'
import { tokens } from '@/tokens/tokens'

// MARK: - Parameters

export interface MotionParams {
  sheen: SpringParams
  tilt: SpringParams
  jelly: SpringParams
  fly: SpringParams
  /** One spring for all four corners; each corner detunes its frequency off this. */
  radius: SpringParams
  /** Time constant (ms) of the grain's lag behind the frame. Not a spring: it
   *  must never overshoot, or the metal skin bounces when a drag stops. */
  grainFollow: number
  /** A slow follower of vx; the difference is what smears the goo. */
  vxLag: SpringParams
  slosh: SloshParams
  jellyMaxScale: number
  jellyMaxSkew: number
  tiltMax: number
  velocityRef: number
  /** Where the room light sits, as a fraction of viewport width. */
  lightX: number
  /** Rest / min / max / spread for the liquid corners. */
  corners: RadiusParams
  /** Per-corner frequency spread, so the four never settle in lockstep. */
  radiusDetune: number
  /** px/s kicked into each corner spring when a window is grabbed and dropped. */
  cornerImpulse: number
  /** Speed (px/s) that counts as fully sheared, for the liquid. */
  sloshVelocityRef: number
  /** Max px the grain slides behind the frame. */
  grainLag: number
}

export function motionParamsFromTokens(): MotionParams {
  const m = tokens.motion
  return {
    sheen: { ...m.springSheen },
    tilt: { ...m.springTilt },
    jelly: { ...m.springJelly },
    fly: { ...m.springFly },
    radius: { ...m.springRadius },
    grainFollow: parseFloat(m.grainFollow),
    vxLag: { ...m.springVxLag },
    slosh: {
      frequencyHz: m.sloshFrequency,
      dampingRatio: m.sloshDamping,
      gain: m.sloshGain,
      shearGain: m.sloshShearGain,
      shearGamma: m.sloshShearGamma,
      maxAngle: m.sloshMax,
      maxAccel: m.sloshMaxAccel,
    },
    jellyMaxScale: m.jellyMaxScale,
    jellyMaxSkew: m.jellyMaxSkew,
    tiltMax: m.tiltMax,
    velocityRef: m.velocityRef,
    lightX: tokens.sheen.lightX,
    corners: {
      rest: parseFloat(tokens.radius.window),
      min: parseFloat(tokens.radius.windowMin),
      max: parseFloat(tokens.radius.windowMax),
      spread: tokens.radiusFlex.spread,
      velocityRef: tokens.radiusFlex.velocityRef,
      gamma: tokens.radiusFlex.gamma,
    },
    radiusDetune: m.radiusDetune,
    cornerImpulse: tokens.radiusFlex.impulse,
    sloshVelocityRef: m.sloshVelocityRef,
    grainLag: tokens.brush.lag,
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
  /** One spring per corner, in CORNER_KEYS order, each detuned off `params.radius`. */
  private corners = CORNER_KEYS.map(() => createSpring(parseFloat(tokens.radius.window)))
  /** Plain followers, not springs — see `follow` in ./spring. */
  private grainX = 0
  private grainY = 0
  private vxLag = createSpring(0)
  private vyLag = createSpring(0)
  private wroteCorners: Corners | null = null

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
    this.nudgeCorners(motionParams.cornerImpulse)
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
    // The frame is about to jump to its committed position while the delta drops
    // to zero. Carry the grain's lag across with it, or the skin would snap.
    this.grainX -= this.dragDx
    this.grainY -= this.dragDy
    this.dragging = false
    this.dragDx = 0
    this.dragDy = 0
    this.tracker.reset()
    this.nudgeCorners(-motionParams.cornerImpulse)
    this.writeTransform()
    engine.wake()
  }

  /**
   * Kicks the four corner springs — picking a window up and setting it down are
   * impacts. Because the springs are detuned against each other, one kick sets
   * all four wobbling out of phase and settling, which puts visible motion at
   * the two moments the eye is actually on the window. An impulse rather than a
   * bias, so it costs none of the lead/trail range during the drag itself.
   */
  nudgeCorners(impulse: number): void {
    if (prefersReducedMotion()) return
    for (const [i, corner] of this.corners.entries()) {
      corner.velocity += impulse * (i % 2 === 0 ? 1 : -1)
    }
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
    const ny = clamp(vy / p.velocityRef, -1, 1)
    const m = clamp(Math.hypot(vx, vy) / p.velocityRef, 0, 1)
    const deform = this.dragging && !this.resizing && !reduced
    this.tilt.target = deform ? n * p.tiltMax : 0
    this.skew.target = deform ? n * p.jellyMaxSkew : 0
    this.sx.target = deform ? 1 + m * p.jellyMaxScale : 1
    this.sy.target = deform ? 1 - 0.6 * m * p.jellyMaxScale : 1

    // The corners answer to how the window is *seen* to move, so a zoom's flight
    // deforms them exactly as a drag does.
    const seenVx = vx + this.flyX.velocity
    const seenVy = vy + this.flyY.velocity
    const targets = reduced ? null : cornerTargets(seenVx, seenVy, p.corners)
    CORNER_KEYS.forEach((key, i) => {
      this.corners[i].target = targets ? targets[key] : p.corners.rest
    })
    this.vxLag.target = n
    this.vyLag.target = ny


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
      for (const c of this.corners) snapSpring(c)
      this.grainX = this.dragDx
      this.grainY = this.dragDy
      snapSpring(this.vxLag)
      snapSpring(this.vyLag)
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
      // The lag springs follow velocity normalized against the jelly's reference;
      // the liquid shears against its own, lower one. Both normalizations are
      // linear, so rescaling here is exact and saves a second pair of springs.
      const shearScale = p.velocityRef / p.sloshVelocityRef
      this.slosh = stepSlosh(this.slosh, { accel: ax, shear: (n - this.vxLag.value) * shearScale }, dt, p.slosh)
      this.sloshY = stepSlosh(this.sloshY, { accel: ay, shear: (ny - this.vyLag.value) * shearScale }, dt, p.slosh)
      this.corners.forEach((c, i) => {
        stepSpring(c, dt, { frequency: detunedFrequency(p.radius.frequency, i, p.radiusDetune), damping: p.radius.damping })
      })
      // The grain chases the frame's own translation, so while a drag is moving
      // it trails by a constant amount and when the drag stops it glides home.
      this.grainX = follow(this.grainX, this.dragDx, dt, p.grainFollow)
      this.grainY = follow(this.grainY, this.dragDy, dt, p.grainFollow)
      stepSpring(this.vxLag, dt, p.vxLag)
      stepSpring(this.vyLag, dt, p.vxLag)
    }

    const gx = clamp(this.grainX - this.dragDx, -p.grainLag, p.grainLag)
    const gy = clamp(this.grainY - this.dragDy, -p.grainLag, p.grainLag)

    setVars(el.frame, {
      '--lp-sheen-x': this.sheen.value.toFixed(4),
      '--lp-tilt': this.tilt.value.toFixed(3),
      '--lp-vx': n.toFixed(3),
      '--lp-vx-lag': this.vxLag.value.toFixed(3),
      '--lp-speed': liquidity(seenVx, seenVy, p.velocityRef).toFixed(3),
      '--lp-slosh': `${((-this.slosh.theta * 180) / Math.PI).toFixed(2)}deg`,
      '--lp-slosh-x': `${(-Math.sin(this.slosh.theta) * tokens.motion.sloshShift).toFixed(2)}px`,
      '--lp-slosh-y': `${(this.sloshY.theta * tokens.motion.sloshLift).toFixed(2)}px`,
      '--lp-grain-x': `${gx.toFixed(2)}px`,
      '--lp-grain-y': `${gy.toFixed(2)}px`,
    })
    this.writeCorners(el.frame)
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
      this.corners.every((c) => isSettled(c, 0.02, 0.2)) &&
      Math.abs(this.grainX - this.dragDx) < 0.05 &&
      Math.abs(this.grainY - this.dragDy) < 0.05 &&
      isSettled(this.vxLag, 0.002, 0.02) &&
      isSettled(this.vyLag, 0.002, 0.02) &&
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

  /**
   * `border-radius` repaints, unlike `transform`, so only write a corner once it
   * has actually moved a visible quarter-pixel.
   */
  private writeCorners(frame: HTMLElement): void {
    const next = {} as Corners
    CORNER_KEYS.forEach((key, i) => {
      next[key] = this.corners[i].value
    })
    const prev = this.wroteCorners
    if (prev && CORNER_KEYS.every((key) => Math.abs(next[key] - prev[key]) < 0.25)) return
    this.wroteCorners = next
    setVars(frame, {
      '--lp-r-tl': `${next.tl.toFixed(2)}px`,
      '--lp-r-tr': `${next.tr.toFixed(2)}px`,
      '--lp-r-br': `${next.br.toFixed(2)}px`,
      '--lp-r-bl': `${next.bl.toFixed(2)}px`,
      '--lp-radius-k': (
        Math.max(...CORNER_KEYS.map((key) => Math.abs(next[key] - motionParams.corners.rest))) /
        Math.max(motionParams.corners.max - motionParams.corners.rest, 1)
      ).toFixed(3),
    })
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
