/**
 * Toggle — an Aqua-style capsule whose knob is a LiquidBubble. Off, the knob
 * is plain platinum in a dark well; on, the well floods with the accent and
 * the knob's liquid takes the tint and slides across.
 */

import { LiquidBubble } from '@/components/LiquidBubble'
import { cx } from '@/lib/cx'
import styles from './Toggle.module.css'

export interface ToggleProps {
  checked: boolean
  onChange(checked: boolean): void
  disabled?: boolean
  label?: string
  className?: string
}

export function Toggle({ checked, onChange, disabled, label, className }: ToggleProps) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={checked}
      aria-label={label}
      disabled={disabled}
      className={cx(styles.toggle, className)}
      data-on={checked ? '' : undefined}
      onClick={() => onChange(!checked)}
    >
      <span className={styles.knob}>
        <LiquidBubble tint={checked ? 'accent' : 'platinum'} size={16} fill={checked ? 0.72 : 0.5} />
      </span>
    </button>
  )
}
