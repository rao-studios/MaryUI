/**
 * TrafficLights — close / shade / zoom, each the same glass bead the Toggle
 * uses for its knob: a well of coloured liquid with a surface line across it,
 * sitting proud of the title bar.
 *
 * The merge runs on a second copy of the liquid *behind* the beads, and it is a
 * hover flourish only: hovering the group loosens the tension and the liquid
 * necks out from between them. A moving window does nothing to it. The beads are
 * a fixed size that never fades — three circles turning into three fat drops
 * mid-drag read as the lights becoming something else, when all that should be
 * moving is the liquid inside them.
 *
 * Inactive windows drain the colour out until you hover them, as Aqua did.
 */

import { GooGroup } from '@/components/GooGroup'
import { LiquidBubble, type BubbleTint } from '@/components/LiquidBubble'
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

const SIZE = parseFloat(tokens.size.traffic)
const GAP = parseFloat(tokens.size.trafficGap)
const FILL = tokens.liquid.fillTraffic
/** Shading drains the well, so a collapsed window reads at a glance. */
const SHADED_FILL = FILL * 0.45
/**
 * The copy in the filtered layer is the *bulk* of the liquid: a full disc, so it
 * has the mass to survive the flow blur and bridge. Anything less and a lone
 * drop simply dissolves under its own filter. The surface line and the glass
 * live on the bead above it, which is opaque and hides this at rest.
 *
 * Both GooGroup layers share one gap, so their pitches only line up while their
 * items are the same width — hence the same SIZE here.
 */
const MERGE_FILL = 1

/** Phases keep the three surfaces from rolling in lockstep. */
const LIGHTS: { tint: BubbleTint; phase: number; label: string; glyph: string }[] = [
  { tint: 'close', phase: 0, label: 'Close', glyph: '×' },
  { tint: 'minimize', phase: 2.3, label: 'Shade', glyph: '–' },
  { tint: 'zoom', phase: 4.1, label: 'Zoom', glyph: '+' },
]

export function TrafficLights({ active, shaded = false, zoomed = false, onClose, onShade, onZoom }: TrafficLightsProps) {
  const handlers = [onClose, onShade, onZoom]
  const fillFor = (i: number) => (i === 1 && shaded ? SHADED_FILL : FILL)
  const labelFor = (i: number) => {
    if (i === 1) return shaded ? 'Unshade' : 'Shade'
    if (i === 2) return zoomed ? 'Restore' : 'Zoom'
    return LIGHTS[i].label
  }

  return (
    <GooGroup
      size="xs"
      blobs={{ count: 3, size: SIZE, shape: 'circle' }}
      gap={GAP}
      respondsToMotion={false}
      className={styles.lights}
      data-active={active}
      data-no-drag=""
      renderBlob={(i) => (
        <LiquidBubble tint={LIGHTS[i].tint} size={SIZE} phase={LIGHTS[i].phase} fill={MERGE_FILL} liquidOnly />
      )}
    >
      {LIGHTS.map((light, i) => (
        <button
          key={light.tint}
          type="button"
          className={styles.light}
          data-goo-index={i}
          aria-label={labelFor(i)}
          aria-pressed={i === 0 ? undefined : i === 1 ? shaded : zoomed}
          onClick={handlers[i]}
        >
          <span className={styles.bead}>
            <LiquidBubble
              tint={light.tint}
              size={SIZE}
              phase={light.phase}
              fill={fillFor(i)}
              glyph={i === 2 && zoomed ? '−' : light.glyph}
            />
          </span>
        </button>
      ))}
    </GooGroup>
  )
}
