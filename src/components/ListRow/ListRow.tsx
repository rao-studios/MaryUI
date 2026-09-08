/** ListRow — one line of a list view: icon, name, then trailing columns. Rows alternate tint. */

import type { ReactNode } from 'react'
import { cx } from '@/lib/cx'
import styles from './ListRow.module.css'

export interface ListRowProps {
  icon?: ReactNode
  name: ReactNode
  columns?: ReactNode[]
  selected?: boolean
  onSelect?(): void
  onOpen?(): void
  className?: string
}

export function ListRow({ icon, name, columns = [], selected, onSelect, onOpen, className }: ListRowProps) {
  return (
    <div
      className={cx(styles.row, className)}
      role="row"
      aria-selected={selected}
      data-selected={selected ? '' : undefined}
      tabIndex={0}
      onClick={onSelect}
      onDoubleClick={onOpen}
      onKeyDown={(event) => {
        if (event.key === 'Enter') onOpen?.()
        if (event.key === ' ') {
          event.preventDefault()
          onSelect?.()
        }
      }}
    >
      <span className={styles.name} role="gridcell">
        {icon ? <span className={styles.icon}>{icon}</span> : null}
        <span className={styles.text}>{name}</span>
      </span>
      {columns.map((column, i) => (
        <span key={i} className={styles.column} role="gridcell">
          {column}
        </span>
      ))}
    </div>
  )
}

export function ListHeader({ columns }: { columns: ReactNode[] }) {
  return (
    <div className={cx(styles.row, styles.header)} role="row">
      {columns.map((column, i) => (
        <span key={i} className={i === 0 ? styles.name : styles.column} role="columnheader">
          {column}
        </span>
      ))}
    </div>
  )
}
