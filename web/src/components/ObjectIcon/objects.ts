/**
 * The object geometry, typed. Kept apart from ObjectIcon.tsx so that Icon can
 * read the fallback table without importing the renderer, which would put a
 * cycle between the two components.
 */

import raw from './objects.json'
import type { Facet, Finish, MaterialName, Shape } from './recipe'

export interface ObjectPart {
  id: string
  d: string
  facet?: Facet
  tone?: number
  bevel?: 'raised' | 'well' | 'none'
  role?: 'body' | 'crease' | 'mark'
  material?: MaterialName
  /** A dotted tokens.json path: a flat fill instead of the material ramp. */
  tint?: string
  /** Drop this part below this rendered size. */
  min?: number
  fillRule?: 'nonzero' | 'evenodd'
  /** How the surface is curved. The facet picks which stretch of the ramp a
   * face occupies; the shape decides how light travels across it. */
  shape?: Shape
  /** How deeply the part sits in what is behind it, as an inner shadow round
   * its own edge. What makes a screen look recessed rather than painted on. */
  ao?: number
  /** How hard a shadow this part throws on whatever is beneath it. */
  castShadow?: number
}

export interface ObjectDef {
  material: MaterialName
  /** 'glossy' adds Aqua's sweep over the lit passes; 'matte' takes the specular
   * cap and the room reflection away and caps the material's own gloss. */
  finish?: Finish
  silhouette: string
  /** The icons.json glyph to fall back to; defaults to the object's own name. */
  glyph?: string
  parts: ObjectPart[]
}

const { $description, $grid, ...defs } = raw as unknown as Record<string, ObjectDef> & {
  $description: string
  $grid: number
}

export const OBJECT_GRID = $grid
export const objectDefs = defs as Record<string, ObjectDef>

export type ObjectName = Exclude<keyof typeof raw, `$${string}`>

export const objectNames = Object.keys(objectDefs) as ObjectName[]

export function hasObject(name: string): name is ObjectName {
  return name in objectDefs
}

/** What this object renders as below object.tier-glyph-max. */
export function objectGlyph(name: ObjectName): string {
  return objectDefs[name].glyph ?? name
}
