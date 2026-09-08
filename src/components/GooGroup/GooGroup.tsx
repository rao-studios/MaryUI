/**
 * GooGroup — makes adjacent controls merge like drops of mercury.
 *
 * Two layers with identical flex layout: a filtered `gooLayer` of blank metal
 * blobs, and an unfiltered `content` layer holding the real children. Only the
 * blobs pass through the SVG goo filter, so text and icons stay crisp. Hovering
 * a child swells its blob (and nudges its neighbors) and the blur+threshold
 * bridges the gap.
 *
 * Children must set `data-goo-index` so hover can be matched to a blob.
 */

import { useCallback, useState, type CSSProperties, type HTMLAttributes, type ReactNode } from 'react'
import { cx } from '@/lib/cx'
import { GOO_FILTER_IDS } from '@/components/SvgDefs'
import styles from './GooGroup.module.css'

export type GooSize = keyof typeof GOO_FILTER_IDS

export interface GooBlobSpec {
  count: number
  /** Blob diameter/height in px. Omit for blobs that stretch to fill (segments). */
  size?: number
  shape?: 'circle' | 'pill' | 'fill'
  /** px of horizontal smear per unit of --lp-vx, applied progressively along the row. */
  smear?: number
}

export interface GooGroupProps extends HTMLAttributes<HTMLDivElement> {
  size?: GooSize
  blobs: GooBlobSpec
  gap: number
  /** Extra classes for every blob, e.g. a tinted metal. */
  blobClassName?: string
  children: ReactNode
}

export function GooGroup({ size = 'sm', blobs, gap, blobClassName, className, style, children, ...rest }: GooGroupProps) {
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
      className={cx(styles.group, blobs.shape === 'fill' && styles.fill, className)}
      style={groupStyle}
      onPointerMove={onPointerMove}
      onPointerLeave={onPointerLeave}
      {...rest}
    >
      <div
        className={styles.gooLayer}
        data-lp-goo-layer=""
        style={{ filter: `url(#${GOO_FILTER_IDS[size]})` }}
        aria-hidden="true"
      >
        {Array.from({ length: blobs.count }, (_, i) => (
          <span
            key={i}
            className={cx(styles.blob, styles[blobs.shape ?? 'circle'], blobClassName)}
            data-hot={hot === i ? '' : undefined}
            data-near={hot !== null && Math.abs(hot - i) === 1 ? '' : undefined}
            style={{ '--lp-goo-i': i } as CSSProperties}
          />
        ))}
      </div>
      <div className={styles.content}>{children}</div>
    </div>
  )
}
