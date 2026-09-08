/**
 * TitleBar — the brushed grab handle of a window: traffic lights on the left,
 * an embossed centered title, the sliding sheen behind both. Double-click zooms.
 */

import { forwardRef, type ReactNode } from 'react'
import { Surface } from '@/components/Surface'
import { TrafficLights } from '@/components/TrafficLights'
import { cx } from '@/lib/cx'
import styles from './TitleBar.module.css'

export interface TitleBarProps {
  title: string
  active: boolean
  shaded?: boolean
  zoomed?: boolean
  onClose(): void
  onShade(): void
  onZoom(): void
  onDoubleClick?(): void
  /** Optional trailing content (a toolbar toggle, a status bubble). */
  accessory?: ReactNode
  className?: string
}

export const TitleBar = forwardRef<HTMLDivElement, TitleBarProps>(function TitleBar(
  { title, active, shaded, zoomed, onClose, onShade, onZoom, onDoubleClick, accessory, className },
  ref,
) {
  return (
    <Surface
      ref={ref}
      variant="titlebar"
      sheen
      className={cx(styles.bar, className)}
      data-active={active}
      onDoubleClick={(event) => {
        if ((event.target as HTMLElement).closest('button')) return
        onDoubleClick?.()
      }}
    >
      <TrafficLights active={active} shaded={shaded} zoomed={zoomed} onClose={onClose} onShade={onShade} onZoom={onZoom} />
      <span className={styles.title}>{title}</span>
      {accessory ? <span className={styles.accessory}>{accessory}</span> : null}
    </Surface>
  )
})
