import { describe, expect, it } from 'vitest'
import { charCount, lineCol, wordCount } from './textStats'

describe('text stats', () => {
  it('counts words characters lines and columns', () => {
    const text = 'Hello world\nsecond  line — café'
    expect(wordCount(text)).toBe(6)
    expect(charCount(text)).toBe(31)
    expect(lineCol(text, text.length)).toEqual({ line: 2, col: 20 })
    expect(lineCol(text, 0)).toEqual({ line: 1, col: 1 })
    expect(wordCount('')).toBe(0)
    expect(charCount('')).toBe(0)
    expect(wordCount('   ')).toBe(0)
  })
})
