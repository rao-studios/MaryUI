/**
 * Spotlight's model: open/closed, the query, the selection, and the pure
 * functions that turn the desktop's apps and windows into items and rank
 * them. No React, no DOM — the C desktop ports this file as lp_spotlight.c
 * and runs the same test cases.
 */

import type { IconName } from '@/components/Icon'
import type { AppDefinition } from '@/desktop/apps/registry'
import type { WMState } from '@/desktop/wm/types'

export type SpotlightItemKind = 'app' | 'window' | 'command'

export interface SpotlightItem {
  kind: SpotlightItemKind
  /** App id, window id, or command id. */
  id: string
  title: string
  /** "Application", "Window · Finder", "Command". */
  subtitle: string
  icon: IconName
  /** An app with a window open (the dock's dot). */
  running: boolean
}

export interface SpotlightState {
  open: boolean
  query: string
  /** Index into the results. */
  selection: number
}

export type SpotlightAction =
  | { type: 'OPEN' }
  | { type: 'CLOSE' }
  | { type: 'TOGGLE' }
  | { type: 'SET_QUERY'; query: string }
  | { type: 'MOVE'; delta: number; count: number }
  | { type: 'SELECT'; index: number }

export const SPOTLIGHT_MAX_RESULTS = 8
/** The bar's centre sits at this fraction of the desktop height. */
export const SPOTLIGHT_Y_FRACTION = 0.38
export const SPOTLIGHT_PLACEHOLDER = 'Say “Hey Mary” or type something…'

export const initialSpotlight: SpotlightState = { open: false, query: '', selection: 0 }

export function spotlightReducer(state: SpotlightState, action: SpotlightAction): SpotlightState {
  switch (action.type) {
    case 'OPEN':
      return { open: true, query: '', selection: 0 }
    case 'CLOSE':
      return state.open ? { ...state, open: false } : state
    case 'TOGGLE':
      return state.open ? { ...state, open: false } : { open: true, query: '', selection: 0 }
    case 'SET_QUERY':
      return { ...state, query: action.query, selection: 0 }
    case 'MOVE': {
      if (action.count <= 0) return state.selection === 0 ? state : { ...state, selection: 0 }
      const selection = (((state.selection + action.delta) % action.count) + action.count) % action.count
      return selection === state.selection ? state : { ...state, selection }
    }
    case 'SELECT':
      return action.index === state.selection ? state : { ...state, selection: action.index }
  }
}

export function isBlankQuery(query: string): boolean {
  return query.trim() === ''
}

/** Every launchable thing: the registered apps (hidden ones too), the Terminal command, then the open windows. */
export function buildSpotlightItems(apps: AppDefinition[], wm: WMState): SpotlightItem[] {
  const windows = wm.order.map((id) => wm.windows[id]).filter((w) => w !== undefined)
  const items: SpotlightItem[] = apps.map((app) => ({
    kind: 'app',
    id: app.id,
    title: app.name ?? app.title,
    subtitle: 'Application',
    icon: app.icon ?? 'document',
    running: windows.some((w) => w.appId === app.id),
  }))
  items.push({
    kind: 'command',
    id: 'terminal',
    title: 'Terminal',
    subtitle: 'Command',
    icon: 'terminal',
    running: windows.some((w) => !apps.some((app) => app.id === w.appId)),
  })
  for (const w of windows) {
    const app = apps.find((a) => a.id === w.appId)
    items.push({
      kind: 'window',
      id: w.id,
      title: w.title,
      subtitle: `Window · ${app ? app.name ?? app.title : w.appId}`,
      icon: app ? app.icon ?? 'document' : 'terminal',
      running: false,
    })
  }
  return items
}

/** 0: the title starts with q · 1: a word of the title starts with q · 2: q appears inside · -1: no match. */
function rank(title: string, q: string): number {
  const t = title.toLowerCase()
  if (t.startsWith(q)) return 0
  for (let i = 1; i < t.length; i++) {
    if (/\s/.test(t[i - 1]) && t.startsWith(q, i)) return 1
  }
  return t.includes(q) ? 2 : -1
}

/**
 * The dock (a blank query: every non-window item) or the ranked matches —
 * title prefix, then a word prefix, then a substring — stable, at most
 * SPOTLIGHT_MAX_RESULTS.
 */
export function spotlightResults(items: SpotlightItem[], query: string): SpotlightItem[] {
  const q = query.trim().toLowerCase()
  if (!q) return items.filter((item) => item.kind !== 'window').slice(0, SPOTLIGHT_MAX_RESULTS)
  const out: SpotlightItem[] = []
  for (let r = 0; r <= 2 && out.length < SPOTLIGHT_MAX_RESULTS; r++) {
    for (const item of items) {
      if (out.length >= SPOTLIGHT_MAX_RESULTS) break
      if (rank(item.title, q) === r) out.push(item)
    }
  }
  return out
}
