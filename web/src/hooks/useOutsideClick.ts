/** Calls `handler` on any pointerdown outside `refs` while `active`. */

import { useEffect, type RefObject } from 'react'

export function useOutsideClick(
  refs: Array<RefObject<HTMLElement | null>>,
  handler: () => void,
  active = true,
): void {
  useEffect(() => {
    if (!active) return
    const onDown = (event: PointerEvent) => {
      const target = event.target as Node | null
      if (refs.some((ref) => ref.current && target && ref.current.contains(target))) return
      handler()
    }
    document.addEventListener('pointerdown', onDown, true)
    return () => document.removeEventListener('pointerdown', onDown, true)
  }, [refs, handler, active])
}
