/**
 * LiquidBubble — a well of colored liquid behind glass. The liquid swirls on
 * its own (CSS keyframes) and tilts with the window it lives in (the engine
 * writes `--lp-slosh` / `--lp-slosh-y` on the window frame; the bubble only
 * consumes them). No JavaScript runs here.
 *
 * Anatomy: shell (the well) → sloshFrame → liquid.back + liquid.front → gloss → glyph.
 */

import type { CSSProperties, ReactNode } from 'react'
import { cx } from '@/lib/cx'
import styles from './LiquidBubble.module.css'

export type BubbleTint = 'close' | 'minimize' | 'zoom' | 'inactive' | 'accent' | 'platinum'

export interface LiquidBubbleProps {
  tint?: BubbleTint
  /** Diameter in px. */
  size?: number
  /** Animation phase offset in seconds so neighbors never swirl in lockstep. */
  phase?: number
  /** Fill level 0..1; defaults to the liquid.fill token. */
  fill?: number
  glyph?: ReactNode
  className?: string
  style?: CSSProperties
}

export function LiquidBubble({ tint = 'platinum', size = 12, phase = 0, fill, glyph, className, style }: LiquidBubbleProps) {
  const vars = {
    ...style,
    '--lp-bubble-size': `${size}px`,
    '--lp-bubble-phase': `${-phase}s`,
    '--lp-bubble-fill': fill,
  } as CSSProperties

  return (
    <span className={cx(styles.shell, styles[tint], className)} style={vars} data-lp-bubble="" aria-hidden="true">
      <span className={styles.sloshFrame}>
        <span className={cx(styles.liquid, styles.back)} />
        <span className={cx(styles.liquid, styles.front)} />
      </span>
      <span className={styles.gloss} />
      {glyph ? <span className={styles.glyph} data-lp-glyph="">{glyph}</span> : null}
    </span>
  )
}
