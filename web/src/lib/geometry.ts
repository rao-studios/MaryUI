/**
 * Rect math for the window manager: clamping windows to the desktop, resizing
 * from any of the eight handles, and the FLIP transform that lets a window
 * appear to travel between two rects using only `transform`.
 *
 * Pure and framework-free so the reducer, the hooks, and the tests share it.
 */

export interface Rect {
  x: number
  y: number
  w: number
  h: number
}

export interface Size {
  w: number
  h: number
}

export type ResizeHandle = 'n' | 'ne' | 'e' | 'se' | 's' | 'sw' | 'w' | 'nw'

export const RESIZE_HANDLES: readonly ResizeHandle[] = ['n', 'ne', 'e', 'se', 's', 'sw', 'w', 'nw']

export function clamp(n: number, lo: number, hi: number): number {
  return n < lo ? lo : n > hi ? hi : n
}

export function rectsEqual(a: Rect, b: Rect): boolean {
  return a.x === b.x && a.y === b.y && a.w === b.w && a.h === b.h
}

export interface ClampOptions {
  /** Horizontal pixels of the window that must remain inside the bounds. */
  minVisible?: number
  /** Height of the title bar; it must stay reachable at the bottom edge. */
  titleHeight?: number
}

/**
 * Keeps a window grabbable: its top edge never rises above the desktop (so the
 * title bar can't hide under the menu bar), at least `minVisible` px stay on
 * screen horizontally, and the title bar never sinks below the bottom edge.
 */
export function clampToBounds(rect: Rect, bounds: Rect, opts: ClampOptions = {}): Rect {
  const minVisible = opts.minVisible ?? 40
  const titleHeight = opts.titleHeight ?? 28
  const x = clamp(rect.x, bounds.x - rect.w + minVisible, bounds.x + bounds.w - minVisible)
  const y = clamp(rect.y, bounds.y, Math.max(bounds.y, bounds.y + bounds.h - titleHeight))
  return x === rect.x && y === rect.y ? rect : { ...rect, x, y }
}

/**
 * Resizes `rect` by dragging `handle` by (dx, dy). The opposite edge stays
 * pinned, minimum size is enforced, and no edge leaves `bounds` when given.
 */
export function resizeFromHandle(
  rect: Rect,
  handle: ResizeHandle,
  dx: number,
  dy: number,
  minSize: Size,
  bounds?: Rect,
): Rect {
  let left = rect.x
  let top = rect.y
  let right = rect.x + rect.w
  let bottom = rect.y + rect.h

  if (handle.includes('w')) left = Math.min(left + dx, right - minSize.w)
  if (handle.includes('e')) right = Math.max(right + dx, left + minSize.w)
  if (handle.includes('n')) top = Math.min(top + dy, bottom - minSize.h)
  if (handle.includes('s')) bottom = Math.max(bottom + dy, top + minSize.h)

  if (bounds) {
    left = Math.max(left, bounds.x)
    top = Math.max(top, bounds.y)
    right = Math.min(right, bounds.x + bounds.w)
    bottom = Math.min(bottom, bounds.y + bounds.h)
    // Re-assert the minimum after clamping, growing away from the clamped edge.
    if (right - left < minSize.w) {
      if (handle.includes('w')) left = right - minSize.w
      else right = left + minSize.w
    }
    if (bottom - top < minSize.h) {
      if (handle.includes('n')) top = bottom - minSize.h
      else bottom = top + minSize.h
    }
  }

  return { x: left, y: top, w: right - left, h: bottom - top }
}

/** The rect a zoomed window occupies: the whole desktop. */
export function fitRect(bounds: Rect): Rect {
  return { ...bounds }
}

/** Shrinks a rect to fit inside `bounds`, keeping its position when possible. */
export function constrainRect(rect: Rect, bounds: Rect, minSize: Size): Rect {
  const w = clamp(rect.w, Math.min(minSize.w, bounds.w), bounds.w)
  const h = clamp(rect.h, Math.min(minSize.h, bounds.h), bounds.h)
  const x = clamp(rect.x, bounds.x, Math.max(bounds.x, bounds.x + bounds.w - w))
  const y = clamp(rect.y, bounds.y, Math.max(bounds.y, bounds.y + bounds.h - h))
  const next = { x, y, w, h }
  return rectsEqual(next, rect) ? rect : next
}

export interface FlipTransform {
  tx: number
  ty: number
  sx: number
  sy: number
}

/**
 * With `transform-origin: 0 0`, applying this transform to an element laid out
 * at `to` makes it appear exactly at `from`. Springing it to identity animates
 * the move without touching layout.
 */
export function flipTransform(from: Rect, to: Rect): FlipTransform {
  return {
    tx: from.x - to.x,
    ty: from.y - to.y,
    sx: to.w === 0 ? 1 : from.w / to.w,
    sy: to.h === 0 ? 1 : from.h / to.h,
  }
}

export function rectCenter(rect: Rect): { x: number; y: number } {
  return { x: rect.x + rect.w / 2, y: rect.y + rect.h / 2 }
}
