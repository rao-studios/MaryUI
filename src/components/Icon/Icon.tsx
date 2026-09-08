/**
 * Icon — a small inline SVG glyph set, stroked in `currentColor` so it takes
 * any ink token. Add glyphs to `paths`; each is a 24×24 path list.
 */

import type { CSSProperties } from 'react'
import { cx } from '@/lib/cx'
import styles from './Icon.module.css'

const paths = {
  folder: ['M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z'],
  document: ['M7 3h7l5 5v13H7z', 'M14 3v5h5'],
  image: ['M4 5h16v14H4z', 'M4 16l5-5 4 4 3-3 4 4', 'M15.5 9.5a1 1 0 1 0 0-.01'],
  music: ['M9 18V5l10-2v13', 'M6 21a3 3 0 1 0 0-6 3 3 0 0 0 0 6z', 'M16 19a3 3 0 1 0 0-6 3 3 0 0 0 0 6z'],
  code: ['M8 7l-5 5 5 5', 'M16 7l5 5-5 5', 'M14 4l-4 16'],
  chevronLeft: ['M15 5l-7 7 7 7'],
  chevronRight: ['M9 5l7 7-7 7'],
  chevronDown: ['M5 9l7 7 7-7'],
  search: ['M11 4a7 7 0 1 0 0 14 7 7 0 0 0 0-14z', 'M16 16l5 5'],
  grid: ['M4 4h6v6H4z', 'M14 4h6v6h-6z', 'M4 14h6v6H4z', 'M14 14h6v6h-6z'],
  list: ['M4 6h16', 'M4 12h16', 'M4 18h16'],
  gear: ['M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8z', 'M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.2 2.2M16.9 16.9l2.2 2.2M4.9 19.1l2.2-2.2M16.9 7.1l2.2-2.2'],
  home: ['M3 11l9-7 9 7', 'M5 10v10h14V10'],
  desktop: ['M3 5h18v11H3z', 'M8 21h8M12 16v5'],
  download: ['M12 4v12', 'M7 11l5 5 5-5', 'M4 20h16'],
  star: ['M12 3l2.8 5.9 6.2.8-4.5 4.4 1.1 6.3L12 17.4l-5.6 3 1.1-6.3L3 9.7l6.2-.8z'],
  cloud: ['M7 18a4 4 0 0 1-.5-8 6 6 0 0 1 11.5 1.5A3.5 3.5 0 0 1 17.5 18z'],
  drive: ['M4 6h16v12H4z', 'M4 14h16', 'M16 16.5h1'],
  check: ['M5 12l5 5 9-10'],
  close: ['M6 6l12 12', 'M18 6L6 18'],
  plus: ['M12 5v14', 'M5 12h14'],
  minus: ['M5 12h14'],
  info: ['M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z', 'M12 11v6', 'M12 8h.01'],
  drop: ['M12 3s6 6.5 6 11a6 6 0 0 1-12 0c0-4.5 6-11 6-11z'],
} as const

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
