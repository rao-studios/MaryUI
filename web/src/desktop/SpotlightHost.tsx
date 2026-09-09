/**
 * Mounts Spotlight over the desktop: builds the items from the app registry
 * and the window manager, ranks them for the query, and launches what the
 * user picks (an app opens, a window comes forward, Terminal is a no-op here —
 * the web has no processes; the C desktop spawns foot).
 */

import { useMemo, type Dispatch } from 'react'
import { Spotlight } from '@/components/Spotlight'
import { apps, openApp } from './apps/registry'
import { buildSpotlightItems, spotlightResults, type SpotlightAction, type SpotlightState } from './spotlight'
import { useWM, useWMStore } from './wm/useWM'

export interface SpotlightHostProps {
  state: SpotlightState
  dispatch: Dispatch<SpotlightAction>
}

export function SpotlightHost({ state, dispatch }: SpotlightHostProps) {
  const store = useWMStore()
  const wm = useWM((s) => s)
  const items = useMemo(() => buildSpotlightItems(Object.values(apps), wm), [wm])
  const results = useMemo(() => spotlightResults(items, state.query), [items, state.query])
  const selection = Math.min(state.selection, Math.max(0, results.length - 1))

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

  return (
    <Spotlight
      open={state.open}
      query={state.query}
      items={results}
      selection={selection}
      onQueryChange={(query) => dispatch({ type: 'SET_QUERY', query })}
      onHover={(index) => dispatch({ type: 'SELECT', index })}
      onMove={(delta) => dispatch({ type: 'MOVE', delta, count: results.length })}
      onActivate={activate}
      onClose={() => dispatch({ type: 'CLOSE' })}
    />
  )
}
