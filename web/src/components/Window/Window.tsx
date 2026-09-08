/**
 * Window — a draggable, resizable, shadeable, zoomable brushed-platinum frame.
 *
 * Two elements share the work. The **frame** is React-owned: its left/top/
 * width/height come from the store, and the motion engine writes the `--lp-*`
 * motion variables onto it. The **chrome** inside is engine-owned: only its
 * `transform` changes, for the drag delta, the jelly deformation, and the FLIP
 * flight when zooming. React never renders during a drag.
 */

import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState, type CSSProperties, type ReactNode } from 'react'
import { Surface } from '@/components/Surface'
import { TitleBar } from '@/components/TitleBar'
import { useDrag, type DragHandlers } from '@/hooks/useDrag'
import { useMotionTarget } from '@/hooks/useMotionTarget'
import { useResize } from '@/hooks/useResize'
import { clampToBounds, type Rect } from '@/lib/geometry'
import { cx } from '@/lib/cx'
import { TITLE_HEIGHT } from '@/desktop/wm/reducer'
import { selectIsFocused, selectWindow } from '@/desktop/wm/selectors'
import { useWM, useWMStore } from '@/desktop/wm/useWM'
import { tokens } from '@/tokens/tokens'
import { ResizeHandles } from './ResizeHandles'
import styles from './Window.module.css'

export interface WindowProps {
  id: string
  children: ReactNode
}

const Z_BASE = tokens.z.windows
const CLOSE_MS = parseFloat(tokens.motion.fast) + 80
const SHADE_MS = parseFloat(tokens.motion.slow) + 80

function applyRect(el: HTMLElement | null, rect: Rect, height = rect.h): void {
  if (!el) return
  el.style.left = `${rect.x}px`
  el.style.top = `${rect.y}px`
  el.style.width = `${rect.w}px`
  el.style.height = `${height}px`
}

function liveRect(el: HTMLElement): Rect {
  return { x: el.offsetLeft, y: el.offsetTop, w: el.offsetWidth, h: el.offsetHeight }
}

export function Window({ id, children }: WindowProps) {
  const store = useWMStore()
  const record = useWM(selectWindow(id))
  const focused = useWM(selectIsFocused(id))

  const frameRef = useRef<HTMLDivElement>(null)
  const chromeRef = useRef<HTMLDivElement>(null)
  const titleRef = useRef<HTMLDivElement>(null)
  const motion = useMotionTarget(frameRef, chromeRef)

  const [closing, setClosing] = useState(false)
  const [shading, setShading] = useState(false)
  const [resizing, setResizing] = useState(false)
  const pendingFlip = useRef<Rect | null>(null)

  const focus = useCallback(() => {
    if (store.getState().focusedId !== id) store.dispatch({ type: 'FOCUS', id })
  }, [store, id])

  // MARK: - Drag

  const dragHandlers = useMemo<DragHandlers>(
    () => ({
      onStart(event, local) {
        const target = event.target as HTMLElement
        if (target.closest('button, [data-no-drag]')) return false
        const w = store.getState().windows[id]
        if (!w || w.state === 'zoomed') return false
        focus()
        motion.beginDrag(local.x, local.y)
      },
      onMove(dx, dy, event) {
        const { windows, bounds } = store.getState()
        const w = windows[id]
        if (!w) return
        const next = clampToBounds({ ...w.rect, x: w.rect.x + dx, y: w.rect.y + dy }, bounds, { titleHeight: TITLE_HEIGHT })
        motion.moveDrag(next.x - w.rect.x, next.y - w.rect.y, event.timeStamp, event.clientX, event.clientY)
      },
      onEnd(dx, dy) {
        const { windows, bounds } = store.getState()
        const w = windows[id]
        if (!w) return
        const next = clampToBounds({ ...w.rect, x: w.rect.x + dx, y: w.rect.y + dy }, bounds, { titleHeight: TITLE_HEIGHT })
        // Commit layout synchronously, zero the delta, then tell the store — no flicker on release.
        const frame = frameRef.current
        if (frame) {
          frame.style.left = `${next.x}px`
          frame.style.top = `${next.y}px`
        }
        motion.endDrag()
        store.dispatch({ type: 'MOVE', id, x: next.x, y: next.y })
      },
    }),
    [store, id, motion, focus],
  )
  useDrag(titleRef, dragHandlers)

  // MARK: - Resize

  const startResize = useResize({
    getRect: () => store.getState().windows[id]?.rect ?? { x: 0, y: 0, w: 0, h: 0 },
    getBounds: () => store.getState().bounds,
    minSize: record?.minSize ?? { w: 240, h: 160 },
    onStart() {
      focus()
      setResizing(true)
      motion.beginResize()
    },
    onUpdate(rect) {
      applyRect(frameRef.current, rect)
      motion.nudge()
    },
    onEnd(rect) {
      applyRect(frameRef.current, rect)
      motion.endResize()
      setResizing(false)
      store.dispatch({ type: 'RESIZE', id, rect })
    },
  })

  // MARK: - State transitions

  const toggleZoom = useCallback(() => {
    const frame = frameRef.current
    if (frame) pendingFlip.current = liveRect(frame)
    store.dispatch({ type: 'TOGGLE_ZOOM', id })
  }, [store, id])

  const toggleShade = useCallback(() => {
    setShading(true)
    store.dispatch({ type: 'TOGGLE_SHADE', id })
  }, [store, id])

  const requestClose = useCallback(() => setClosing(true), [])

  useLayoutEffect(() => {
    const from = pendingFlip.current
    const frame = frameRef.current
    if (!from || !frame) return
    pendingFlip.current = null
    motion.flyFrom(from, liveRect(frame))
  }, [record?.rect, record?.state, motion])

  useEffect(() => {
    if (!shading) return
    const t = setTimeout(() => setShading(false), SHADE_MS)
    return () => clearTimeout(t)
  }, [shading])

  useEffect(() => {
    if (!closing) return
    const t = setTimeout(() => store.dispatch({ type: 'CLOSE', id }), CLOSE_MS)
    return () => clearTimeout(t)
  }, [closing, store, id])

  useEffect(() => {
    motion.nudge()
  }, [record?.rect, record?.state, motion])

  if (!record) return null

  const shaded = record.state === 'shaded'
  const frameStyle: CSSProperties = {
    left: record.rect.x,
    top: record.rect.y,
    width: record.rect.w,
    height: shaded ? TITLE_HEIGHT : record.rect.h,
    zIndex: Z_BASE + record.z,
  }

  return (
    <div
      ref={frameRef}
      className={styles.frame}
      style={frameStyle}
      data-focused={focused}
      data-state={record.state}
      data-anim={shading ? 'shade' : undefined}
      data-resizing={resizing ? '' : undefined}
      data-closing={closing ? '' : undefined}
      role="dialog"
      aria-label={record.title}
      onPointerDownCapture={focus}
    >
      <Surface ref={chromeRef} variant="flat" className={styles.chrome}>
        <TitleBar
          ref={titleRef}
          title={record.title}
          active={focused}
          shaded={shaded}
          zoomed={record.state === 'zoomed'}
          onClose={requestClose}
          onShade={toggleShade}
          onZoom={toggleZoom}
          onDoubleClick={toggleZoom}
          className={styles.titleBar}
        />
        <div className={cx(styles.body)}>{children}</div>
      </Surface>
      {record.resizable && record.state === 'normal' ? <ResizeHandles onStart={startResize} /> : null}
    </div>
  )
}
