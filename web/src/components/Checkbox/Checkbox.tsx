/** Checkbox — a small raised platinum box; checked, it floods with the accent. */

import type { ReactNode } from 'react'
import { Icon } from '@/components/Icon'
import { cx } from '@/lib/cx'
import styles from './Checkbox.module.css'

export interface CheckboxProps {
  checked: boolean
  onChange(checked: boolean): void
  children?: ReactNode
  disabled?: boolean
  className?: string
}

export function Checkbox({ checked, onChange, children, disabled, className }: CheckboxProps) {
  return (
    <label className={cx(styles.checkbox, className)} data-disabled={disabled ? '' : undefined}>
      <input
        type="checkbox"
        className={styles.input}
        checked={checked}
        disabled={disabled}
        onChange={(event) => onChange(event.target.checked)}
      />
      <span className={styles.box} aria-hidden="true">
        <Icon name="check" size={11} strokeWidth={2.6} className={styles.mark} />
      </span>
      {children ? <span className={styles.label}>{children}</span> : null}
    </label>
  )
}
