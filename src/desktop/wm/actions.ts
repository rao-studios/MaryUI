/** Every way the window-manager state can change. */

import type { Rect } from '@/lib/geometry'
import type { OpenWindowSpec } from './types'

export type WMAction =
  | { type: 'OPEN'; spec: OpenWindowSpec }
  | { type: 'CLOSE'; id: string }
  | { type: 'FOCUS'; id: string }
  | { type: 'FOCUS_NEXT' }
  | { type: 'MOVE'; id: string; x: number; y: number }
  | { type: 'RESIZE'; id: string; rect: Rect }
  | { type: 'TOGGLE_SHADE'; id: string }
  | { type: 'TOGGLE_ZOOM'; id: string }
  | { type: 'SET_TITLE'; id: string; title: string }
  | { type: 'SET_BOUNDS'; bounds: Rect }
