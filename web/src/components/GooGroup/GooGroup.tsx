/**
 * GooGroup — makes adjacent controls merge like drops of mercury.
 *
 * Two layers with identical layout: a filtered `gooLayer` of blobs carrying no
 * lighting of their own, and an unfiltered `content` layer holding the real
 * children. Only the blobs pass through the merge filter, so text and icons stay
 * crisp; the filter supplies the silhouette's specular dome and shaded rim (see
 * SvgDefs).
 *
 * Three things make the merge feel like liquid rather than two circles that
 * happen to overlap:
 *  - **Tension.** Hovering the group, or moving the window it lives in, swaps
 *    the filter from `rest` to `flow`, so the beads become willing to bridge.
 *    Both filters are handed to CSS as custom properties and CSS chooses, so a
 *    drag changes tension without React seeing it. `respondsToMotion={false}`
 *    opts a group out of the motion half of that.
 *  - **Attraction.** The neighbours of a hot blob slide *toward* it, so a neck
 *    forms between them instead of a gap closing.
 *  - **Lag.** Each blob answers a beat after the one before it, and the live
 *    smear rides on its own element so no transition can smooth it away.
 *
 * Children must set `data-goo-index` so hover can be matched to a blob.
 */

import { useCallback, useState, type CSSProperties, type HTMLAttributes, type ReactNode } from 'react'
import { cx } from '@/lib/cx'
import { gooFilterId, type GooSize } from '@/components/SvgDefs'
import styles from './GooGroup.module.css'

export type { GooSize }

export interface GooBlobSpec {
  count: number
  /** Blob diameter/height in px. Omit for blobs that stretch to fill (segments). */
  size?: number
  shape?: 'circle' | 'pill' | 'fill'
  /** px of horizontal smear per unit of velocity lag, applied progressively along the row. */
  smear?: number
}

export interface GooGroupProps extends HTMLAttributes<HTMLDivElement> {
  size?: GooSize
  blobs: GooBlobSpec
  gap: number
  /** Extra classes for every blob, e.g. a tinted metal. */
  blobClassName?: string
  /** Contents of blob `i` inside the filtered layer. Omit for plain metal blobs. */
  renderBlob?(index: number): ReactNode
  /** Hold the merge tension open regardless of hover or motion. */
  flowing?: boolean
  /**
   * Whether a moving window smears, stretches and loosens this group. Off for
   * groups that must keep a fixed silhouette while the window travels — the
   * traffic lights, whose beads should behave like a Toggle knob and only ever
   * slosh what is inside them.
   */
  respondsToMotion?: boolean
  children: ReactNode
}

export function GooGroup({
  size = 'sm',
  blobs,
  gap,
  blobClassName,
  renderBlob,
  flowing = false,
  respondsToMotion = true,
  className,
  style,
  children,
  ...rest
}: GooGroupProps) {
  const [hot, setHot] = useState<number | null>(null)

  const onPointerMove = useCallback((event: React.PointerEvent<HTMLDivElement>) => {
    const target = (event.target as HTMLElement).closest<HTMLElement>('[data-goo-index]')
    const index = target ? Number(target.dataset.gooIndex) : NaN
    setHot(Number.isNaN(index) ? null : index)
  }, [])

  const onPointerLeave = useCallback(() => setHot(null), [])

  const groupStyle = {
    ...style,
    '--lp-goo-gap': `${gap}px`,
    '--lp-goo-size': blobs.size ? `${blobs.size}px` : undefined,
    '--lp-goo-smear': `${blobs.smear ?? 0}px`,
  } as CSSProperties

  return (
    <div
      className={cx(
        styles.group,
        blobs.shape === 'fill' && styles.fill,
        flowing && styles.flowing,
        !respondsToMotion && styles.still,
        className,
      )}
      style={groupStyle}
      onPointerMove={onPointerMove}
      onPointerLeave={onPointerLeave}
      {...rest}
    >
      <div
        className={styles.gooLayer}
        data-lp-goo-layer=""
        style={
          {
            '--lp-goo-rest': `url(#${gooFilterId(size, 'rest')})`,
            '--lp-goo-flow': `url(#${gooFilterId(size, 'flow')})`,
          } as CSSProperties
        }
        aria-hidden="true"
      >
        {Array.from({ length: blobs.count }, (_, i) => (
          <span
            key={i}
            className={cx(styles.blob, styles[blobs.shape ?? 'circle'], renderBlob && styles.custom, blobClassName)}
            data-hot={hot === i ? '' : undefined}
            /* -1 to the left of the hot blob, +1 to its right: which way to lean. */
            data-pull={hot !== null && Math.abs(hot - i) === 1 ? Math.sign(hot - i) : undefined}
            style={{ '--lp-goo-i': i } as CSSProperties}
          >
            <span className={styles.blobMotion}>{renderBlob?.(i)}</span>
          </span>
        ))}
      </div>
      <div className={styles.content}>{children}</div>
    </div>
  )
}
