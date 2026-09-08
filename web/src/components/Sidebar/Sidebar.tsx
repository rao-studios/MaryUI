/** Sidebar — a source list: section headers and selectable rows with icons. */

import type { ReactNode } from 'react'
import { cx } from '@/lib/cx'
import styles from './Sidebar.module.css'

export function Sidebar({ children, className }: { children: ReactNode; className?: string }) {
  return (
    <nav className={cx(styles.sidebar, className)} aria-label="Sidebar">
      {children}
    </nav>
  )
}

export function SidebarSection({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section className={styles.section}>
      <h3 className={styles.heading}>{title}</h3>
      <ul className={styles.list}>{children}</ul>
    </section>
  )
}

export interface SidebarItemProps {
  icon?: ReactNode
  selected?: boolean
  onSelect?(): void
  children: ReactNode
}

export function SidebarItem({ icon, selected, onSelect, children }: SidebarItemProps) {
  return (
    <li>
      <button type="button" className={styles.item} data-selected={selected ? '' : undefined} onClick={onSelect}>
        {icon ? <span className={styles.icon}>{icon}</span> : null}
        <span className={styles.label}>{children}</span>
      </button>
    </li>
  )
}
