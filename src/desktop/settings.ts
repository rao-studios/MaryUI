/**
 * Desktop preferences: accent, goo, wallpaper source, reduced motion. A micro
 * store mirrored onto <html data-*> so CSS can react, and persisted to
 * localStorage so the inline script in index.html can restore it before paint.
 */

import { useSyncExternalStore } from 'react'

export type Accent = 'blue' | 'graphite'
export type WallpaperMode = 'procedural' | 'raster'

export interface Settings {
  accent: Accent
  goo: boolean
  wallpaper: WallpaperMode
  reducedMotion: boolean
}

const KEY = 'lp-settings'

function read(): Settings {
  const defaults: Settings = { accent: 'blue', goo: true, wallpaper: 'procedural', reducedMotion: false }
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
  if (s.reducedMotion) html.dataset.reducedMotion = 'on'
  else delete html.dataset.reducedMotion
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
