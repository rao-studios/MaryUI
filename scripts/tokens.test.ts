/** Unit tests for the token transforms in tokens-lib.mjs. */

import { describe, expect, it } from 'vitest'
// @ts-expect-error — plain ESM module without a declaration file.
import { buildAll, cssName, flatten, parseColor, resolveAliases, toCss, toCssValue } from './tokens-lib.mjs'
import tree from '../tokens/tokens.json'

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

describe('the real tokens.json', () => {
  const out = buildAll(tree)

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
    expect(out.ts).toMatch(/sloshFrequency: 1\.7/)
  })
})
