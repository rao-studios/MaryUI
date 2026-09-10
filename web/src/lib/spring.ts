/**
 * A one-dimensional damped spring, parameterized the way designers think about
 * it — natural frequency in Hz and a damping ratio — rather than raw stiffness.
 * Integrated with semi-implicit Euler, which is stable for every frequency the
 * design system uses at frame rates down to 30 fps.
 *
 * Framework-free and allocation-free: `stepSpring` mutates in place so the
 * motion engine can run dozens of springs per frame without garbage.
 */

export interface SpringParams {
  /** Natural frequency in Hz — how many oscillations per second when undamped. */
  frequency: number
  /** Damping ratio: < 1 overshoots, 1 is critical, > 1 is sluggish. */
  damping: number
}

export interface Spring {
  value: number
  velocity: number
  target: number
}

export function createSpring(value = 0, target = value): Spring {
  return { value, velocity: 0, target }
}

/** Advances the spring by `dt` seconds toward its target. Returns the same object. */
export function stepSpring(s: Spring, dt: number, p: SpringParams): Spring {
  const k = (2 * Math.PI * p.frequency) ** 2
  const c = 2 * p.damping * Math.sqrt(k)
  const acceleration = -k * (s.value - s.target) - c * s.velocity
  s.velocity += acceleration * dt
  s.value += s.velocity * dt
  return s
}

/** Jumps straight to the target with no residual motion. */
export function snapSpring(s: Spring): Spring {
  s.value = s.target
  s.velocity = 0
  return s
}

export function isSettled(s: Spring, tolerance = 0.001, velocityTolerance = 0.01): boolean {
  return Math.abs(s.value - s.target) < tolerance && Math.abs(s.velocity) < velocityTolerance
}

/**
 * A first-order lag: closes a fraction of the remaining gap every frame, so it
 * tracks a moving target with a constant trail and eases to rest when the
 * target stops — and, unlike a spring, it can never overshoot.
 *
 * Use it where something should *follow* rather than *bounce*. The brushed
 * grain is the case that named it: on a spring it sprang back past the frame
 * when a drag stopped, which read as the metal being on elastic rather than
 * being dragged. `tauMs` is the time constant — the gap is down to 37% after
 * one, and effectively closed after three.
 *
 * Frame-rate independent: the exponential is evaluated against real dt rather
 * than assuming a fixed step.
 */
export function follow(value: number, target: number, dt: number, tauMs: number): number {
  if (tauMs <= 0) return target
  return target + (value - target) * Math.exp((-dt * 1000) / tauMs)
}
