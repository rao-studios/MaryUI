/** Window-manager state shapes. Pure data; the reducer is the only writer. */

import type { Rect, Size } from '@/lib/geometry'

export type WindowState = 'normal' | 'shaded' | 'zoomed'

export interface WindowRecord {
  id: string
  appId: string
  title: string
  /** Layout when `state` is normal or shaded; the pre-zoom rect while zoomed is in `prevRect`. */
  rect: Rect
  prevRect: Rect | null
  state: WindowState
  z: number
  minSize: Size
  resizable: boolean
}

export interface WMState {
  windows: Record<string, WindowRecord>
  /** Insertion order, so the DOM never reorders (and never remounts) on focus. */
  order: string[]
  focusedId: string | null
  nextZ: number
  nextId: number
  /** The desktop area: viewport minus the menu bar. */
  bounds: Rect
}

export interface OpenWindowSpec {
  appId: string
  title: string
  rect?: Partial<Rect>
  minSize?: Size
  resizable?: boolean
  /** Reuse an existing window of this app instead of opening a second one. */
  singleton?: boolean
}
