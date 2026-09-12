/**
 * How much of the recipe an object gets, as a function of its rendered size.
 *
 * Tier is a function of the CSS size ONLY — never devicePixelRatio. A
 * dpr-dependent tier would draw the same icon differently on two monitors and
 * would need a resize listener, and a listener costs frames on a desktop whose
 * whole contract is that an idle screen schedules none.
 */

import { tokens } from '@/tokens/tokens'

export type IconTier = 'glyph' | 'plain' | 'lit'

/** ≤19px. Four tonal layers and a 1px keyline inside a 16px box cancel to grey. */
export const GLYPH_MAX = Number.parseFloat(tokens.object.tierGlyphMax)
/** ≤27px. Shape and material family read; the finish does not. */
export const PLAIN_MAX = Number.parseFloat(tokens.object.tierPlainMax)

export function iconTier(size: number): IconTier {
  if (size <= GLYPH_MAX) return 'glyph'
  if (size <= PLAIN_MAX) return 'plain'
  return 'lit'
}
