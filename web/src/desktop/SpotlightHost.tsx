/**
 * Mounts Spotlight over the desktop: builds the items from the app registry
 * and the window manager, ranks them for the query, and launches what the
 * user picks (an app opens, a window comes forward, Terminal is a no-op here —
 * the web has no processes; the C desktop spawns foot). Also builds the app
 * commands (née the menu bar) Spotlight shows inline in its dock state, and
 * the "searching <app>" context for whichever window is focused.
 */

import { useMemo, type Dispatch } from 'react'
import { Spotlight } from '@/components/Spotlight'
import { apps, openApp } from './apps/registry'
import { buildMenus, type MenuEntry } from './menus'
import { useSettings } from './settings'
import { buildSpotlightItems, spotlightResults, type SpotlightAction, type SpotlightState } from './spotlight'
import { useWM, useWMStore } from './wm/useWM'

export interface SpotlightHostProps {
  state: SpotlightState
  dispatch: Dispatch<SpotlightAction>
}

export function SpotlightHost({ state, dispatch }: SpotlightHostProps) {
  const store = useWMStore()
  const wm = useWM((s) => s)
  const settings = useSettings()
  const items = useMemo(() => buildSpotlightItems(Object.values(apps), wm), [wm])
  const results = useMemo(() => spotlightResults(items, state.query), [items, state.query])
  const selection = Math.min(state.selection, Math.max(0, results.length - 1))
  const menus = useMemo(() => buildMenus(store, wm, settings), [store, wm, settings])

  const focused = wm.focusedId ? wm.windows[wm.focusedId] : null
  const focusedApp = focused ? apps[focused.appId] : undefined
  const context = focused ? { icon: focusedApp?.icon ?? 'document', name: focusedApp?.name ?? focusedApp?.title ?? focused.title } : null

  const activate = (index: number) => {
    const item = results[index]
    if (item) {
      if (item.kind === 'app') openApp(store, item.id)
      else if (item.kind === 'window') {
        const record = store.getState().windows[item.id]
        if (record?.state === 'shaded') store.dispatch({ type: 'TOGGLE_SHADE', id: item.id })
        store.dispatch({ type: 'FOCUS', id: item.id })
      }
    }
    dispatch({ type: 'CLOSE' })
  }

  const activateCommand = (entry: MenuEntry) => {
    if (!('separator' in entry)) entry.onSelect?.()
    dispatch({ type: 'CLOSE' })
  }

  return (
    <Spotlight
      open={state.open}
      query={state.query}
      items={results}
      selection={selection}
      menus={menus}
      context={context}
      onQueryChange={(query) => dispatch({ type: 'SET_QUERY', query })}
      onHover={(index) => dispatch({ type: 'SELECT', index })}
      onMove={(delta) => dispatch({ type: 'MOVE', delta, count: results.length })}
      onActivate={activate}
      onActivateCommand={activateCommand}
      onClose={() => dispatch({ type: 'CLOSE' })}
    />
  )
}
