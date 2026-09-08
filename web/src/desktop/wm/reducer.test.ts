import { describe, expect, it } from 'vitest'
import { initialState, reducer } from './reducer'
import type { WMState } from './types'

const bounds = { x: 0, y: 24, w: 1200, h: 776 }

function open(state: WMState, appId = 'finder', rect?: Partial<{ x: number; y: number; w: number; h: number }>) {
  return reducer(state, { type: 'OPEN', spec: { appId, title: appId, rect } })
}

describe('window manager reducer', () => {
  it('opens windows with increasing z and focuses the newest', () => {
    let s = open(initialState(bounds))
    s = open(s, 'gallery')
    expect(s.order).toEqual(['w1', 'w2'])
    expect(s.focusedId).toBe('w2')
    expect(s.windows.w2.z).toBeGreaterThan(s.windows.w1.z)
  })

  it('reuses a singleton window and un-shades it', () => {
    let s = open(initialState(bounds), 'about')
    s = reducer(s, { type: 'TOGGLE_SHADE', id: 'w1' })
    s = reducer(s, { type: 'OPEN', spec: { appId: 'about', title: 'About', singleton: true } })
    expect(s.order).toEqual(['w1'])
    expect(s.windows.w1.state).toBe('normal')
    expect(s.focusedId).toBe('w1')
  })

  it('FOCUS raises with a strictly higher z and keeps other records identical', () => {
    let s = open(open(initialState(bounds)), 'gallery')
    const before = s.windows.w2
    s = reducer(s, { type: 'FOCUS', id: 'w1' })
    expect(s.windows.w1.z).toBeGreaterThan(s.windows.w2.z)
    expect(s.windows.w2).toBe(before)
    expect(reducer(s, { type: 'FOCUS', id: 'w1' })).toBe(s)
  })

  it('MOVE keeps the title bar inside the bounds', () => {
    let s = open(initialState(bounds), 'finder', { x: 100, y: 100, w: 400, h: 300 })
    s = reducer(s, { type: 'MOVE', id: 'w1', x: -900, y: -100 })
    expect(s.windows.w1.rect.y).toBe(24)
    expect(s.windows.w1.rect.x).toBe(-360)
  })

  it('RESIZE enforces the minimum size', () => {
    let s = open(initialState(bounds))
    s = reducer(s, { type: 'RESIZE', id: 'w1', rect: { x: 10, y: 30, w: 10, h: 10 } })
    expect(s.windows.w1.rect.w).toBe(240)
    expect(s.windows.w1.rect.h).toBe(160)
  })

  it('TOGGLE_ZOOM fills the desktop and restores the exact previous rect', () => {
    let s = open(initialState(bounds), 'finder', { x: 100, y: 100, w: 400, h: 300 })
    const original = s.windows.w1.rect
    s = reducer(s, { type: 'TOGGLE_ZOOM', id: 'w1' })
    expect(s.windows.w1.state).toBe('zoomed')
    expect(s.windows.w1.rect).toEqual(bounds)
    expect(s.windows.w1.prevRect).toEqual(original)
    s = reducer(s, { type: 'TOGGLE_ZOOM', id: 'w1' })
    expect(s.windows.w1.state).toBe('normal')
    expect(s.windows.w1.rect).toEqual(original)
    expect(s.windows.w1.prevRect).toBeNull()
  })

  it('TOGGLE_SHADE keeps the rect and toggles back', () => {
    let s = open(initialState(bounds), 'finder', { x: 100, y: 100, w: 400, h: 300 })
    const rect = s.windows.w1.rect
    s = reducer(s, { type: 'TOGGLE_SHADE', id: 'w1' })
    expect(s.windows.w1.state).toBe('shaded')
    expect(s.windows.w1.rect).toBe(rect)
    expect(reducer(s, { type: 'MOVE', id: 'w1', x: 200, y: 200 }).windows.w1.rect).toEqual({ ...rect, x: 200, y: 200 })
    s = reducer(s, { type: 'TOGGLE_SHADE', id: 'w1' })
    expect(s.windows.w1.state).toBe('normal')
  })

  it('CLOSE hands focus to the top-most remaining window', () => {
    let s = open(open(open(initialState(bounds)), 'gallery'), 'about')
    s = reducer(s, { type: 'FOCUS', id: 'w1' })
    s = reducer(s, { type: 'CLOSE', id: 'w1' })
    expect(s.order).toEqual(['w2', 'w3'])
    expect(s.focusedId).toBe('w3')
  })

  it('FOCUS_NEXT cycles to the bottom-most window', () => {
    let s = open(open(initialState(bounds)), 'gallery')
    s = reducer(s, { type: 'FOCUS_NEXT' })
    expect(s.focusedId).toBe('w1')
    s = reducer(s, { type: 'FOCUS_NEXT' })
    expect(s.focusedId).toBe('w2')
  })

  it('SET_BOUNDS refits zoomed windows and re-clamps the rest', () => {
    let s = open(open(initialState(bounds), 'finder', { x: 900, y: 600, w: 400, h: 300 }), 'gallery')
    s = reducer(s, { type: 'TOGGLE_ZOOM', id: 'w2' })
    const small = { x: 0, y: 24, w: 800, h: 500 }
    s = reducer(s, { type: 'SET_BOUNDS', bounds: small })
    expect(s.windows.w2.rect).toEqual(small)
    const r = s.windows.w1.rect
    expect(r.x + r.w).toBeLessThanOrEqual(800)
    expect(r.y + r.h).toBeLessThanOrEqual(524)
  })
})
