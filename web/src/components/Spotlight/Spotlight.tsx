/**
 * Spotlight — the floating search bar that is also the application dock.
 * `SpotlightPanel` is the pure look (the Gallery shows one inline);
 * `Spotlight` mounts it over the desktop, focuses the bar, and closes on a
 * press outside. The model (open, query, selection, ranking) is
 * desktop/spotlight.ts; the C desktop paints the same panel (lp_spotlight_panel).
 */

import { useEffect, useMemo, useRef, type CSSProperties, type KeyboardEvent, type RefObject } from 'react'
import { createPortal } from 'react-dom'
import { Icon } from '@/components/Icon'
import { ListRow } from '@/components/ListRow'
import { Surface } from '@/components/Surface'
import { TextField } from '@/components/TextField'
import { useOutsideClick } from '@/hooks/useOutsideClick'
import { cx } from '@/lib/cx'
import { isBlankQuery, SPOTLIGHT_PLACEHOLDER, type SpotlightItem } from '@/desktop/spotlight'
import styles from './Spotlight.module.css'

export interface SpotlightPanelProps {
  query: string
  /** The results for the query (the dock when it is blank). */
  items: SpotlightItem[]
  selection: number
  placeholder?: string
  onQueryChange?(query: string): void
  onHover?(index: number): void
  onActivate?(index: number): void
  /** ↑/↓ (and ←/→ in the dock) step the selection by this much. */
  onMove?(delta: number): void
  inputRef?: RefObject<HTMLInputElement>
  className?: string
  style?: CSSProperties
}

export function SpotlightPanel({
  query,
  items,
  selection,
  placeholder = SPOTLIGHT_PLACEHOLDER,
  onQueryChange,
  onHover,
  onActivate,
  onMove,
  inputRef,
  className,
  style,
}: SpotlightPanelProps) {
  const dock = isBlankQuery(query)
  const onKeyDown = (event: KeyboardEvent<HTMLInputElement>) => {
    if (event.key === 'ArrowDown' || (dock && event.key === 'ArrowRight')) {
      event.preventDefault()
      onMove?.(1)
    } else if (event.key === 'ArrowUp' || (dock && event.key === 'ArrowLeft')) {
      event.preventDefault()
      onMove?.(-1)
    } else if (event.key === 'Enter') {
      event.preventDefault()
      onActivate?.(selection)
    }
  }
  return (
    <div className={cx(styles.shell, className)} style={style} data-lp-spotlight="">
      <Surface variant="flat" sheen className={styles.panel}>
        <TextField
          ref={inputRef}
          round
          large
          className={styles.bar}
          icon={<Icon name="search" size={16} />}
          placeholder={placeholder}
          value={query}
          onChange={(event) => onQueryChange?.(event.target.value)}
          onKeyDown={onKeyDown}
          spellCheck={false}
          autoComplete="off"
          aria-label="Spotlight"
        />
        <div className={styles.divider} />
        {dock ? (
          <div className={styles.dock} role="listbox" aria-label="Applications">
            {items.map((item, i) => (
              <button
                key={`${item.kind}:${item.id}`}
                type="button"
                role="option"
                aria-selected={i === selection}
                className={styles.cell}
                data-selected={i === selection ? '' : undefined}
                onPointerEnter={() => onHover?.(i)}
                onClick={() => onActivate?.(i)}
              >
                <span className={styles.plate}>
                  <Icon name={item.icon} size={36} strokeWidth={1.6} />
                </span>
                <span className={styles.label}>{item.title}</span>
                <span className={cx(styles.dot, item.running && styles.running)} aria-hidden="true" />
              </button>
            ))}
          </div>
        ) : items.length ? (
          <div className={styles.results} role="grid" aria-label="Results">
            {items.map((item, i) => (
              <div key={`${item.kind}:${item.id}`} onPointerEnter={() => onHover?.(i)}>
                <ListRow
                  icon={<Icon name={item.icon} size={14} />}
                  name={item.title}
                  columns={[item.subtitle]}
                  selected={i === selection}
                  onSelect={() => onActivate?.(i)}
                  onOpen={() => onActivate?.(i)}
                />
              </div>
            ))}
          </div>
        ) : (
          <p className={styles.empty}>No results for “{query.trim()}”.</p>
        )}
      </Surface>
    </div>
  )
}

export interface SpotlightProps extends Omit<SpotlightPanelProps, 'inputRef' | 'className' | 'style'> {
  open: boolean
  onClose(): void
}

export function Spotlight({ open, onClose, ...panel }: SpotlightProps) {
  const shellRef = useRef<HTMLDivElement>(null)
  const inputRef = useRef<HTMLInputElement>(null)
  const refs = useMemo(() => [shellRef], [])
  useOutsideClick(refs, onClose, open)
  useEffect(() => {
    if (open) inputRef.current?.focus()
  }, [open])
  if (!open) return null
  return createPortal(
    <div className={styles.overlay}>
      <div ref={shellRef} className={styles.place}>
        <SpotlightPanel {...panel} inputRef={inputRef} />
      </div>
    </div>,
    document.body,
  )
}
