/**
 * MenuBar — the brushed strip across the top: the Rao monogram menu, the app
 * menus, and status on the right. Click opens a menu; while one is open,
 * hovering another switches to it; Esc or a click elsewhere closes.
 */

import { useCallback, useEffect, useMemo, useRef, useState, type ReactNode } from 'react'
import { Menu } from '@/components/Menu'
import { Monogram } from '@/components/Monogram'
import { Surface } from '@/components/Surface'
import { useClock } from '@/hooks/useClock'
import { useOutsideClick } from '@/hooks/useOutsideClick'
import { cx } from '@/lib/cx'
import { buildMenus } from '@/desktop/menus'
import { useSettings } from '@/desktop/settings'
import { useWM, useWMStore } from '@/desktop/wm/useWM'
import styles from './MenuBar.module.css'

export interface MenuBarProps {
  /** Extra status items on the right, before the clock. */
  status?: ReactNode
}

export function MenuBar({ status }: MenuBarProps) {
  const store = useWMStore()
  const state = useWM((s) => s)
  const settings = useSettings()
  const clock = useClock()
  const menus = useMemo(() => buildMenus(store, state, settings), [store, state, settings])

  const [openId, setOpenId] = useState<string | null>(null)
  const barRef = useRef<HTMLDivElement>(null)
  const triggerRefs = useRef(new Map<string, HTMLButtonElement>())

  const close = useCallback(() => setOpenId(null), [])
  const menuRefs = useMemo(() => [barRef], [])
  useOutsideClick(menuRefs, () => {
    // Clicks inside a portaled menu are handled by the menu itself.
    const hovered = document.querySelector('[data-lp-menu]:hover')
    if (!hovered) close()
  }, openId !== null)

  useEffect(() => {
    if (!openId) return
    const onKey = (event: KeyboardEvent) => {
      if (event.key === 'Escape') close()
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [openId, close])

  const openIndex = menus.findIndex((m) => m.id === openId)
  const step = (delta: number) => {
    if (openIndex < 0) return
    setOpenId(menus[(openIndex + delta + menus.length) % menus.length].id)
  }

  const anchorFor = (id: string) => {
    const el = triggerRefs.current.get(id)
    if (!el) return { x: 0, y: 0 }
    const box = el.getBoundingClientRect()
    return { x: box.left, y: box.bottom + 2 }
  }

  const open = menus.find((m) => m.id === openId)

  return (
    <Surface ref={barRef} variant="bar" sheen className={styles.bar} role="menubar">
      <div className={styles.left}>
        {menus.map((menu) => (
          <button
            key={menu.id}
            ref={(el) => {
              if (el) triggerRefs.current.set(menu.id, el)
              else triggerRefs.current.delete(menu.id)
            }}
            type="button"
            role="menuitem"
            aria-haspopup="menu"
            aria-expanded={openId === menu.id}
            className={cx(styles.trigger, menu.id === 'rao' && styles.mark)}
            data-open={openId === menu.id ? '' : undefined}
            onPointerDown={(event) => {
              event.preventDefault()
              setOpenId((current) => (current === menu.id ? null : menu.id))
            }}
            onPointerEnter={() => {
              if (openId && openId !== menu.id) setOpenId(menu.id)
            }}
          >
            {menu.id === 'rao' ? <Monogram size={18} /> : menu.label}
          </button>
        ))}
      </div>
      <div className={styles.right}>
        {status}
        <span className={styles.clock}>{clock}</span>
      </div>
      {open ? (
        <Menu
          key={open.id}
          entries={open.entries}
          anchor={anchorFor(open.id)}
          onClose={close}
          onArrowLeft={() => step(-1)}
          onArrowRight={() => step(1)}
        />
      ) : null}
    </Surface>
  )
}
