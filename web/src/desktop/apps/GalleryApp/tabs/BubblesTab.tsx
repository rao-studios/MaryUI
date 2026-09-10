/** LiquidBubble at every tint and size. Drag the window to see them slosh. */

import { LiquidBubble, type BubbleTint } from '@/components/LiquidBubble'
import { Surface } from '@/components/Surface'
import styles from '../GalleryApp.module.css'

const tints: BubbleTint[] = ['close', 'minimize', 'zoom', 'accent', 'platinum', 'inactive']
const sizes = [12, 20, 32, 56, 88]

export function BubblesTab() {
  return (
    <>
      <section className={styles.section}>
        <h2 className={styles.heading}>Liquid bubbles</h2>
        <p className={styles.note}>
          Pure CSS: two rounded layers roll at different periods behind an embossed crest line, and the window's motion
          engine tilts them through <code className={styles.mono}> --lp-slosh</code>. Grab this window's title bar and
          flick it.
        </p>
        <Surface variant="flat" className={styles.bubbleRow}>
          {sizes.map((size, i) => (
            <LiquidBubble key={size} tint="accent" size={size} phase={i * 1.3} />
          ))}
        </Surface>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>One bead, everywhere</h2>
        <p className={styles.note}>
          The same object is the Toggle's knob, the Slider's thumb and a window's traffic light — a glass well of
          liquid with a surface line across it. Only the tint, the size and the fill change.
        </p>
        <Surface variant="titlebar" className={styles.bubbleRow}>
          {(['close', 'minimize', 'zoom', 'accent', 'platinum'] as const).map((tint, i) => (
            <LiquidBubble key={tint} tint={tint} size={32} phase={i * 1.1} />
          ))}
        </Surface>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>Tints</h2>
        <div className={styles.row}>
          {tints.map((tint, i) => (
            <div key={tint} style={{ display: 'grid', justifyItems: 'center', gap: 6 }}>
              <LiquidBubble tint={tint} size={40} phase={i * 0.9} />
              <span className={styles.mono} style={{ color: 'var(--lp-ink-tertiary)' }}>
                {tint}
              </span>
            </div>
          ))}
        </div>
      </section>

      <section className={styles.section}>
        <h2 className={styles.heading}>Fill levels</h2>
        <div className={styles.row}>
          {[0.2, 0.4, 0.6, 0.8, 1].map((fill, i) => (
            <div key={fill} style={{ display: 'grid', justifyItems: 'center', gap: 6 }}>
              <LiquidBubble tint="zoom" size={40} fill={fill} phase={i * 0.7} />
              <span className={styles.mono} style={{ color: 'var(--lp-ink-tertiary)' }}>
                {fill}
              </span>
            </div>
          ))}
        </div>
      </section>
    </>
  )
}
