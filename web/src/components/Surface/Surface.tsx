/**
 * Surface — the brushed platinum every other component sits on.
 *
 * Anatomy: base gradient (from the variant's -top/-bottom tokens) → brush
 * grain (the baked tile under `mix-blend-mode: overlay`) → sheen (an optional
 * highlight band positioned by `--lp-sheen-x`) → children.
 *
 * The grain is a real element rather than a pseudo-element so it can be
 * transformed: the engine writes `--lp-grain-x/y`, and the metal skin drags a
 * few px behind the frame and springs back, so the hairline scratches visibly
 * slide under the light as a window moves.
 */

import { forwardRef, type HTMLAttributes, type ReactNode } from 'react'
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
  return (
    <div ref={ref} className={cx(styles.surface, styles[variant], className)} {...rest}>
      <i className={styles.grain} aria-hidden="true" />
      {sheen ? <i className={styles.sheen} aria-hidden="true" /> : null}
      {children}
    </div>
  )
})
