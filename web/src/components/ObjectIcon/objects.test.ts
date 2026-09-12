/**
 * The object tier's contract. Three things are load-bearing: the geometry is
 * well formed, every small-size fallback resolves to a real glyph, and the
 * header the C desktop reads still says what this file says — objects are on
 * both targets now, and `linux/include/maryui/lp_objects.h` is generated from
 * here (`npm run tokens`, enforced by `make gen-check`).
 *
 * The structural checks read their allowed values out of objects.schema.json
 * rather than repeating them, so the schema stays the single source of truth
 * for what a part may say and the test cannot drift from it.
 */

import { readFileSync } from 'node:fs'
import { describe, expect, it } from 'vitest'
import objects from './objects.json'
import schema from './objects.schema.json'
import icons from '../Icon/icons.json'
import tokens from '../../../tokens/tokens.json'

interface Part {
  id: string
  d: string
  facet?: string
  tone?: number
  bevel?: string
  role?: string
  material?: string
  tint?: string
  min?: number
  fillRule?: string
}
interface ObjectIconDef { material: string; silhouette: string; glyph?: string; parts: Part[] }

const entries = Object.entries(objects).filter(([k]) => !k.startsWith('$')) as [string, ObjectIconDef][]
const glyphNames = new Set(Object.keys(icons))

const partSchema = schema.$defs.part
const partKeys = new Set(Object.keys(partSchema.properties))
const materials = new Set(schema.$defs.material.enum)
const enumOf = (key: string) =>
  new Set((partSchema.properties as Record<string, { enum?: string[] }>)[key].enum)

describe('objects.json', () => {
  it('is on the 32 grid the object tokens declare', () => {
    expect((objects as { $grid: number }).$grid).toBe(schema.properties.$grid.const)
  })

  it('says only what the schema allows', () => {
    for (const [name, icon] of entries) {
      expect(materials.has(icon.material), `${name}.material`).toBe(true)
      for (const part of icon.parts) {
        const where = `${name}.${part.id}`
        expect(Object.keys(part).filter((k) => !partKeys.has(k)), where).toEqual([])
        expect(part.d, where).toBeTruthy()
        if (part.facet) expect(enumOf('facet').has(part.facet), where).toBe(true)
        if (part.bevel) expect(enumOf('bevel').has(part.bevel), where).toBe(true)
        if (part.role) expect(enumOf('role').has(part.role), where).toBe(true)
        if (part.material) expect(materials.has(part.material), where).toBe(true)
        if (part.tone !== undefined) expect(Math.abs(part.tone), where).toBeLessThanOrEqual(1)
      }
    }
  })

  it('names a silhouette that is one of its own parts', () => {
    for (const [name, icon] of entries)
      expect(icon.parts.map((p) => p.id), name).toContain(icon.silhouette)
  })

  it('gives every part a unique id', () => {
    for (const [name, icon] of entries) {
      const ids = icon.parts.map((p) => p.id)
      expect(new Set(ids).size, name).toBe(ids.length)
    }
  })

  it('resolves every small-size glyph fallback', () => {
    for (const [name, icon] of entries) {
      const fallback = icon.glyph ?? name
      expect(glyphNames.has(fallback), `${name} falls back to "${fallback}"`).toBe(true)
    }
  })

  it('resolves every tint to a real token', () => {
    const at = (path: string) =>
      path.split('.').reduce<unknown>(
        (node, key) => (node == null ? node : (node as Record<string, unknown>)[key]),
        tokens,
      ) as { $value?: unknown } | undefined
    for (const [name, icon] of entries)
      for (const part of icon.parts)
        if (part.tint) expect(at(part.tint)?.$value, `${name}.${part.id} → ${part.tint}`).toBeTruthy()
  })

  it('keeps ink inside the 32 grid', () => {
    /*
     * Path data packs numbers without separators — "l-4.2.93" is -4.2 then .93 —
     * so this has to match SVG's number grammar, including the leading-dot form.
     * A naive /-?\d+(\.\d+)?/ reads that .93 as 93 and reports a false breach.
     */
    const NUMBER = /-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?/g
    for (const [name, icon] of entries)
      for (const part of icon.parts)
        for (const n of part.d.match(NUMBER) ?? [])
          expect(Math.abs(Number(n)), `${name}.${part.id} — "${n}"`).toBeLessThanOrEqual(32)
  })

  it('carries no colour of its own — lighting lives in tokens.json', () => {
    /* Checked over the icons only: the file's own $description talks about colour. */
    const geometry = JSON.stringify(Object.fromEntries(entries))
    expect(geometry).not.toMatch(/#[0-9a-f]{3,8}\b|rgba?\(|gradient/i)
  })
})

describe('the C pipeline', () => {
  /*
   * The object tier used to be web-only, and this described that: it asserted
   * neither build script so much as named objects.json. The C renderer is in
   * scope now (linux/src/draw/lp_object_icon.c), so what has to hold instead is
   * that the generated header still says what the JSON says — `make gen-check`
   * fails when the two drift, and this fails when the emitter drops something.
   */
  const header = readFileSync(new URL('../../../../linux/include/maryui/lp_objects.h', import.meta.url), 'utf8')

  it('emits every object, in JSON order, with its parts', () => {
    const cName = (name: string) =>
      `LP_OBJ_${name.replace(/([a-z0-9])([A-Z])/g, '$1_$2').replace(/[^A-Za-z0-9]+/g, '_').toUpperCase()}`
    const enumNames = Array.from(header.matchAll(/^ {4}(LP_OBJ_[A-Z0-9_]+),$/gm), (m) => m[1])
      .filter((n) => !n.startsWith('LP_OBJ_MATERIAL_') && n !== 'LP_OBJ_COUNT')
    expect(enumNames).toEqual(entries.map(([name]) => cName(name)))
    expect(header).toContain(`#define LP_OBJ_GRID ${(objects as { $grid: number }).$grid}`)
    for (const [name, def] of entries) {
      expect(header).toContain(`static const lp_obj_part ${cName(name)}_PARTS[] = {`)
      expect(header).toContain(`${cName(name)}_PARTS, ${def.parts.length} },`)
    }
  })

  it('carries every part\'s geometry and no colour of its own', () => {
    for (const [, def] of entries) {
      for (const part of def.parts) expect(header).toContain(JSON.stringify(part.d))
    }
    const body = header.slice(header.indexOf('static const lp_obj_part'))
    expect(body).not.toMatch(/#[0-9a-f]{3,8}\b|rgba?\(/i)
  })

  it('gives every object a glyph to fall back to below the lit tier', () => {
    for (const [name, def] of entries) {
      const glyph = def.glyph ?? name
      expect(glyphNames.has(glyph)).toBe(true)
      expect(header).toContain(`${JSON.stringify(glyph)}, LP_OBJ_${name.replace(/([a-z0-9])([A-Z])/g, '$1_$2').replace(/[^A-Za-z0-9]+/g, '_').toUpperCase()}_PARTS`)
    }
  })
})
