/** Unit tests for the token transforms in tokens-lib.mjs. */

import { describe, expect, it } from 'vitest'
// @ts-expect-error — plain ESM module without a declaration file.
import { buildAll, cDimension, cDurationMs, cFloat, cIconName, cName, cssName, flatten, parseColor, resolveAliases, toCss, toCssValue, toIconsC } from './tokens-lib.mjs'
import tree from '../tokens/tokens.json'
import icons from '../src/components/Icon/icons.json'

describe('flatten', () => {
  it('inherits $type from groups and keeps source order', () => {
    const tokens = flatten({ a: { $type: 'color', x: { $value: '#fff' }, y: { $value: '#000' } } })
    expect(tokens.map((t: { path: string[] }) => t.path.join('.'))).toEqual(['a.x', 'a.y'])
    expect(tokens[0].type).toBe('color')
  })
})

describe('resolveAliases', () => {
  it('resolves whole and embedded references', () => {
    const tokens = resolveAliases(
      flatten({
        p: { $type: 'color', one: { $value: '#123456' } },
        s: { $type: 'color', a: { $value: '{p.one}' }, b: { $value: 'linear-gradient({p.one}, {s.a})' } },
      }),
    )
    expect(tokens[1].value).toBe('#123456')
    expect(tokens[2].value).toBe('linear-gradient(#123456, #123456)')
  })

  it('throws on cycles and unknown references', () => {
    expect(() => resolveAliases(flatten({ a: { $value: '{b}' }, b: { $value: '{a}' } }))).toThrow(/cycle/)
    expect(() => resolveAliases(flatten({ a: { $value: '{nope}' } }))).toThrow(/Unknown/)
  })
})

describe('css', () => {
  it('names variables with the lp prefix and dashes', () => {
    expect(cssName(['motion', 'spring-jelly', 'frequency'])).toBe('--lp-motion-spring-jelly-frequency')
  })

  it('serializes shadows and font stacks', () => {
    expect(
      toCssValue({
        type: 'shadow',
        value: [{ inset: true, offsetX: '0', offsetY: '1px', blur: '0', spread: '0', color: 'red' }],
      }),
    ).toBe('inset 0 1px 0 0 red')
    expect(toCssValue({ type: 'fontFamily', value: ['SF Pro Text', 'Arial', 'sans-serif'] })).toBe(
      '"SF Pro Text", Arial, sans-serif',
    )
  })

  it('emits a dark block only when a token has a dark extension', () => {
    const plain = toCss(resolveAliases(flatten({ a: { $type: 'color', $value: '#fff' } })))
    expect(plain).not.toContain("data-theme='dark'")
    const dark = toCss(
      resolveAliases(
        flatten({ a: { $type: 'color', $value: '#fff', $extensions: { 'com.rao.lp': { dark: '#000' } } } }),
      ),
    )
    expect(dark).toContain("data-theme='dark'")
  })
})

describe('parseColor', () => {
  it('handles hex and rgba forms with 0..1 channels', () => {
    expect(parseColor('#ff0000')).toEqual({ red: 1, green: 0, blue: 0, alpha: 1 })
    expect(parseColor('#0f0')).toEqual({ red: 0, green: 1, blue: 0, alpha: 1 })
    expect(parseColor('rgba(0, 0, 255, 0.5)')).toEqual({ red: 0, green: 0, blue: 1, alpha: 0.5 })
    expect(parseColor('nonsense')).toBeNull()
  })
})

describe('c', () => {
  it('names macros with the LP prefix and underscores', () => {
    expect(cName(['accent', 'blue', 'focus-ring'])).toBe('LP_ACCENT_BLUE_FOCUS_RING')
    expect(cName(['platinum', '0'])).toBe('LP_PLATINUM_0')
    expect(cIconName('chevronLeft')).toBe('LP_ICON_CHEVRON_LEFT')
  })

  it('formats numbers and units the way C wants them', () => {
    expect(cFloat(12)).toBe('12.0f')
    expect(cFloat(0.00005)).toBe('0.00005f')
    expect(cDimension('12px')).toEqual({ unit: 'px', number: 12 })
    expect(cDimension('105deg')).toEqual({ unit: 'deg', number: 105 })
    expect(cDurationMs('120ms')).toBe(120)
    expect(cDurationMs('7s')).toBe(7000)
  })

  it('emits the icon table in JSON order', () => {
    const header = toIconsC({ folder: ['M1 1'], chevronLeft: ['M2 2', 'M3 3'] })
    expect(header).toContain('LP_ICON_FOLDER,\n    LP_ICON_CHEVRON_LEFT,\n    LP_ICON_COUNT,')
    expect(header).toContain('#define LP_ICON_PATH_MAX 2')
    expect(header).toContain('{ "M2 2", "M3 3", NULL }')
  })
})

