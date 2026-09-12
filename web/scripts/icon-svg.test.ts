/**
 * The export library mirrors recipe.ts rather than importing it: Sketch has no
 * CSS variables, so it bakes `accent` and `folder` to a literal ramp where the
 * runtime must leave them as var(). Everything else has to agree exactly, and
 * nothing but this test would notice if it stopped.
 */

import { describe, expect, it } from 'vitest'
// @ts-expect-error — plain ESM module without a declaration file.
import * as exporter from './icon-svg-lib.mjs'
import { FACETS, MATERIALS, facetStops, isDynamic, rampAt } from '../src/components/ObjectIcon/recipe'
import { tokens } from '../src/tokens/tokens'

describe('the .svg exporter', () => {
  it('knows the same materials and facets as the renderer', () => {
    expect([...exporter.MATERIALS].sort()).toEqual([...MATERIALS].sort())
    expect([...exporter.FACETS].sort()).toEqual([...FACETS].sort())
    expect(exporter.HEADROOM).toBe(tokens.object.headroom)
    expect(exporter.VIEWBOX).toBe(tokens.object.viewbox)
  })

  it('computes the same ramp for every literal material and facet', () => {
    for (const material of MATERIALS) {
      if (isDynamic(material)) continue
      for (const facet of FACETS)
        expect(exporter.facetStops(material, facet), `${material}/${facet}`).toEqual(
          facetStops(material, facet),
        )
    }
  })

  it('bakes the switchable materials to a real ramp instead of a variable', () => {
    for (const material of MATERIALS) {
      if (!isDynamic(material)) continue
      /* The renderer emits color-mix over CSS variables; Sketch cannot read those. */
      expect(rampAt(material, 0.5)).toContain('var(--lp-')
      for (const stop of exporter.facetStops(material, 'top'))
        expect(stop, `${material} baked`).toMatch(/^#[0-9a-f]{6}$/)
    }
  })

  it('bakes folders to the shipped appearance', () => {
    expect(exporter.BAKED.folder).toBe(tokens.object.folderAppearance)
  })

  it('scopes every referenced id to its object, so a page of them can share one document', () => {
    /* The Sketch contact sheet inlines all of them; an unscoped id resolves to the first object that declared it. */
    const seen = new Map<string, string>()
    for (const name of exporter.objectNames as string[]) {
      const svg: string = exporter.objectSvg(name, exporter.objects[name])
      const defs = [...svg.matchAll(/<(?:clipPath|linearGradient)\s+id="([^"]+)"/g)].map((m) => m[1])
      for (const [, ref] of svg.matchAll(/url\(#([^)]+)\)/g)) expect(defs, `${name} → #${ref}`).toContain(ref)
      for (const id of defs) {
        expect(seen.get(id), `#${id} in ${name}`).toBeUndefined()
        seen.set(id, name)
      }
    }
  })
})
