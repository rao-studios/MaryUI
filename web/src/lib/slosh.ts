/**
 * Liquid slosh: a damped pendulum driven by the motion of its container.
 *
 * Two things drive it, and the second one is why it reads at all. A container
 * that accelerates tips its liquid — but a window being dragged spends almost
 * all of its time at roughly constant velocity, where acceleration is zero, so
 * acceleration alone leaves the liquid dead flat through the middle of every
 * gesture and twitching only at its ends.
 *
 * So the liquid is also treated as viscous rather than frictionless: it is
 * dragged along by the well, its own bulk speed lagging behind, and that
 * *shear* — how far the liquid has failed to catch up — tilts the surface for as
 * long as the gesture lasts. The pendulum then rings it back down.
 *
 * One state per window is enough (all three bubbles share the same physical
 * motion); per-bubble variety comes from CSS animation phase, not physics.
 */

import { clamp } from './geometry'

export interface SloshState {
  /** Surface tilt in radians. Positive tilts the liquid toward +x. */
  theta: number
  /** Angular velocity in rad/s. */
  omega: number
}

/** What the container is doing to the liquid this frame. */
export interface SloshDrive {
  /** Container acceleration in px/s². The kick at the start and end of a gesture. */
  accel: number
  /**
   * Normalized velocity the liquid has not caught up with (-1..1) — the
   * container's speed minus a slow follower of it. Sustains through a drag.
   */
  shear: number
}

export interface SloshParams {
  frequencyHz: number
  dampingRatio: number
  /** Radians of tilt per px/s² of container acceleration. */
  gain: number
  /** Drive per unit of shear. */
  shearGain: number
  /** Response curve on the shear; below 1 lifts slow, careful drags. */
  shearGamma: number
  /** Hard limit on tilt so the liquid never flips. */
  maxAngle: number
  /** Accelerations beyond this are clipped before driving the pendulum. */
  maxAccel: number
}

export function createSlosh(): SloshState {
  return { theta: 0, omega: 0 }
}

/** Advances the pendulum by `dt` seconds under what the container is doing. */
export function stepSlosh(s: SloshState, drive: SloshDrive, dt: number, p: SloshParams): SloshState {
  const k = (2 * Math.PI * p.frequencyHz) ** 2
  const c = 2 * p.dampingRatio * Math.sqrt(k)
  const shear = clamp(drive.shear, -1, 1)
  const force =
    clamp(drive.accel, -p.maxAccel, p.maxAccel) * p.gain +
    Math.sign(shear) * Math.abs(shear) ** p.shearGamma * p.shearGain
  const alpha = -k * s.theta - c * s.omega - force
  const omega = s.omega + alpha * dt
  const theta = clamp(s.theta + omega * dt, -p.maxAngle, p.maxAngle)
  // Hitting the rim bleeds energy, like liquid splashing against glass.
  return { theta, omega: Math.abs(theta) >= p.maxAngle ? omega * 0.4 : omega }
}

/**
 * Tolerance is a tilt nobody can see at 18px (0.005 rad ≈ 0.29°), not a tilt of
 * zero — otherwise the engine animates a ring-down long past the point it reads.
 */
export function isSloshSettled(s: SloshState, tolerance = 0.005, velocityTolerance = 0.03): boolean {
  return Math.abs(s.theta) < tolerance && Math.abs(s.omega) < velocityTolerance
}
