/** Creates a window's `WindowMotion` once and attaches it to the frame/chrome elements. */

import { useLayoutEffect, useRef, type RefObject } from 'react'
import { WindowMotion } from '@/lib/motionEngine'

export function useMotionTarget(
  frameRef: RefObject<HTMLElement | null>,
  chromeRef: RefObject<HTMLElement | null>,
): WindowMotion {
  const motionRef = useRef<WindowMotion | null>(null)
  motionRef.current ??= new WindowMotion()

  useLayoutEffect(() => {
    const motion = motionRef.current!
    const frame = frameRef.current
    const chrome = chromeRef.current
    if (!frame || !chrome) return
    motion.attach({ frame, chrome })
    return () => motion.detach()
  }, [frameRef, chromeRef])

  return motionRef.current
}
