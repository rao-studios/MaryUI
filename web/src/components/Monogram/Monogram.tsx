/**
 * Monogram — the Rao mark: a serif R and its mirror image sharing a stem.
 * `flat` inherits `currentColor` for the menu bar; `platinum` fills the mark
 * with the metal ramp, the brushed grain, and a fixed specular for hero use.
 */

import { useId, type CSSProperties } from 'react'
import { MONOGRAM_SYMBOL_ID } from '@/components/SvgDefs'
import { sharedBrushDataUri } from '@/lib/textures'
import { cx } from '@/lib/cx'
import styles from './Monogram.module.css'

export interface MonogramProps {
  variant?: 'flat' | 'platinum'
  /** Rendered size in px (the mark is square). */
  size?: number
  className?: string
  style?: CSSProperties
  title?: string
}

export function Monogram({ variant = 'flat', size = 16, className, style, title = 'Rao' }: MonogramProps) {
  const uid = useId().replace(/:/g, '')
  const href = `#${MONOGRAM_SYMBOL_ID}`

  if (variant === 'flat') {
    return (
      <svg className={cx(styles.mark, styles.flat, className)} width={size} height={size} viewBox="0 0 200 200" style={style} role="img" aria-label={title}>
        <use href={href} />
      </svg>
    )
  }

  const gradient = `lp-mono-grad-${uid}`
  const pattern = `lp-mono-brush-${uid}`
  const spec = `lp-mono-spec-${uid}`
  const shadow = `lp-mono-shadow-${uid}`

  return (
    <svg className={cx(styles.mark, styles.platinum, className)} width={size} height={size} viewBox="0 0 200 200" style={style} role="img" aria-label={title}>
      <defs>
        <linearGradient id={gradient} x1="0" y1="0" x2="0" y2="1">
          <stop offset="0" stopColor="var(--lp-platinum-0)" />
          <stop offset="0.45" stopColor="var(--lp-platinum-4)" />
          <stop offset="0.5" stopColor="var(--lp-platinum-6)" />
          <stop offset="1" stopColor="var(--lp-platinum-3)" />
        </linearGradient>
        <linearGradient id={spec} x1="0" y1="0" x2="1" y2="1">
          <stop offset="0.3" stopColor="#fff" stopOpacity="0" />
          <stop offset="0.5" stopColor="#fff" stopOpacity="0.7" />
          <stop offset="0.7" stopColor="#fff" stopOpacity="0" />
        </linearGradient>
        <pattern id={pattern} patternUnits="userSpaceOnUse" width="128" height="128">
          <image href={sharedBrushDataUri()} width="128" height="128" />
        </pattern>
        <filter id={shadow} x="-20%" y="-20%" width="140%" height="140%">
          <feDropShadow dx="0" dy="4" stdDeviation="4" floodColor="#000" floodOpacity="0.35" />
        </filter>
      </defs>
      <g filter={`url(#${shadow})`}>
        <use href={href} fill={`url(#${gradient})`} />
      </g>
      <use href={href} fill={`url(#${pattern})`} style={{ mixBlendMode: 'overlay', opacity: 0.9 }} />
      <use href={href} fill={`url(#${spec})`} style={{ mixBlendMode: 'screen' }} />
      <use href={href} fill="none" stroke="rgba(0,0,0,0.35)" strokeWidth="1" />
    </svg>
  )
}
