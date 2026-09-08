/**
 * Slider — a native range input dressed as an inset rail with a platinum
 * thumb. The filled portion takes the accent. Keyboard works for free.
 */

import { useId, type CSSProperties } from 'react'
import { cx } from '@/lib/cx'
import styles from './Slider.module.css'

export interface SliderProps {
  value: number
  min?: number
  max?: number
  step?: number
  onChange(value: number): void
  label?: string
  /** Show the value beside the rail. */
  showValue?: boolean
  format?(value: number): string
  disabled?: boolean
  className?: string
}

export function Slider({ value, min = 0, max = 100, step = 1, onChange, label, showValue, format, disabled, className }: SliderProps) {
  const id = useId()
  const pct = max === min ? 0 : ((value - min) / (max - min)) * 100
  return (
    <div className={cx(styles.slider, className)} style={{ '--lp-slider-pct': `${pct}%` } as CSSProperties}>
      {label ? (
        <label className={styles.label} htmlFor={id}>
          {label}
        </label>
      ) : null}
      <input
        id={id}
        className={styles.input}
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        disabled={disabled}
        aria-label={label ? undefined : 'Slider'}
        onChange={(event) => onChange(Number(event.target.value))}
      />
      {showValue ? <span className={styles.value}>{format ? format(value) : value}</span> : null}
    </div>
  )
}
