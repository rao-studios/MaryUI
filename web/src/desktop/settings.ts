/**
 * Desktop preferences: accent, goo, wallpaper source, reduced motion, and the
 * Gallery's debug switches. A micro store mirrored onto <html data-*> so CSS can
 * react, and persisted to localStorage so the inline script in index.html can
 * restore it before paint.
 */

import { useSyncExternalStore } from 'react'

export type Accent = 'blue' | 'graphite'
export type WallpaperMode = 'molten' | 'procedural' | 'raster'
export type MoltenTone = 'platinum' | 'faithful'

export interface Settings {
  accent: Accent
  goo: boolean
  wallpaper: WallpaperMode
  /** Which grade the molten wallpaper wears. */
  moltenTone: MoltenTone
  reducedMotion: boolean
  /** Draw the painted hairline across each bead's waterline. Debug only. */
  crestLine: boolean
  /** Strip the merge filter so the raw blobs behind it are visible. */
  showMergeLayer: boolean
  /** Pause the ambient wave, to judge the slosh on its own. */
  freezeLiquid: boolean
}

const KEY = 'lp-settings'

function read(): Settings {
  const defaults: Settings = {
    accent: 'blue',
    goo: true,
    wallpaper: 'molten',
    moltenTone: 'platinum',
    reducedMotion: false,
    crestLine: false,
    showMergeLayer: false,
    freezeLiquid: false,
  }
  try {
    return { ...defaults, ...JSON.parse(localStorage.getItem(KEY) ?? '{}') }
  } catch {
    return defaults
  }
}

let state = read()
const listeners = new Set<() => void>()

function apply(s: Settings): void {
  const html = document.documentElement
  html.dataset.accent = s.accent
  html.dataset.goo = s.goo ? 'on' : 'off'
  html.dataset.wallpaper = s.wallpaper
  html.dataset.moltenTone = s.moltenTone
  if (s.reducedMotion) html.dataset.reducedMotion = 'on'
  else delete html.dataset.reducedMotion
  toggleFlag(html, 'crest', s.crestLine)
  toggleFlag(html, 'mergeLayer', s.showMergeLayer)
  toggleFlag(html, 'freezeLiquid', s.freezeLiquid)
}

/** Debug flags are absent rather than 'off', so CSS can match on presence alone. */
function toggleFlag(html: HTMLElement, name: string, on: boolean): void {
  if (on) html.dataset[name] = 'on'
  else delete html.dataset[name]
}

export function getSettings(): Settings {
  return state
}

export function updateSettings(patch: Partial<Settings>): void {
  state = { ...state, ...patch }
  apply(state)
  try {
    localStorage.setItem(KEY, JSON.stringify(state))
  } catch {
    /* private mode; the in-memory state still applies */
  }
  for (const l of listeners) l()
}

export function subscribeSettings(listener: () => void): () => void {
  listeners.add(listener)
  return () => listeners.delete(listener)
}

export function useSettings(): Settings {
  return useSyncExternalStore(subscribeSettings, getSettings, getSettings)
}

if (typeof document !== 'undefined') apply(state)
