/**
 * Pointer-capture drag on an element. Reports deltas from the pointerdown
 * point; the caller decides what they mean. Uses native listeners so the
 * pointer stays captured even when it leaves the element or the window.
 */

import { useEffect, type RefObject } from 'react'

export interface DragHandlers {
  /** Return false to ignore this pointerdown (e.g. it started on a button). */
  onStart?(event: PointerEvent, local: { x: number; y: number }): boolean | void
  onMove?(dx: number, dy: number, event: PointerEvent): void
  onEnd?(dx: number, dy: number, event: PointerEvent): void
}

export function useDrag(ref: RefObject<HTMLElement | null>, handlers: DragHandlers, enabled = true): void {
  useEffect(() => {
    const el = ref.current
    if (!el || !enabled) return

    let startX = 0
    let startY = 0
    let active = false

    const onMove = (event: PointerEvent) => {
      if (!active) return
      handlers.onMove?.(event.clientX - startX, event.clientY - startY, event)
    }

    const onUp = (event: PointerEvent) => {
      if (!active) return
      active = false
      el.removeEventListener('pointermove', onMove)
      el.removeEventListener('pointerup', onUp)
      el.removeEventListener('pointercancel', onUp)
      if (el.hasPointerCapture(event.pointerId)) el.releasePointerCapture(event.pointerId)
      handlers.onEnd?.(event.clientX - startX, event.clientY - startY, event)
    }

    const onDown = (event: PointerEvent) => {
      if (event.button !== 0 || active) return
      const box = el.getBoundingClientRect()
      const local = { x: event.clientX - box.left, y: event.clientY - box.top }
      if (handlers.onStart?.(event, local) === false) return
      event.preventDefault()
      active = true
      startX = event.clientX
      startY = event.clientY
      el.setPointerCapture(event.pointerId)
      el.addEventListener('pointermove', onMove)
      el.addEventListener('pointerup', onUp)
      el.addEventListener('pointercancel', onUp)
    }

    el.addEventListener('pointerdown', onDown)
    return () => {
      el.removeEventListener('pointerdown', onDown)
      el.removeEventListener('pointermove', onMove)
      el.removeEventListener('pointerup', onUp)
      el.removeEventListener('pointercancel', onUp)
    }
  }, [ref, handlers, enabled])
}
