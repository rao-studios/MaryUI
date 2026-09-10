/**
 * Liquid corners: a moving window is a drop, and a drop's corners do not agree.
 *
 * Each corner is scored by how much it *leads* the travel: project the unit
 * velocity onto the corner's outward normal. A leading corner is pushed flat by
 * the direction of travel; a trailing corner is left rounder by surface tension.
 * The four results feed four independent springs in the motion engine (detuned
 * against each other), so they arrive, overshoot and settle out of phase.
 *
 * Pure and framework-free, like `spring.ts` and `slosh.ts`.
 */

import { clamp } from './geometry'

export interface Corners {
  tl: number
  tr: number
  br: number
  bl: number
}

export interface RadiusParams {
  /** Resting radius in px — where every corner sits on an idle window. */
  rest: number
  /** Hard floor for a fully leading corner. */
  min: number
  /** Hard ceiling for a fully trailing corner. */
  max: number
  /** px a corner travels from rest at full speed. */
  spread: number
  /**
   * Speed (px/s) that counts as "full speed". Deliberately its own value rather
   * than the engine's `velocity-ref`: that one is scaled for the jelly, where
   * full deflection should need a hard fling. Corners want to be at full spread
   * for an ordinary drag, which peaks nearer 400 px/s.
   */
  velocityRef: number
  /** Response curve on the normalized speed. Below 1 lifts slow, careful drags. */
  gamma: number
}

/** Outward normals, normalized so a corner facing straight into the travel scores 1. */
const SQRT1_2 = Math.SQRT1_2
const NORMALS: Record<keyof Corners, readonly [number, number]> = {
  tl: [-SQRT1_2, -SQRT1_2],
  tr: [SQRT1_2, -SQRT1_2],
  br: [SQRT1_2, SQRT1_2],
  bl: [-SQRT1_2, SQRT1_2],
}

export const CORNER_KEYS = ['tl', 'tr', 'br', 'bl'] as const

/** 0..1: how liquid the geometry is right now. Controls scale their own radius by this. */
export function liquidity(vx: number, vy: number, velocityRef: number, gamma = 1): number {
  return clamp(Math.hypot(vx, vy) / Math.max(velocityRef, 1), 0, 1) ** gamma
}

/** Resting radius on every corner. */
export function restCorners(rest: number): Corners {
  return { tl: rest, tr: rest, br: rest, bl: rest }
}

/**
 * Where each corner wants to be, in px, for a window travelling at (vx, vy) px/s.
 * At rest every corner returns `rest`; at speed the leading pair flattens toward
 * `min` and the trailing pair rounds toward `max`.
 */
export function cornerTargets(vx: number, vy: number, p: RadiusParams): Corners {
  const speed = Math.hypot(vx, vy)
  if (speed < 1e-3) return restCorners(p.rest)

  const k = liquidity(vx, vy, p.velocityRef, p.gamma)
  const ux = vx / speed
  const uy = vy / speed
  const out = {} as Corners
  for (const key of CORNER_KEYS) {
    const [nx, ny] = NORMALS[key]
    const lead = ux * nx + uy * ny
    out[key] = clamp(p.rest - lead * p.spread * k, p.min, p.max)
  }
  return out
}

/**
 * Spring frequency for corner `index`, spread around `base` by ±`detune`.
 * Four slightly different springs is what stops the corners from moving as one
 * rounded rectangle and makes them read as four independent bulges of water.
 */
export function detunedFrequency(base: number, index: number, detune: number): number {
  const ladder = [-1, 0.5, -0.5, 1]
  return base * (1 + ladder[index % ladder.length] * detune)
}
