/**
 * Menu — a dropdown panel of MenuItems, portaled to the body so no surface
 * clips it. Owns roving keyboard focus; the MenuBar owns which menu is open.
 * Blur, not refraction: the panel is near-opaque platinum with a soft backdrop.
 */

import { useCallback, useEffect, useState, type CSSProperties } from 'react'
import { createPortal } from 'react-dom'
import { MenuItem, MenuSeparator } from '@/components/MenuItem'
import { cx } from '@/lib/cx'
import styles from './Menu.module.css'

export interface MenuItemModel {
  id: string
  label: string
  shortcut?: string
  checked?: boolean
  disabled?: boolean
  onSelect?(): void
}

export type MenuEntry = MenuItemModel | { separator: true }

export function isSeparator(entry: MenuEntry): entry is { separator: true } {
  return 'separator' in entry
}

export interface MenuProps {
  entries: MenuEntry[]
  /** Viewport coordinates of the panel's top-left corner. */
  anchor: { x: number; y: number }
  onClose(): void
  onArrowLeft?(): void
  onArrowRight?(): void
  minWidth?: number
  className?: string
}

export function Menu({ entries, anchor, onClose, onArrowLeft, onArrowRight, minWidth = 200, className }: MenuProps) {
  const [active, setActive] = useState<number | null>(null)

  const enabledIndexes = entries.map((e, i) => (!isSeparator(e) && !e.disabled ? i : -1)).filter((i) => i >= 0)

  const move = useCallback(
    (delta: number) => {
      if (enabledIndexes.length === 0) return
      setActive((current) => {
        const pos = current === null ? -1 : enabledIndexes.indexOf(current)
        const next = (pos + delta + enabledIndexes.length) % enabledIndexes.length
        return enabledIndexes[next]
      })
    },
    [enabledIndexes],
  )

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      switch (event.key) {
        case 'ArrowDown':
          event.preventDefault()
          move(1)
          break
        case 'ArrowUp':
          event.preventDefault()
          move(-1)
          break
        case 'ArrowLeft':
          event.preventDefault()
          onArrowLeft?.()
          break
        case 'ArrowRight':
          event.preventDefault()
          onArrowRight?.()
          break
        case 'Enter':
        case ' ': {
          event.preventDefault()
          const entry = active === null ? null : entries[active]
          if (entry && !isSeparator(entry) && !entry.disabled) {
            entry.onSelect?.()
            onClose()
          }
          break
        }
        case 'Escape':
          event.preventDefault()
          onClose()
          break
      }
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [active, entries, move, onArrowLeft, onArrowRight, onClose])

  const style: CSSProperties = { left: anchor.x, top: anchor.y, minWidth }

  return createPortal(
    <div className={cx(styles.menu, className)} style={style} role="menu" data-lp-menu="">
      {entries.map((entry, index) =>
        isSeparator(entry) ? (
          <MenuSeparator key={`sep-${index}`} />
        ) : (
          <MenuItem
            key={entry.id}
            label={entry.label}
            shortcut={entry.shortcut}
            checked={entry.checked}
            disabled={entry.disabled}
            active={active === index}
            onActivate={() => setActive(index)}
            onSelect={() => {
              entry.onSelect?.()
              onClose()
            }}
          />
        ),
      )}
    </div>,
    document.body,
  )
}
