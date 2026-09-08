/**
 * Desktop keyboard shortcuts. Cmd/Ctrl+W closes, Cmd/Ctrl+M shades,
 * Ctrl+` cycles windows. Browsers may keep Cmd+W for themselves.
 */

import { useEffect } from 'react'
import type { WMStore } from '@/desktop/wm/useWM'

export function useDesktopKeys(store: WMStore, onEscape?: () => void): void {
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      const mod = event.metaKey || event.ctrlKey
      const { focusedId } = store.getState()
      if (event.key === 'Escape') {
        onEscape?.()
        return
      }
      if (!mod) return
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
  }, [store, onEscape])
}
