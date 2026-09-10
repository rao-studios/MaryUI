/**
 * The window manager's pure state machine. Invariants it upholds:
 *  - focusing a window gives it a strictly increasing z and leaves every other
 *    record referentially identical, so unrelated windows never re-render;
 *  - a moved window always keeps its title bar reachable inside the bounds;
 *  - resizing respects the window's minimum size;
 *  - zooming remembers the previous rect and restores it exactly;
 *  - closing hands focus to the top-most remaining window.
 */

import { clampToBounds, constrainRect, fitRect, rectsEqual, type Rect } from '@/lib/geometry'
import { tokens } from '@/tokens/tokens'
import type { WMAction } from './actions'
import type { WMState, WindowRecord } from './types'

/** The one height the reducer needs: how much of a window must stay reachable. */
export const TITLE_HEIGHT = parseFloat(tokens.size.titlebarHeight)
export const DEFAULT_MIN_SIZE = { w: 240, h: 160 }

export function initialState(bounds: Rect): WMState {
  return { windows: {}, order: [], focusedId: null, nextZ: 1, nextId: 1, bounds }
}

export function topMostId(state: WMState, except?: string): string | null {
  let best: WindowRecord | null = null
  for (const id of state.order) {
    if (id === except) continue
    const w = state.windows[id]
    if (!best || w.z > best.z) best = w
  }
  return best?.id ?? null
}

function withWindow(state: WMState, id: string, patch: Partial<WindowRecord>): WMState {
  const current = state.windows[id]
  if (!current) return state
  return { ...state, windows: { ...state.windows, [id]: { ...current, ...patch } } }
}

function raise(state: WMState, id: string): WMState {
  const current = state.windows[id]
  if (!current) return state
  if (state.focusedId === id) return state
  return {
    ...state,
    focusedId: id,
    nextZ: state.nextZ + 1,
    windows: { ...state.windows, [id]: { ...current, z: state.nextZ } },
  }
}

/** Staggers new windows so they never open exactly on top of each other. */
function defaultRect(state: WMState, index: number): Rect {
  const w = Math.min(640, Math.max(DEFAULT_MIN_SIZE.w, state.bounds.w - 80))
  const h = Math.min(440, Math.max(DEFAULT_MIN_SIZE.h, state.bounds.h - 80))
  const offset = (index % 8) * TITLE_HEIGHT
  return { x: state.bounds.x + 60 + offset, y: state.bounds.y + 40 + offset, w, h }
}

export function reducer(state: WMState, action: WMAction): WMState {
  switch (action.type) {
    case 'OPEN': {
      const { spec } = action
      if (spec.singleton) {
        const existing = state.order.find((id) => state.windows[id].appId === spec.appId)
        if (existing) {
          const w = state.windows[existing]
          const shown = w.state === 'shaded' ? withWindow(state, existing, { state: 'normal' }) : state
          return raise(shown, existing)
        }
      }
      const id = `w${state.nextId}`
      const base = defaultRect(state, state.order.length)
      const minSize = spec.minSize ?? DEFAULT_MIN_SIZE
      const rect = constrainRect({ ...base, ...spec.rect }, state.bounds, minSize)
      const record: WindowRecord = {
        id,
        appId: spec.appId,
        title: spec.title,
        rect,
        prevRect: null,
        state: 'normal',
        z: state.nextZ,
        minSize,
        resizable: spec.resizable ?? true,
      }
      return {
        ...state,
        windows: { ...state.windows, [id]: record },
        order: [...state.order, id],
        focusedId: id,
        nextZ: state.nextZ + 1,
        nextId: state.nextId + 1,
      }
    }

    case 'CLOSE': {
      if (!state.windows[action.id]) return state
      const windows = { ...state.windows }
      delete windows[action.id]
      const next = { ...state, windows, order: state.order.filter((id) => id !== action.id) }
      return { ...next, focusedId: state.focusedId === action.id ? topMostId(next) : state.focusedId }
    }

    case 'FOCUS':
      return raise(state, action.id)

    case 'FOCUS_NEXT': {
      if (state.order.length < 2) return state
      const sorted = [...state.order].sort((a, b) => state.windows[a].z - state.windows[b].z)
      return raise(state, sorted[0])
    }

    case 'MOVE': {
      const w = state.windows[action.id]
      if (!w || w.state === 'zoomed') return state
      const rect = clampToBounds({ ...w.rect, x: action.x, y: action.y }, state.bounds, { titleHeight: TITLE_HEIGHT })
      return rectsEqual(rect, w.rect) ? state : withWindow(state, action.id, { rect })
    }

    case 'RESIZE': {
      const w = state.windows[action.id]
      if (!w || !w.resizable || w.state !== 'normal') return state
      const rect = {
        ...action.rect,
        w: Math.max(action.rect.w, w.minSize.w),
        h: Math.max(action.rect.h, w.minSize.h),
      }
      return rectsEqual(rect, w.rect) ? state : withWindow(state, action.id, { rect })
    }

    case 'TOGGLE_SHADE': {
      const w = state.windows[action.id]
      if (!w) return state
      if (w.state === 'shaded') return raise(withWindow(state, action.id, { state: 'normal' }), action.id)
      if (w.state === 'zoomed') {
        return withWindow(state, action.id, { state: 'shaded', rect: w.prevRect ?? w.rect, prevRect: null })
      }
      return withWindow(state, action.id, { state: 'shaded' })
    }

    case 'TOGGLE_ZOOM': {
      const w = state.windows[action.id]
      if (!w) return state
      if (w.state === 'zoomed') {
        const rect = w.prevRect ? constrainRect(w.prevRect, state.bounds, w.minSize) : w.rect
        return raise(withWindow(state, action.id, { state: 'normal', rect, prevRect: null }), action.id)
      }
      return raise(
        withWindow(state, action.id, { state: 'zoomed', prevRect: w.rect, rect: fitRect(state.bounds) }),
        action.id,
      )
    }

    case 'SET_TITLE':
      return withWindow(state, action.id, { title: action.title })

    case 'SET_BOUNDS': {
      if (rectsEqual(action.bounds, state.bounds)) return state
      let changed = false
      const windows: Record<string, WindowRecord> = {}
      for (const id of state.order) {
        const w = state.windows[id]
        const rect =
          w.state === 'zoomed'
            ? fitRect(action.bounds)
            : clampToBounds(constrainRect(w.rect, action.bounds, w.minSize), action.bounds, { titleHeight: TITLE_HEIGHT })
        if (rect !== w.rect && !rectsEqual(rect, w.rect)) {
          windows[id] = { ...w, rect }
          changed = true
        } else {
          windows[id] = w
        }
      }
      return { ...state, bounds: action.bounds, windows: changed ? windows : state.windows }
    }
  }
}
