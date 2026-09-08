import { describe, expect, it } from 'vitest'
import { RESIZE_HANDLES, clampToBounds, constrainRect, flipTransform, resizeFromHandle, type Rect } from './geometry'

const bounds: Rect = { x: 0, y: 24, w: 1200, h: 776 }
const min = { w: 240, h: 160 }

describe('clampToBounds', () => {
  it('never lets the title bar rise above the desktop', () => {
    expect(clampToBounds({ x: 100, y: -50, w: 400, h: 300 }, bounds).y).toBe(24)
  })

  it('keeps at least 40px visible horizontally on both sides', () => {
    expect(clampToBounds({ x: -1000, y: 100, w: 400, h: 300 }, bounds).x).toBe(-360)
    expect(clampToBounds({ x: 5000, y: 100, w: 400, h: 300 }, bounds).x).toBe(1160)
  })

  it('keeps the title bar reachable at the bottom', () => {
    expect(clampToBounds({ x: 0, y: 5000, w: 400, h: 300 }, bounds).y).toBe(24 + 776 - 28)
  })

  it('returns the same reference when nothing changes', () => {
    const rect = { x: 100, y: 100, w: 400, h: 300 }
    expect(clampToBounds(rect, bounds)).toBe(rect)
  })
})

describe('resizeFromHandle', () => {
  const rect: Rect = { x: 100, y: 100, w: 400, h: 300 }

  it('moves only the edges named by the handle', () => {
    expect(resizeFromHandle(rect, 'e', 50, 999, min)).toEqual({ x: 100, y: 100, w: 450, h: 300 })
    expect(resizeFromHandle(rect, 'n', 999, -20, min)).toEqual({ x: 100, y: 80, w: 400, h: 320 })
    expect(resizeFromHandle(rect, 'sw', -10, 10, min)).toEqual({ x: 90, y: 100, w: 410, h: 310 })
  })

  it('enforces the minimum size from every handle', () => {
    for (const handle of RESIZE_HANDLES) {
      const shrunk = resizeFromHandle(rect, handle, handle.includes('w') ? 1000 : -1000, handle.includes('n') ? 1000 : -1000, min)
      expect(shrunk.w).toBeGreaterThanOrEqual(min.w)
      expect(shrunk.h).toBeGreaterThanOrEqual(min.h)
    }
  })

  it('pins the opposite edge while shrinking', () => {
    const shrunk = resizeFromHandle(rect, 'w', 1000, 0, min)
    expect(shrunk.x + shrunk.w).toBe(500)
    expect(shrunk.w).toBe(min.w)
  })

  it('does not push edges outside the bounds', () => {
    const grown = resizeFromHandle(rect, 'nw', -500, -500, min, bounds)
    expect(grown.x).toBe(0)
    expect(grown.y).toBe(24)
    const grownSe = resizeFromHandle(rect, 'se', 5000, 5000, min, bounds)
    expect(grownSe.x + grownSe.w).toBe(1200)
    expect(grownSe.y + grownSe.h).toBe(800)
  })
})

describe('constrainRect', () => {
  it('shrinks and shifts oversize windows into the bounds', () => {
    const r = constrainRect({ x: 900, y: 700, w: 2000, h: 2000 }, bounds, min)
    expect(r).toEqual({ x: 0, y: 24, w: 1200, h: 776 })
  })
})

describe('flipTransform', () => {
  it('maps the destination rect back onto the source', () => {
    const t = flipTransform({ x: 10, y: 20, w: 200, h: 100 }, { x: 0, y: 24, w: 400, h: 400 })
    expect(t).toEqual({ tx: 10, ty: -4, sx: 0.5, sy: 0.25 })
  })
})
