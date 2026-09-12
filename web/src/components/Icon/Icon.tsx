/**
 * Icon — the one entry point for both tiers of mark.
 *
 * `glyph` (the default) is a stroked outline in `currentColor`, from
 * icons.json — a 24x24 path list each, so the C desktop's lp_icons.h is
 * generated from the same file; add glyphs there.
 *
 * `object` asks for the lit tier: a drawn object rather than a diagram of one,
 * from ObjectIcon. It is a request and not a guarantee — a name with no
 * geometry, or any size at or below object.tier-glyph-max, falls back to the
 * glyph. That fallback is the point rather than a shortfall: at 14px a mark
 * should be a mark, and it is why the sidebar, the list rows and the icons
 * inside controls look exactly as they did.
 */

import type { CSSProperties } from 'react'
import { cx } from '@/lib/cx'
import { ObjectIcon, hasObject, iconTier, objectGlyph, type ObjectName } from '@/components/ObjectIcon'
import icons from './icons.json'
import styles from './Icon.module.css'

const paths = icons as Record<keyof typeof icons, readonly string[]>

/** A stroked control mark. */
export type GlyphName = keyof typeof paths

/** Anything nameable: every glyph, plus the objects that have no glyph of their own. */
export type IconName = GlyphName | ObjectName

export const iconNames = Object.keys(paths) as GlyphName[]

export interface IconProps {
  name: IconName
  size?: number
  strokeWidth?: number
  /** 'object' asks for the lit tier where the name and the size both allow it. */
  variant?: 'glyph' | 'object'
  className?: string
  style?: CSSProperties
  title?: string
}

export function Icon({
  name,
  size = 16,
  strokeWidth = 1.7,
  variant = 'glyph',
  className,
  style,
  title,
}: IconProps) {
  if (variant === 'object' && hasObject(name) && iconTier(size) !== 'glyph')
    return <ObjectIcon name={name as ObjectName} size={size} className={className} style={style} title={title} />

  const glyph = (hasObject(name) ? objectGlyph(name) : name) as GlyphName
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
      {paths[glyph].map((d, i) => (
        <path key={i} d={d} />
      ))}
    </svg>
  )
}
