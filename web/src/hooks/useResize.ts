/**
 * Resize gesture from one of the eight window handles. Layout is written
 * straight to the frame each move (one element, unavoidable) and committed to
 * the store on release.
 */

import { useCallback, useRef, type PointerEvent as ReactPointerEvent } from 'react'
import { resizeFromHandle, type Rect, type ResizeHandle, type Size } from '@/lib/geometry'

export interface ResizeOptions {
  getRect(): Rect
  getBounds(): Rect
  minSize: Size
  onStart?(): void
  onUpdate(rect: Rect): void
  onEnd(rect: Rect): void
}

export type ResizeStarter = (handle: ResizeHandle) => (event: ReactPointerEvent<HTMLElement>) => void

export function useResize(options: ResizeOptions): ResizeStarter {
  const optionsRef = useRef(options)
  optionsRef.current = options

  return useCallback<ResizeStarter>(
    (handle) => (event) => {
      if (event.button !== 0) return
      event.preventDefault()
      event.stopPropagation()
      const el = event.currentTarget
      const opts = optionsRef.current
      const start = opts.getRect()
      const bounds = opts.getBounds()
      const startX = event.clientX
      const startY = event.clientY
      let last = start
      opts.onStart?.()

      const onMove = (e: PointerEvent) => {
        last = resizeFromHandle(start, handle, e.clientX - startX, e.clientY - startY, opts.minSize, bounds)
        optionsRef.current.onUpdate(last)
      }
      const onUp = (e: PointerEvent) => {
        el.removeEventListener('pointermove', onMove)
        el.removeEventListener('pointerup', onUp)
        el.removeEventListener('pointercancel', onUp)
        if (el.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId)
        optionsRef.current.onEnd(last)
      }

      el.setPointerCapture(event.pointerId)
      el.addEventListener('pointermove', onMove)
      el.addEventListener('pointerup', onUp)
      el.addEventListener('pointercancel', onUp)
    },
    [],
  )
}
