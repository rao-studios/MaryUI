/**
 * React bindings for the window-manager store.
 *
 * Selectors must return existing references or primitives — never freshly
 * built objects — because `useSyncExternalStore` compares results by identity.
 * The reducer spreads only the records it changes, so `s => s.windows[id]` is
 * stable for untouched windows.
 */

import { createContext, useContext, useSyncExternalStore } from 'react'
import type { Store } from './store'
import type { WMAction } from './actions'
import type { WMState } from './types'

export type WMStore = Store<WMState, WMAction>

export const WMContext = createContext<WMStore | null>(null)

export function useWMStore(): WMStore {
  const store = useContext(WMContext)
  if (!store) throw new Error('useWMStore must be used inside <WMContext.Provider>')
  return store
}

export function useWM<T>(selector: (state: WMState) => T): T {
  const store = useWMStore()
  return useSyncExternalStore(store.subscribe, () => selector(store.getState()), () => selector(store.getState()))
}

export function useWMDispatch(): (action: WMAction) => void {
  return useWMStore().dispatch
}
