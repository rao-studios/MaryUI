/**
 * Icon — a small inline SVG glyph set, stroked in `currentColor` so it takes
 * any ink token. The glyphs live in icons.json (a 24×24 path list each) so
 * the C desktop's lp_icons.h is generated from the same file; add glyphs there.
 */

import type { CSSProperties } from 'react'
import { cx } from '@/lib/cx'
import icons from './icons.json'
import styles from './Icon.module.css'

const paths = icons as Record<keyof typeof icons, readonly string[]>

export type IconName = keyof typeof paths

export const iconNames = Object.keys(paths) as IconName[]

export interface IconProps {
  name: IconName
  size?: number
  strokeWidth?: number
  className?: string
  style?: CSSProperties
  title?: string
}

export function Icon({ name, size = 16, strokeWidth = 1.7, className, style, title }: IconProps) {
  return (
    <svg
      className={cx(styles.icon, className)}
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={strokeWidth}
      strokeLinecap="round"
      strokeLinejoin="round"
      style={style}
      role={title ? 'img' : undefined}
      aria-hidden={title ? undefined : 'true'}
    >
      {title ? <title>{title}</title> : null}
      {paths[name].map((d, i) => (
        <path key={i} d={d} />
      ))}
    </svg>
  )
}
