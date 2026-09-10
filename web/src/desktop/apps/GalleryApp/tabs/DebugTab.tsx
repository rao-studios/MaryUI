/**
 * Switches for seeing the machinery: the painted crest hairline, the raw blobs
 * behind the merge filter, and a freeze on the ambient wave so the slosh can be
 * judged on its own. All of them live in `settings.ts`, so they survive a
 * reload and are mirrored onto <html data-*> for CSS to pick up.
 */

import { Toggle } from '@/components/Toggle'
import { LiquidBubble } from '@/components/LiquidBubble'
import { Surface } from '@/components/Surface'
import { updateSettings, useSettings, type Settings } from '@/desktop/settings'
import styles from '../GalleryApp.module.css'

interface Switch {
  key: keyof Pick<Settings, 'crestLine' | 'showMergeLayer' | 'freezeLiquid'>
  label: string
  note: string
}

const SWITCHES: Switch[] = [
  {
    key: 'crestLine',
    label: 'Liquid crest line',
    note: 'A painted hairline across each bead’s waterline, lit above and shadowed below. Useful for reading the slosh exactly; too loud for a finished bead, where the liquid’s own top highlight does the job.',
  },
  {
    key: 'showMergeLayer',
    label: 'Show merge layer',
    note: 'Strip the filter off the goo layer and outline it, so you can see the raw blobs it is being handed rather than only the silhouette it returns.',
  },
  {
    key: 'freezeLiquid',
    label: 'Freeze liquid roll',
    note: 'Hold the ambient wave still. Drag a window with this on to see the slosh with nothing else moving.',
  },
]

export function DebugTab() {
  const settings = useSettings()

  return (
    <>
      <section className={styles.section}>
        <h2 className={styles.heading}>Debug</h2>
        <p className={styles.note}>
          These are switches for looking at the system, not preferences for using it. They persist, so
          turn them off when you are done.
        </p>
        <div className={styles.stack}>
          {SWITCHES.map((s) => (
            <div key={s.key}>
              <div className={styles.row} style={{ marginBottom: 4 }}>
                <Toggle
                  checked={settings[s.key]}
                  onChange={(checked) => updateSettings({ [s.key]: checked })}
                  label={s.label}
                />
                <span style={{ fontSize: 'var(--lp-text-md)' }}>{s.label}</span>
              </div>
              <p className={styles.note} style={{ margin: 0 }}>
                {s.note}
              </p>
            </div>
          ))}
        </div>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>Waterline</h2>
        <p className={styles.note}>
          The same beads at the size they are used. Toggle the crest line above and watch what changes:
          the waterline itself is the boundary between pale glass and colour, and it tilts with the
          slosh whether or not the hairline is drawn on top of it.
        </p>
        <Surface variant="titlebar" className={styles.bubbleRow}>
          {(['close', 'minimize', 'zoom'] as const).map((tint, i) => (
            <LiquidBubble key={tint} tint={tint} size={18} phase={i * 1.4} />
          ))}
          {(['close', 'minimize', 'zoom'] as const).map((tint, i) => (
            <LiquidBubble key={`${tint}-lg`} tint={tint} size={56} phase={i * 1.4} />
          ))}
        </Surface>
      </section>
    </>
  )
}
