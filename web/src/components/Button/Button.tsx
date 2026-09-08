/**
 * Button — a raised platinum capsule with an embossed label. `primary` fills
 * with the accent, `quiet` shows its metal only on hover. Pressing sinks the
 * gradient and the emboss, like Aqua's buttons did.
 */

import { forwardRef, type ButtonHTMLAttributes, type ReactNode } from 'react'
import { cx } from '@/lib/cx'
import styles from './Button.module.css'

export type ButtonVariant = 'default' | 'primary' | 'quiet'
export type ButtonSize = 'sm' | 'md'

export interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: ButtonVariant
  size?: ButtonSize
  icon?: ReactNode
  /** Icon-only button: square, with the label as the accessible name. */
  iconOnly?: boolean
  children?: ReactNode
}

export const Button = forwardRef<HTMLButtonElement, ButtonProps>(function Button(
  { variant = 'default', size = 'md', icon, iconOnly = false, className, children, type = 'button', ...rest },
  ref,
) {
  return (
    <button
      ref={ref}
      type={type}
      className={cx(styles.button, styles[variant], styles[size], iconOnly && styles.iconOnly, className)}
      aria-label={iconOnly && typeof children === 'string' ? children : undefined}
      {...rest}
    >
      {icon ? <span className={styles.icon}>{icon}</span> : null}
      {iconOnly ? null : <span className={styles.label}>{children}</span>}
    </button>
  )
})
