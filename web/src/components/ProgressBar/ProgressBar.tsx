/**
 * ProgressBar — an inset rail with a liquid accent fill. Indeterminate shows
 * Aqua's barber pole, in platinum.
 */

import type { CSSProperties } from 'react'
import { cx } from '@/lib/cx'
import styles from './ProgressBar.module.css'

export interface ProgressBarProps {
  /** 0..1; omit for indeterminate. */
  value?: number
  label?: string
  className?: string
}

export function ProgressBar({ value, label, className }: ProgressBarProps) {
  const indeterminate = value === undefined
  const pct = indeterminate ? 0 : Math.max(0, Math.min(1, value)) * 100
  return (
    <div
      className={cx(styles.rail, indeterminate && styles.indeterminate, className)}
      role="progressbar"
      aria-label={label}
      aria-valuemin={0}
      aria-valuemax={100}
      aria-valuenow={indeterminate ? undefined : Math.round(pct)}
      style={{ '--lp-progress': `${pct}%` } as CSSProperties}
    >
      <div className={styles.fill} />
    </div>
  )
}
