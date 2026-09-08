import { describe, expect, it } from 'vitest'
import { PointerTracker } from './velocity'

describe('PointerTracker', () => {
  it('reports the velocity of steady motion once the EMA has warmed up', () => {
    const tracker = new PointerTracker()
    let result = { vx: 0, vy: 0, ax: 0, ay: 0 }
    for (let i = 0; i <= 30; i++) {
      const t = i * 16
      tracker.push(t, i * 8, 0) // 500 px/s
      result = tracker.sample(t + 1)
    }
    expect(result.vx).toBeCloseTo(500, 0)
    expect(result.vy).toBe(0)
  })

  it('produces acceleration when velocity changes and zero once steady', () => {
    const tracker = new PointerTracker()
    let peak = 0
    let last = { vx: 0, vy: 0, ax: 0, ay: 0 }
    for (let i = 0; i <= 40; i++) {
      const t = i * 16
      tracker.push(t, i * 8, 0)
      last = tracker.sample(t + 1)
      peak = Math.max(peak, Math.abs(last.ax))
    }
    expect(peak).toBeGreaterThan(1000)
    expect(Math.abs(last.ax)).toBeLessThan(1)
  })

  it('decays to rest when samples stop arriving', () => {
    const tracker = new PointerTracker()
    for (let i = 0; i <= 10; i++) {
      tracker.push(i * 16, i * 8, 0)
      tracker.sample(i * 16 + 1)
    }
    let result = tracker.sample(400)
    for (let f = 1; f < 20; f++) result = tracker.sample(400 + f * 16)
    expect(Math.abs(result.vx)).toBeLessThan(1)
  })

  it('resets cleanly', () => {
    const tracker = new PointerTracker()
    tracker.push(0, 0, 0)
    tracker.push(16, 100, 0)
    tracker.sample(17)
    tracker.reset()
    expect(tracker.hasSamples).toBe(false)
    expect(tracker.sample(40)).toEqual({ vx: 0, vy: 0, ax: 0, ay: 0 })
  })
})
