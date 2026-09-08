/**
 * MenuItem — one row of a dropdown: check column, label, shortcut. The
 * highlight is the accent gradient (Aqua's blue or graphite). `separator`
 * renders the hairline instead.
 */

import type { ReactNode } from 'react'
import { cx } from '@/lib/cx'
import styles from './MenuItem.module.css'

export interface MenuItemProps {
  label: ReactNode
  shortcut?: string
  checked?: boolean
  disabled?: boolean
  /** Highlighted by hover or keyboard. */
  active?: boolean
  onSelect?(): void
  onActivate?(): void
  className?: string
}

export function MenuItem({ label, shortcut, checked, disabled, active, onSelect, onActivate, className }: MenuItemProps) {
  return (
    <button
      type="button"
      role="menuitemcheckbox"
      aria-checked={checked ?? false}
      aria-disabled={disabled}
      className={cx(styles.item, className)}
      data-active={active && !disabled ? '' : undefined}
      disabled={disabled}
      onPointerEnter={onActivate}
      onClick={() => {
        if (!disabled) onSelect?.()
      }}
    >
      <span className={styles.check} aria-hidden="true">
        {checked ? '✓' : ''}
      </span>
      <span className={styles.label}>{label}</span>
      {shortcut ? <span className={styles.shortcut}>{shortcut}</span> : null}
    </button>
  )
}

export function MenuSeparator() {
  return <hr className={styles.separator} role="separator" />
}
