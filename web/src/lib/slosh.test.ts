import { describe, expect, it } from 'vitest'
import { createSlosh, isSloshSettled, stepSlosh, type SloshParams } from './slosh'

const params: SloshParams = { frequencyHz: 1.7, dampingRatio: 0.14, gain: 0.00005, maxAngle: 0.5, maxAccel: 12000 }

describe('slosh', () => {
  it('settles at the static tilt for a constant acceleration', () => {
    let s = createSlosh()
    const ax = 4000
    for (let i = 0; i < 120 * 10; i++) s = stepSlosh(s, ax, 1 / 120, params)
    const k = (2 * Math.PI * params.frequencyHz) ** 2
    expect(s.theta).toBeCloseTo((-ax * params.gain) / k, 4)
  })

  it('rings at roughly its natural period once released', () => {
    let s = { theta: 0.3, omega: 0 }
    const crossings: number[] = []
    const dt = 1 / 240
    let prev = s.theta
    for (let t = 0; t < 2; t += dt) {
      s = stepSlosh(s, 0, dt, params)
      if (Math.sign(s.theta) !== Math.sign(prev) && prev !== 0) crossings.push(t)
      prev = s.theta
    }
    expect(crossings.length).toBeGreaterThanOrEqual(4)
    const half = crossings[1] - crossings[0]
    expect(half).toBeCloseTo(1 / (2 * params.frequencyHz), 1)
  })

  it('decays to rest and reports settled', () => {
    let s = { theta: 0.4, omega: 0 }
    for (let i = 0; i < 120 * 8; i++) s = stepSlosh(s, 0, 1 / 120, params)
    expect(isSloshSettled(s)).toBe(true)
  })

  it('never exceeds the rim and never produces NaN at coarse steps', () => {
    let s = createSlosh()
    for (let i = 0; i < 200; i++) {
      s = stepSlosh(s, i % 2 ? 1e6 : -1e6, 1 / 30, params)
      expect(Number.isFinite(s.theta)).toBe(true)
      expect(Math.abs(s.theta)).toBeLessThanOrEqual(params.maxAngle)
    }
  })
})
