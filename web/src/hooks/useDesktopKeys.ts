/**
 * Desktop keyboard shortcuts. Cmd/Ctrl+W closes, Cmd/Ctrl+M shades,
 * Ctrl+` cycles windows, Ctrl/Cmd+Space toggles Spotlight, Escape closes
 * whatever is open. Browsers may keep Cmd+W (and ⌘Space) for themselves.
 */

import { useEffect } from 'react'
import type { WMStore } from '@/desktop/wm/useWM'

export interface DesktopKeyOptions {
  onEscape?(): void
  onToggleSpotlight?(): void
  /** Spotlight is up: the window shortcuts stand down (its bar has the keyboard). */
  suspended?: boolean
}

export function useDesktopKeys(store: WMStore, { onEscape, onToggleSpotlight, suspended = false }: DesktopKeyOptions = {}): void {
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      const mod = event.metaKey || event.ctrlKey
      const { focusedId } = store.getState()
      if (event.key === 'Escape') {
        onEscape?.()
        return
      }
      if (!mod) return
      if (event.code === 'Space') {
        event.preventDefault()
        onToggleSpotlight?.()
        return
      }
      if (suspended) return
      if (event.key === '`') {
        event.preventDefault()
        store.dispatch({ type: 'FOCUS_NEXT' })
      } else if (focusedId && (event.key === 'w' || event.key === 'W')) {
        event.preventDefault()
        store.dispatch({ type: 'CLOSE', id: focusedId })
      } else if (focusedId && (event.key === 'm' || event.key === 'M')) {
        event.preventDefault()
        store.dispatch({ type: 'TOGGLE_SHADE', id: focusedId })
      }
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [store, onEscape, onToggleSpotlight, suspended])
}
