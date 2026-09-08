/** Swatches straight from the generated tokens module. Click a swatch to copy its CSS variable. */

import { useState } from 'react'
import { tokens } from '@/tokens/tokens'
import styles from '../GalleryApp.module.css'

interface Swatch {
  name: string
  value: string
}

function group(prefix: string, record: Record<string, string>): Swatch[] {
  return Object.entries(record).map(([k, v]) => ({ name: `${prefix}-${k.replace(/[A-Z]/g, (c) => `-${c.toLowerCase()}`)}`, value: v }))
}

const groups: { title: string; swatches: Swatch[] }[] = [
  { title: 'Platinum ramp', swatches: group('platinum', tokens.platinum) },
  { title: 'Surfaces', swatches: group('surface', tokens.surface) },
  { title: 'Ink', swatches: group('ink', tokens.ink) },
  { title: 'Accent · Blue', swatches: group('accent-blue', tokens.accent.blue) },
  { title: 'Accent · Graphite', swatches: group('accent-graphite', tokens.accent.graphite) },
  {
    title: 'Traffic',
    swatches: [
      ...group('traffic-close', tokens.traffic.close),
      ...group('traffic-minimize', tokens.traffic.minimize),
      ...group('traffic-zoom', tokens.traffic.zoom),
    ],
  },
]

export function TokensTab() {
  const [copied, setCopied] = useState<string | null>(null)

  const copy = (name: string) => {
    const text = `var(--lp-${name})`
    void navigator.clipboard?.writeText(text).catch(() => {})
    setCopied(name)
    setTimeout(() => setCopied(null), 1200)
  }

  return (
    <>
      {groups.map((g) => (
        <section key={g.title} className={styles.section}>
          <h2 className={styles.heading}>{g.title}</h2>
          <div className={styles.swatches}>
            {g.swatches.map((s) => (
              <button key={s.name} type="button" className={styles.swatch} onClick={() => copy(s.name)} title={`Copy var(--lp-${s.name})`}>
                <span className={styles.chip} style={{ background: s.value }} />
                <span>{copied === s.name ? 'copied' : s.name}</span>
              </button>
            ))}
          </div>
        </section>
      ))}
      <section className={styles.section}>
        <h2 className={styles.heading}>Shape and rhythm</h2>
        <div className={styles.row}>
          {Object.entries(tokens.radius).map(([k, v]) => (
            <div key={k} style={{ display: 'grid', justifyItems: 'center', gap: 6 }}>
              <span style={{ width: 48, height: 48, borderRadius: v, background: 'linear-gradient(180deg, var(--lp-platinum-0), var(--lp-platinum-3))', boxShadow: 'var(--lp-shadow-emboss-raised), 0 0 0 1px var(--lp-edge-hairline)' }} />
              <span className={styles.mono} style={{ color: 'var(--lp-ink-tertiary)' }}>radius-{k} {v}</span>
            </div>
          ))}
        </div>
        <div className={styles.row}>
          {Object.entries(tokens.space).map(([k, v]) => (
            <div key={k} style={{ display: 'grid', justifyItems: 'center', gap: 6 }}>
              <span style={{ width: v, height: 24, background: 'var(--lp-accent-base)', borderRadius: 2 }} />
              <span className={styles.mono} style={{ color: 'var(--lp-ink-tertiary)' }}>{k} · {v}</span>
            </div>
          ))}
        </div>
      </section>
    </>
  )
}
