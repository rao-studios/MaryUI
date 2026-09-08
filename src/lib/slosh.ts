/**
 * Liquid slosh: a damped pendulum driven by the acceleration of its container.
 * Drag a window and the liquid in its indicators tilts against the motion, then
 * rings down — exactly like water in a glass you just moved.
 *
 * One state per window is enough (all three bubbles share the same physical
 * acceleration); per-bubble variety comes from CSS animation phase, not physics.
 */

import { clamp } from './geometry'

export interface SloshState {
  /** Surface tilt in radians. Positive tilts the liquid toward +x. */
  theta: number
  /** Angular velocity in rad/s. */
  omega: number
}

export interface SloshParams {
  frequencyHz: number
  dampingRatio: number
  /** Radians of tilt per px/s² of container acceleration. */
  gain: number
  /** Hard limit on tilt so the liquid never flips. */
  maxAngle: number
  /** Accelerations beyond this are clipped before driving the pendulum. */
  maxAccel: number
}

export function createSlosh(): SloshState {
  return { theta: 0, omega: 0 }
}

/** Advances the pendulum by `dt` seconds under a container acceleration `ax` (px/s²). */
export function stepSlosh(s: SloshState, ax: number, dt: number, p: SloshParams): SloshState {
  const k = (2 * Math.PI * p.frequencyHz) ** 2
  const c = 2 * p.dampingRatio * Math.sqrt(k)
  const drive = clamp(ax, -p.maxAccel, p.maxAccel) * p.gain
  const alpha = -k * s.theta - c * s.omega - drive
  const omega = s.omega + alpha * dt
  const theta = clamp(s.theta + omega * dt, -p.maxAngle, p.maxAngle)
  // Hitting the rim bleeds energy, like liquid splashing against glass.
  return { theta, omega: Math.abs(theta) >= p.maxAngle ? omega * 0.4 : omega }
}

export function isSloshSettled(s: SloshState, tolerance = 0.002, velocityTolerance = 0.02): boolean {
  return Math.abs(s.theta) < tolerance && Math.abs(s.omega) < velocityTolerance
}
