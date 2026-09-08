/**
 * TextField — an inset well with an optional leading icon. Focus lights the
 * accent ring; the well itself never changes color.
 */

import { forwardRef, type InputHTMLAttributes, type ReactNode } from 'react'
import { cx } from '@/lib/cx'
import styles from './TextField.module.css'

export interface TextFieldProps extends InputHTMLAttributes<HTMLInputElement> {
  icon?: ReactNode
  /** Round capsule, as in a toolbar search field. */
  round?: boolean
}

export const TextField = forwardRef<HTMLInputElement, TextFieldProps>(function TextField(
  { icon, round = false, className, ...rest },
  ref,
) {
  return (
    <label className={cx(styles.field, round && styles.round, className)}>
      {icon ? <span className={styles.icon}>{icon}</span> : null}
      <input ref={ref} className={styles.input} {...rest} />
    </label>
  )
})
