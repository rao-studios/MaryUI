import { describe, expect, it } from 'vitest'
import { CORNER_KEYS, cornerTargets, detunedFrequency, liquidity, restCorners, type RadiusParams } from './radius'

const params: RadiusParams = { rest: 12, min: 8, max: 16, spread: 6, velocityRef: 1100, gamma: 0.7 }

describe('liquid corners', () => {
  it('rests at the resting radius on every corner', () => {
    expect(cornerTargets(0, 0, params)).toEqual(restCorners(12))
  })

  it('flattens the leading corners and rounds the trailing ones', () => {
    // Travelling right: the right corners lead, the left corners trail.
    const c = cornerTargets(params.velocityRef, 0, params)
    expect(c.tr).toBeLessThan(params.rest)
    expect(c.br).toBeLessThan(params.rest)
    expect(c.tl).toBeGreaterThan(params.rest)
    expect(c.bl).toBeGreaterThan(params.rest)
    // Pure horizontal travel treats top and bottom alike.
    expect(c.tr).toBeCloseTo(c.br, 6)
    expect(c.tl).toBeCloseTo(c.bl, 6)
  })

  it('picks out a single corner on a diagonal fling', () => {
    const v = params.velocityRef
    const c = cornerTargets(v, v, params)
    expect(c.br).toBeLessThan(Math.min(c.tr, c.bl))
    expect(c.tl).toBeGreaterThan(Math.max(c.tr, c.bl))
    // The two side corners are neither leading nor trailing.
    expect(c.tr).toBeCloseTo(params.rest, 6)
    expect(c.bl).toBeCloseTo(params.rest, 6)
  })

  it('never leaves the 8..16px range, however hard it is flung', () => {
    for (const [vx, vy] of [[1e6, 0], [0, -1e6], [-1e6, 1e6], [3e5, -7e5]]) {
      const c = cornerTargets(vx, vy, params)
      for (const key of CORNER_KEYS) {
        expect(c[key]).toBeGreaterThanOrEqual(params.min)
        expect(c[key]).toBeLessThanOrEqual(params.max)
      }
    }
  })

  it('scales with speed between rest and full travel', () => {
    const slow = cornerTargets(params.velocityRef * 0.25, 0, params)
    const fast = cornerTargets(params.velocityRef, 0, params)
    expect(slow.tr).toBeGreaterThan(fast.tr)
    expect(slow.tr).toBeLessThan(params.rest)
  })

  it('reports liquidity as normalized, clamped speed', () => {
    expect(liquidity(0, 0, 2500)).toBe(0)
    expect(liquidity(2500, 0, 2500)).toBeCloseTo(1, 6)
    expect(liquidity(1e6, 1e6, 2500)).toBe(1)
    expect(liquidity(0, 1250, 2500)).toBeCloseTo(0.5, 6)
  })

  it('lifts an ordinary drag well clear of rest', () => {
    // A careful drag peaks near 400 px/s. Against the engine's 2500 px/s jelly
    // reference that spread the corners 1.6px of the 8 available; the corners
    // have their own, much lower reference precisely so this reads.
    const careful = cornerTargets(400, 0, params)
    expect(careful.tr).toBeLessThan(10)
    expect(careful.tl).toBeGreaterThan(14)
  })

  it('reaches the full range on a brisk drag, not only on a fling', () => {
    const brisk = cornerTargets(1050, 0, params)
    expect(brisk.tr).toBeCloseTo(params.min, 6)
    expect(brisk.tl).toBeCloseTo(params.max, 6)
  })

  it('curves the response so slow speeds are not crushed', () => {
    const quarter = params.rest - cornerTargets(params.velocityRef * 0.25, 0, params).tr
    const full = params.rest - cornerTargets(params.velocityRef, 0, params).tr
    expect(quarter / full).toBeGreaterThan(0.25)
  })

  it('detunes the four corner springs away from each other', () => {
    const f = [0, 1, 2, 3].map((i) => detunedFrequency(2.6, i, 0.08))
    expect(new Set(f).size).toBe(4)
    for (const v of f) expect(Math.abs(v - 2.6) / 2.6).toBeLessThanOrEqual(0.08 + 1e-9)
  })
})
