#!/usr/bin/env node
/**
 * Reads tokens/tokens.json and writes:
 *   src/styles/tokens.css                       CSS custom properties (--lp-*)
 *   src/tokens/tokens.ts                        typed constants + cssVar()
 *   tokens/export/liquid-platinum.sketchpalette Sketch Palettes import
 *   tokens/export/palette-index.txt             token names in palette order
 *
 * Run via `npm run tokens`; `npm run dev` and `npm run build` run it first and
 * the Vite plugin re-runs it whenever tokens.json changes.
 */

import { readFileSync, writeFileSync, mkdirSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'
import { buildAll } from './tokens-lib.mjs'

const repo = join(dirname(fileURLToPath(import.meta.url)), '..')
const source = join(repo, 'tokens', 'tokens.json')

const tree = JSON.parse(readFileSync(source, 'utf8'))
const out = buildAll(tree)

const targets = [
  ['src/styles/tokens.css', out.css],
  ['src/tokens/tokens.ts', out.ts],
  ['tokens/export/liquid-platinum.sketchpalette', out.palette],
  ['tokens/export/palette-index.txt', out.index],
]

for (const [rel, content] of targets) {
  const file = join(repo, rel)
  mkdirSync(dirname(file), { recursive: true })
  writeFileSync(file, content)
}

console.log(`tokens: ${out.tokens.length} tokens → ${targets.map(([rel]) => rel).join(', ')}`)
