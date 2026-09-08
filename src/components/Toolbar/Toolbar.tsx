/** Toolbar — the brushed strip under a title bar that holds a window's controls. */

import type { HTMLAttributes, ReactNode } from 'react'
import { Surface } from '@/components/Surface'
import { cx } from '@/lib/cx'
import styles from './Toolbar.module.css'

export interface ToolbarProps extends HTMLAttributes<HTMLDivElement> {
  children: ReactNode
}

export function Toolbar({ className, children, ...rest }: ToolbarProps) {
  return (
    <Surface variant="flat" className={cx(styles.toolbar, className)} role="toolbar" {...rest}>
      {children}
    </Surface>
  )
}

export function ToolbarSpacer() {
  return <span className={styles.spacer} />
}

export function ToolbarGroup({ children, className }: { children: ReactNode; className?: string }) {
  return <div className={cx(styles.group, className)}>{children}</div>
}
