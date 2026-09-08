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
