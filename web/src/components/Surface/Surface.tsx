/**
 * Surface — the brushed platinum every other component sits on.
 *
 * Anatomy: base gradient (from the variant's -top/-bottom tokens) → brush
 * grain (the baked tile under `mix-blend-mode: overlay`) → sheen (an optional
 * highlight band positioned by `--lp-sheen-x`) → children.
 *
 * The brushed metal is one continuous sheet the whole page is cut out of. A
 * window does not carry its grain with it: it uncovers a different part of the
 * sheet as it moves, which is what makes the scratches visibly travel. The
 * motion engine writes the frame's page position into `--lp-grain-x/y`; this
 * component contributes the other half of the sum, `--lp-s-x/y`, which is where
 * this particular surface sits inside that frame. Without it the sheet would
 * restart at every surface, because a background-position is relative to the
 * element's own box.
 *
 * That offset only changes when the layout does, so it is measured on mount and
 * on resize — never per frame.
 */

import { forwardRef, useCallback, useLayoutEffect, useRef, type HTMLAttributes, type MutableRefObject, type ReactNode } from 'react'
import { cx } from '@/lib/cx'
import styles from './Surface.module.css'

export type SurfaceVariant = 'raised' | 'flat' | 'well' | 'bar' | 'titlebar' | 'body'

export interface SurfaceProps extends HTMLAttributes<HTMLDivElement> {
  variant?: SurfaceVariant
  /** Paint the sliding specular highlight. Title bars and the menu bar want it; wells do not. */
  sheen?: boolean
  children?: ReactNode
}

export const Surface = forwardRef<HTMLDivElement, SurfaceProps>(function Surface(
  { variant = 'flat', sheen = false, className, children, ...rest },
  ref,
) {
  const own = useRef<HTMLDivElement | null>(null)

  /* Merge the forwarded ref with the one this component needs for measuring. */
  const setRef = useCallback(
    (node: HTMLDivElement | null) => {
      own.current = node
      if (typeof ref === 'function') ref(node)
      else if (ref) (ref as MutableRefObject<HTMLDivElement | null>).current = node
    },
    [ref],
  )

  useLayoutEffect(() => {
    const el = own.current
    if (!el) return
    const frame = el.closest('[data-lp-frame]') as HTMLElement | null
    /* A surface outside any window — the menu bar, Spotlight — is already in
     * page coordinates and needs no correction. */
    if (!frame) return

    const measure = () => {
      const a = el.getBoundingClientRect()
      const b = frame.getBoundingClientRect()
      el.style.setProperty('--lp-s-x', `${Math.round(a.left - b.left)}px`)
      el.style.setProperty('--lp-s-y', `${Math.round(a.top - b.top)}px`)
    }
    measure()
    const observer = new ResizeObserver(measure)
    observer.observe(el)
    observer.observe(frame)
    return () => observer.disconnect()
  }, [])

  return (
    <div ref={setRef} className={cx(styles.surface, styles[variant], className)} {...rest}>
      <i className={styles.grain} aria-hidden="true" />
      {sheen ? <i className={styles.sheen} aria-hidden="true" /> : null}
      {children}
    </div>
  )
})