describe('the real tokens.json', () => {
  const out = buildAll(tree, icons, { sourceHash: 'abc', iconsHash: 'def' })

  /*
   * lp_icon.c includes lp_tokens.h and lp_icons.h together, so a name defined by
   * both is a macro redefinition and the C desktop stops compiling. An `icon`
   * token group would have done exactly that: cName() maps icon.viewbox to
   * LP_ICON_VIEWBOX and icon.stroke to LP_ICON_STROKE, which lp_icons.h already
   * owns. That is why the object tier's tokens live under `object`.
   */
  it('defines no macro that lp_icons.h already defines', () => {
    const names = (src: string) => new Set(Array.from(src.matchAll(/^#define (LP_[A-Z0-9_]+)/gm), (m) => m[1]))
    const clash = [...names(out.c)].filter((name) => names(out.iconsC).has(name))
    expect(clash).toEqual([])
  })

  it('keeps the Spotlight panel concentric with the field inside it', () => {
    /*
     * The field is a pill, so its corner is half its own height; add the panel's
     * padding and you get the radius at which the two arcs share a centre and
     * the gap between them stays even the whole way round. Nothing else in the
     * file would notice if a later change to the bar height or the padding left
     * the panel behind.
     */
    const px = (path: string) =>
      Number.parseFloat(out.tokens.find((t: { path: string[] }) => t.path.join('.') === path).value)
    const field = px('size.spotlight-bar-height') / 2
    expect(px('radius.spotlight')).toBe(field + px('space.2'))
  })

  it('keeps the object tier under a prefix of its own', () => {
    expect(out.c).toContain('#define LP_OBJECT_VIEWBOX 32')
    expect(out.c).toContain('#define LP_OBJECT_TIER_GLYPH_MAX 19.0f')
    // light-azimuth aliases goo.specular-azimuth: icons are lit by the same lamp as the beads.
    expect(out.c).toContain('#define LP_OBJECT_LIGHT_AZIMUTH 250.0f')
    expect(out.c).toContain('#define LP_OBJECT_MATERIAL_MANILA_MID')
  })

  it('emits the C header with every token type', () => {
    expect(out.c).toContain('#define LP_PLATINUM_0 ((lp_color){ 0.9686f, 0.9686f, 0.9765f, 1.0f })')
    expect(out.c).toContain('#define LP_SURFACE_WINDOW_TOP ((lp_color){ 0.9333f, 0.9373f, 0.949f, 1.0f })')
    expect(out.c).toContain('#define LP_RADIUS_WINDOW 12.0f')
    expect(out.c).toContain('#define LP_SHEEN_ANGLE_DEG 105.0f')
    expect(out.c).toContain('#define LP_MOTION_FAST_MS 120.0f')
    expect(out.c).toContain('#define LP_LIQUID_WAVE_PERIOD_MS 3400.0f')
    // A very small float must not come through in scientific notation.
    expect(out.c).toContain('#define LP_MOTION_SLOSH_GAIN 0.0009f')
    expect(out.c).toContain('#define LP_Z_WINDOWS 100')
    expect(out.c).toContain('#define LP_TEXT_WEIGHT_SEMIBOLD 600')
    expect(out.c).toContain('#define LP_MOTION_SPRING_JELLY ((lp_spring_params){ 2.2f, 0.55f })')
    expect(out.c).toContain('#define LP_MOTION_EASE_SPRING ((lp_cubic_bezier){ 0.34f, 1.56f, 0.64f, 1.0f })')
    expect(out.c).toContain('#define LP_SHADOW_WINDOW_COUNT 2')
    expect(out.c).toContain('{ 1, 0.0f, 1.0f, 0.0f, 0.0f, ((lp_color){ 1.0f, 1.0f, 1.0f, 0.78f }) }')
    expect(out.c).toContain('"P052"')
    expect(out.c).toContain('#define LP_FONT_UI_PANGO "-apple-system, BlinkMacSystemFont, SF Pro Text, Helvetica Neue, Helvetica, Arial, Inter, sans-serif"')
    expect(out.c).toContain('#define LP_TOKENS_SOURCE_SHA "abc"')
    const count = Number(/#define LP_TOKEN_COUNT (\d+)/.exec(out.c)?.[1])
    expect(count).toBe(out.tokens.length)
    expect(out.c).not.toMatch(/\{[a-z]+\.[a-z]/)
  })

  it('emits every icon into the C header', () => {
    expect(out.iconsC).toContain(`#define LP_ICON_PATH_MAX ${Math.max(...Object.values(icons).map((p) => p.length))}`)
    expect(out.iconsC?.split("\n").filter((l: string) => l.startsWith('    LP_ICON_') && !l.includes('COUNT')).length).toBe(Object.keys(icons).length)
    expect(out.iconsC).toContain('LP_ICON_FOLDER,')
  })

  it('builds without unresolved aliases', () => {
    expect(out.css).not.toMatch(/\{[a-z]/)
    expect(out.tokens.length).toBeGreaterThan(100)
  })

  it('produces a valid Sketch palette with as many entries as the index', () => {
    const palette = JSON.parse(out.palette)
    expect(palette.compatibleVersion).toBe('2.0')
    expect(palette.colors.length).toBe(out.index.trim().split('\n').length)
    for (const c of palette.colors) {
      for (const ch of ['red', 'green', 'blue', 'alpha']) {
        expect(c[ch]).toBeGreaterThanOrEqual(0)
        expect(c[ch]).toBeLessThanOrEqual(1)
      }
    }
  })

  it('exposes motion constants as numbers in the TypeScript output', () => {
    expect(out.ts).toContain('springJelly: {')
    // Read the expected value from the source rather than pinning a tuned
    // number here: these emitter tests are about the emitter, not the design.
    expect(out.ts).toContain(`sloshFrequency: ${tree.motion['slosh-frequency'].$value}`)
  })
})
