/**
 * LiquidBubble — a bead of glass holding coloured liquid, with a visible
 * surface line across it. The Toggle's knob, the Slider's thumb and the window
 * traffic lights are all the same object at different sizes and tints.
 *
 * The liquid rolls on its own (CSS keyframes) and sloshes with the window it
 * lives in: the engine writes `--lp-slosh` / `--lp-slosh-x` / `--lp-slosh-y` on
 * the window frame and the bead only consumes them. No JavaScript runs here.
 *
 * Anatomy: shell (the glass well) → sloshFrame → liquid.back + liquid.front +
 * meniscus → gloss → glyph.
 */

import type { CSSProperties, ReactNode } from 'react'
import { cx } from '@/lib/cx'
import styles from './LiquidBubble.module.css'

export type BubbleTint = 'close' | 'minimize' | 'zoom' | 'inactive' | 'accent' | 'platinum'

export interface LiquidBubbleProps {
  tint?: BubbleTint
  /** Diameter in px. */
  size?: number
  /** Animation phase offset in seconds so neighbors never roll in lockstep. */
  phase?: number
  /** Fill level 0..1; defaults to the liquid.fill token. */
  fill?: number
  glyph?: ReactNode
  /** Draw only the liquid — for a caller putting it inside a merge filter. */
  liquidOnly?: boolean
  className?: string
  style?: CSSProperties
}

export function LiquidBubble({
  tint = 'platinum',
  size = 12,
  phase = 0,
  fill,
  glyph,
  liquidOnly = false,
  className,
  style,
}: LiquidBubbleProps) {
  const vars = {
    ...style,
    '--lp-bubble-size': `${size}px`,
    /* Unitless: CSS cannot divide by a length, so the slosh offsets scale by this. */
    '--lp-bubble-scale': size / 12,
    '--lp-bubble-phase': `${-phase}s`,
    '--lp-bubble-fill': fill,
  } as CSSProperties

  return (
    <span
      className={cx(styles.shell, styles[tint], liquidOnly && styles.bare, className)}
      style={vars}
      data-lp-bubble=""
      aria-hidden="true"
    >
      <span className={styles.sloshFrame}>
        <span className={cx(styles.liquid, styles.back)} />
        <span className={cx(styles.liquid, styles.front)} />
        <span className={styles.meniscus} />
      </span>
      {liquidOnly ? null : <span className={styles.gloss} />}
      {glyph ? (
        <span className={styles.glyph} data-lp-glyph="">
          {glyph}
        </span>
      ) : null}
    </span>
  )
}
