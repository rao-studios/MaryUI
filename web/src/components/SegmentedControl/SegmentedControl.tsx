/**
 * SegmentedControl — a well holding a sliding platinum thumb. The thumb and a
 * hover blob live in a merge-filtered layer, so hovering the segment next to
 * the selection necks a bead of metal toward it before the thumb snaps over.
 * Hovering the track loosens the tension, exactly as it does in GooGroup.
 */

import { useState, type CSSProperties, type ReactNode } from 'react'
import { gooFilterId } from '@/components/SvgDefs'
import { cx } from '@/lib/cx'
import styles from './SegmentedControl.module.css'

export interface SegmentOption<T extends string> {
  value: T
  label?: ReactNode
  icon?: ReactNode
  ariaLabel?: string
}

export interface SegmentedControlProps<T extends string> {
  options: SegmentOption<T>[]
  value: T
  onChange(value: T): void
  size?: 'sm' | 'md'
  className?: string
  'aria-label'?: string
}

export function SegmentedControl<T extends string>({ options, value, onChange, size = 'md', className, ...aria }: SegmentedControlProps<T>) {
  const [hot, setHot] = useState<number | null>(null)
  const index = Math.max(0, options.findIndex((o) => o.value === value))
  const count = options.length

  const onKeyDown = (event: React.KeyboardEvent) => {
    const delta = event.key === 'ArrowRight' ? 1 : event.key === 'ArrowLeft' ? -1 : 0
    if (!delta) return
    event.preventDefault()
    onChange(options[(index + delta + count) % count].value)
  }

  return (
    <div
      className={cx(styles.track, styles[size], className)}
      style={{ '--lp-seg-count': count, '--lp-seg-index': index } as CSSProperties}
      role="radiogroup"
      aria-label={aria['aria-label']}
      onKeyDown={onKeyDown}
      onPointerLeave={() => setHot(null)}
    >
      <div
        className={styles.gooLayer}
        data-lp-goo-layer=""
        style={
          {
            /* xxs: these two shapes already touch, so blur only erodes the pill's caps. */
            '--lp-goo-rest': `url(#${gooFilterId('xxs', 'rest')})`,
            '--lp-goo-flow': `url(#${gooFilterId('xxs', 'flow')})`,
          } as CSSProperties
        }
        aria-hidden="true"
      >
        <span className={styles.thumb} />
        {options.map((_, i) => (
          <span
            key={i}
            className={styles.hotBlob}
            data-hot={hot === i && i !== index ? '' : undefined}
            style={{ '--lp-seg-i': i } as CSSProperties}
          />
        ))}
      </div>
      <div className={styles.segments}>
        {options.map((option, i) => (
          <button
            key={option.value}
            type="button"
            role="radio"
            aria-checked={option.value === value}
            aria-label={option.ariaLabel}
            className={styles.segment}
            data-selected={option.value === value ? '' : undefined}
            tabIndex={option.value === value ? 0 : -1}
            onPointerEnter={() => setHot(i)}
            onClick={() => onChange(option.value)}
          >
            {option.icon ? <span className={styles.icon}>{option.icon}</span> : null}
            {option.label ? <span className={styles.label}>{option.label}</span> : null}
          </button>
        ))}
      </div>
    </div>
  )
}
