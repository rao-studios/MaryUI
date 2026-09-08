import { describe, expect, it } from 'vitest'
import { createSpring, isSettled, snapSpring, stepSpring } from './spring'

function run(s: ReturnType<typeof createSpring>, seconds: number, p: { frequency: number; damping: number }) {
  const dt = 1 / 120
  for (let t = 0; t < seconds; t += dt) stepSpring(s, dt, p)
  return s
}

describe('spring', () => {
  it('converges to its target', () => {
    const s = run(createSpring(0, 1), 3, { frequency: 2, damping: 0.5 })
    expect(s.value).toBeCloseTo(1, 3)
    expect(isSettled(s)).toBe(true)
  })

  it('overshoots when underdamped and not when critically damped', () => {
    const under = createSpring(0, 1)
    const critical = createSpring(0, 1)
    let underMax = 0
    let criticalMax = 0
    const dt = 1 / 120
    for (let t = 0; t < 2; t += dt) {
      underMax = Math.max(underMax, stepSpring(under, dt, { frequency: 2, damping: 0.3 }).value)
      criticalMax = Math.max(criticalMax, stepSpring(critical, dt, { frequency: 2, damping: 1 }).value)
    }
    expect(underMax).toBeGreaterThan(1.05)
    expect(criticalMax).toBeLessThanOrEqual(1.001)
  })

  it('stays stable at a 30 fps step for the fastest system spring', () => {
    const s = createSpring(0, 1)
    for (let i = 0; i < 300; i++) stepSpring(s, 1 / 30, { frequency: 2.3, damping: 0.8 })
    expect(Number.isFinite(s.value)).toBe(true)
    expect(s.value).toBeCloseTo(1, 2)
  })

  it('snaps without residual velocity', () => {
    const s = snapSpring(createSpring(0, 5))
    expect(s.value).toBe(5)
    expect(s.velocity).toBe(0)
  })
})
