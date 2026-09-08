/** Identity-stable selectors for `useWM`. */

import type { WMState, WindowRecord } from './types'

export const selectOrder = (s: WMState): string[] => s.order
export const selectFocusedId = (s: WMState): string | null => s.focusedId
export const selectBounds = (s: WMState) => s.bounds
export const selectWindow = (id: string) => (s: WMState): WindowRecord | undefined => s.windows[id]
export const selectIsFocused = (id: string) => (s: WMState): boolean => s.focusedId === id
