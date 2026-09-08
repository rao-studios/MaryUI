/** AboutApp — the platinum monogram, the system's name, and its version. */

import { LiquidBubble } from '@/components/LiquidBubble'
import { Monogram } from '@/components/Monogram'
import type { AppProps } from '@/desktop/apps/registry'
import styles from './AboutApp.module.css'

export function AboutApp(_props: AppProps) {
  return (
    <div className={styles.about}>
      <Monogram variant="platinum" size={150} className={styles.mark} />
      <h1 className={styles.title}>Liquid Platinum</h1>
      <p className={styles.subtitle}>Rao design system · version 0.1.0</p>
      <p className={styles.blurb}>
        Brushed platinum that moves like liquid. Every surface is a token, every window a physics target.
      </p>
      <div className={styles.bubbles} aria-hidden="true">
        <LiquidBubble tint="close" size={14} phase={0} />
        <LiquidBubble tint="minimize" size={14} phase={1.5} />
        <LiquidBubble tint="zoom" size={14} phase={3} />
        <LiquidBubble tint="accent" size={14} phase={4.5} />
      </div>
    </div>
  )
}
