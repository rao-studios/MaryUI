/**
 * TrafficLights — close / shade / zoom, each a LiquidBubble set in a platinum
 * rim. The rims are GooGroup blobs, so hovering one swells it into its
 * neighbors and a fast drag smears all three sideways like beads of mercury.
 * Inactive windows drain the color out until you hover them, as Aqua did.
 */

import { GooGroup } from '@/components/GooGroup'
import { LiquidBubble } from '@/components/LiquidBubble'
import { tokens } from '@/tokens/tokens'
import styles from './TrafficLights.module.css'

export interface TrafficLightsProps {
  active: boolean
  shaded?: boolean
  zoomed?: boolean
  onClose(): void
  onShade(): void
  onZoom(): void
}

const BUBBLE = parseFloat(tokens.size.traffic)
const RIM = BUBBLE + 2
const GAP = parseFloat(tokens.size.trafficGap) - 2

export function TrafficLights({ active, shaded = false, zoomed = false, onClose, onShade, onZoom }: TrafficLightsProps) {
  return (
    <GooGroup
      size="xs"
      blobs={{ count: 3, size: RIM, shape: 'circle', smear: 2.5 }}
      gap={GAP}
      className={styles.lights}
      data-active={active}
      data-no-drag=""
    >
      <button type="button" className={styles.light} data-goo-index={0} aria-label="Close" onClick={onClose}>
        <LiquidBubble tint="close" size={BUBBLE} phase={0} glyph="×" />
      </button>
      <button
        type="button"
        className={styles.light}
        data-goo-index={1}
        aria-label={shaded ? 'Unshade' : 'Shade'}
        aria-pressed={shaded}
        onClick={onShade}
      >
        <LiquidBubble tint="minimize" size={BUBBLE} phase={2.3} fill={shaded ? 0.38 : undefined} glyph="–" />
      </button>
      <button
        type="button"
        className={styles.light}
        data-goo-index={2}
        aria-label={zoomed ? 'Restore' : 'Zoom'}
        aria-pressed={zoomed}
        onClick={onZoom}
      >
        <LiquidBubble tint="zoom" size={BUBBLE} phase={4.1} glyph={zoomed ? '−' : '+'} />
      </button>
    </GooGroup>
  )
}
