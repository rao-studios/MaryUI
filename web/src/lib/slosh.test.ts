import { describe, expect, it } from 'vitest'
import { createSlosh, isSloshSettled, stepSlosh, type SloshParams } from './slosh'

const params: SloshParams = {
  frequencyHz: 1.3,
  dampingRatio: 0.16,
  gain: 0.0009,
  shearGain: 18,
  shearGamma: 0.7,
  maxAngle: 0.5,
  maxAccel: 12000,
}

const still = { accel: 0, shear: 0 }

describe('slosh', () => {
  it('settles at the static tilt for a constant acceleration', () => {
    let s = createSlosh()
    const accel = 4000
    for (let i = 0; i < 120 * 10; i++) s = stepSlosh(s, { accel, shear: 0 }, 1 / 120, params)
    const k = (2 * Math.PI * params.frequencyHz) ** 2
    expect(s.theta).toBeCloseTo((-accel * params.gain) / k, 4)
  })

  it('holds a steady lean while the liquid is being sheared', () => {
    // A dragged window is mostly constant velocity, where accel is zero. Shear
    // is what keeps the surface tilted through the middle of the gesture.
    let s = createSlosh()
    for (let i = 0; i < 120 * 8; i++) s = stepSlosh(s, { accel: 0, shear: 0.5 }, 1 / 120, params)
    const k = (2 * Math.PI * params.frequencyHz) ** 2
    expect(s.theta).toBeCloseTo(-(0.5 ** params.shearGamma * params.shearGain) / k, 4)
    expect(Math.abs(s.theta)).toBeGreaterThan(0.1)
  })

  it('lifts slow gestures more than a linear response would', () => {
    const tilt = (shear: number) => {
      let s = createSlosh()
      for (let i = 0; i < 120 * 8; i++) s = stepSlosh(s, { accel: 0, shear }, 1 / 120, params)
      return Math.abs(s.theta)
    }
    // Quarter the shear, and gamma < 1 leaves well over a quarter of the tilt.
    expect(tilt(0.25) / tilt(1)).toBeGreaterThan(0.3)
  })

  it('rings at roughly its natural period once released', () => {
    let s = { theta: 0.3, omega: 0 }
    const crossings: number[] = []
    const dt = 1 / 240
    let prev = s.theta
    for (let t = 0; t < 2; t += dt) {
      s = stepSlosh(s, still, dt, params)
      if (Math.sign(s.theta) !== Math.sign(prev) && prev !== 0) crossings.push(t)
      prev = s.theta
    }
    expect(crossings.length).toBeGreaterThanOrEqual(4)
    const half = crossings[1] - crossings[0]
    expect(half).toBeCloseTo(1 / (2 * params.frequencyHz), 1)
  })

  it('decays to rest and reports settled', () => {
    let s = { theta: 0.4, omega: 0 }
    for (let i = 0; i < 120 * 8; i++) s = stepSlosh(s, still, 1 / 120, params)
    expect(isSloshSettled(s)).toBe(true)
  })

  it('never exceeds the rim and never produces NaN at coarse steps', () => {
    let s = createSlosh()
    for (let i = 0; i < 200; i++) {
      s = stepSlosh(s, { accel: i % 2 ? 1e6 : -1e6, shear: i % 2 ? 40 : -40 }, 1 / 30, params)
      expect(Number.isFinite(s.theta)).toBe(true)
      expect(Math.abs(s.theta)).toBeLessThanOrEqual(params.maxAngle)
    }
  })
})
