/**
 * Live colour for every bead in the system. The model is small: each bead wears
 * a *tint* — close, minimize, zoom, accent, platinum — and a tint is three
 * values, the liquid (`base`), its shadow (`deep`) and its lit surface
 * (`light`). Traffic lights, toggle knobs and slider thumbs are all covered by
 * those five.
 *
 * Writes land on `--lp-bead-*` custom properties, which `LiquidBubble` reads
 * before falling back to the shared tokens. That is deliberate: `accent` and
 * `platinum` alias tokens the whole desktop is built on, and recolouring a knob
 * should not restyle buttons, toggle tracks and selection highlights.
 *
 * Follows the Motion tab: live, plus a JSON patch to paste back into
 * tokens.json when a set of colours is worth keeping.
 */

import { useCallback, useState } from 'react'
import { Button } from '@/components/Button'
import { LiquidBubble, type BubbleTint } from '@/components/LiquidBubble'
import { Toggle } from '@/components/Toggle'
import { Surface } from '@/components/Surface'
import { tokens } from '@/tokens/tokens'
import styles from '../GalleryApp.module.css'

type Role = 'base' | 'deep' | 'light'

interface Tint {
  tint: BubbleTint
  label: string
  /** Where each role comes from in tokens.json, for the patch and the reset. */
  paths: Record<Role, string>
  defaults: Record<Role, string>
}

const t = tokens
const TINTS: Tint[] = [
  {
    tint: 'close',
    label: 'Close',
    paths: { base: 'traffic.close.base', deep: 'traffic.close.deep', light: 'traffic.close.light' },
    defaults: { base: t.traffic.close.base, deep: t.traffic.close.deep, light: t.traffic.close.light },
  },
  {
    tint: 'minimize',
    label: 'Shade',
    paths: { base: 'traffic.minimize.base', deep: 'traffic.minimize.deep', light: 'traffic.minimize.light' },
    defaults: { base: t.traffic.minimize.base, deep: t.traffic.minimize.deep, light: t.traffic.minimize.light },
  },
  {
    tint: 'zoom',
    label: 'Zoom',
    paths: { base: 'traffic.zoom.base', deep: 'traffic.zoom.deep', light: 'traffic.zoom.light' },
    defaults: { base: t.traffic.zoom.base, deep: t.traffic.zoom.deep, light: t.traffic.zoom.light },
  },
  {
    tint: 'accent',
    label: 'Knob, on',
    paths: { base: 'accent.blue.base', deep: 'accent.blue.deep', light: 'accent.blue.light' },
    defaults: { base: t.accent.blue.base, deep: t.accent.blue.deep, light: t.accent.blue.light },
  },
  {
    tint: 'platinum',
    label: 'Knob, off',
    paths: { base: 'platinum.4', deep: 'platinum.7', light: 'platinum.0' },
    defaults: { base: t.platinum['4'], deep: t.platinum['7'], light: t.platinum['0'] },
  },
]

const ROLES: { role: Role; label: string; hint: string }[] = [
  { role: 'base', label: 'Liquid', hint: 'the body of the liquid' },
  { role: 'deep', label: 'Shadow', hint: 'where the liquid falls away' },
  { role: 'light', label: 'Surface', hint: 'the lit top of it' },
]

const beadVar = (tint: BubbleTint, role: Role) => `--lp-bead-${tint}-${role}`

function readVar(tint: BubbleTint, role: Role, fallback: string): string {
  const v = getComputedStyle(document.documentElement).getPropertyValue(beadVar(tint, role)).trim()
  return v || fallback
}

function ColorField({ value, onChange, label }: { value: string; onChange(v: string): void; label: string }) {
  return (
    <label className={styles.row} style={{ gap: 'var(--lp-space-2)', marginBottom: 0 }}>
      <input
        type="color"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        aria-label={label}
        className={styles.colorWell}
      />
      <span className={styles.mono} style={{ color: 'var(--lp-ink-secondary)' }}>
        {value}
      </span>
    </label>
  )
}

export function CustomizeTab() {
  const [, bump] = useState(0)
  const [copied, setCopied] = useState(false)
  const rerender = useCallback(() => bump((n) => n + 1), [])

  const set = (tint: BubbleTint, role: Role, value: string) => {
    document.documentElement.style.setProperty(beadVar(tint, role), value)
    rerender()
  }

  const patch = () => {
    const out: Record<string, string> = {}
    for (const tint of TINTS) {
      for (const { role } of ROLES) {
        const value = readVar(tint.tint, role, tint.defaults[role])
        if (value.toLowerCase() !== tint.defaults[role].toLowerCase()) out[tint.paths[role]] = value
      }
    }
    return JSON.stringify(out, null, 2)
  }

  const copyPatch = () => {
    void navigator.clipboard?.writeText(patch()).catch(() => {})
    setCopied(true)
    setTimeout(() => setCopied(false), 1200)
  }

  const reset = () => {
    for (const tint of TINTS) {
      for (const { role } of ROLES) document.documentElement.style.removeProperty(beadVar(tint.tint, role))
    }
    rerender()
  }

  const changed = patch() !== '{}'

  return (
    <>
      <section className={styles.section}>
        <h2 className={styles.heading}>Bead colour</h2>
        <p className={styles.note}>
          Every bead in the system wears one of these five tints, and a tint is three values. Edits
          apply live to the real title-bar lights and to every knob and thumb — but only to beads: the
          toggle track, the buttons and the selection highlight keep the desktop’s accent.
        </p>

        <div className={styles.stack} style={{ maxWidth: 'none' }}>
          {TINTS.map((tint) => (
            <Surface key={tint.tint} variant="raised" className={styles.tintRow}>
              <div className={styles.tintPreview}>
                <LiquidBubble tint={tint.tint} size={18} />
                <LiquidBubble tint={tint.tint} size={40} phase={1.1} />
                <span className={styles.label} style={{ minWidth: 0 }}>
                  {tint.label}
                </span>
              </div>
              <div className={styles.tintFields}>
                {ROLES.map(({ role, label, hint }) => (
                  <div key={role}>
                    <span className={styles.label} title={hint}>
                      {label}
                    </span>
                    <ColorField
                      label={`${tint.label} ${label}`}
                      value={readVar(tint.tint, role, tint.defaults[role])}
                      onChange={(v) => set(tint.tint, role, v)}
                    />
                  </div>
                ))}
              </div>
            </Surface>
          ))}
        </div>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>In place</h2>
        <p className={styles.note}>
          The same tints as they are actually used. The track under these knobs is the desktop accent,
          so it should not move when you edit the knob.
        </p>
        <div className={styles.row}>
          <Toggle checked onChange={() => {}} label="On" />
          <Toggle checked={false} onChange={() => {}} label="Off" />
          <Button variant="primary">Primary</Button>
        </div>
      </section>

      <section className={styles.section}>
        <div className={styles.row}>
          <Button variant="primary" onClick={copyPatch} disabled={!changed}>
            {copied ? 'Copied' : 'Copy JSON patch'}
          </Button>
          <Button onClick={reset} disabled={!changed}>
            Reset to tokens
          </Button>
        </div>
        <pre className={styles.code}>{changed ? patch() : '// unchanged from tokens.json'}</pre>
      </section>
    </>
  )
}
