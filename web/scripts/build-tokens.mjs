#!/usr/bin/env node
/**
 * Reads tokens/tokens.json (and src/components/Icon/icons.json) and writes:
 *   src/styles/tokens.css                       CSS custom properties (--lp-*)
 *   src/tokens/tokens.ts                        typed constants + cssVar()
 *   tokens/export/liquid-platinum.sketchpalette Sketch Palettes import
 *   tokens/export/palette-index.txt             token names in palette order
 *   ../linux/include/maryui/lp_tokens.h         the same tokens for the C desktop (MaryUI/linux)
 *   ../linux/include/maryui/lp_icons.h          the icon paths for the C desktop
 *   ../linux/include/maryui/lp_objects.h        the object tier's geometry for the C desktop
 *
 * Run via `npm run tokens`; `npm run dev` and `npm run build` run it first and
 * the Vite plugin re-runs it whenever tokens.json or icons.json changes.
 * `--out <dir>` writes every file flat into <dir> instead (make gen-check).
 */

import { createHash } from 'node:crypto'
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { basename, dirname, join } from 'node:path'
import { buildAll } from './tokens-lib.mjs'

const repo = join(dirname(fileURLToPath(import.meta.url)), '..')
const source = join(repo, 'tokens', 'tokens.json')
const iconsSource = join(repo, 'src', 'components', 'Icon', 'icons.json')
const objectsSource = join(repo, 'src', 'components', 'ObjectIcon', 'objects.json')

let outDir = null
const args = process.argv.slice(2)
for (let i = 0; i < args.length; i++) {
  if (args[i] === '--out') outDir = args[++i]
}

const sha = (text) => createHash('sha256').update(text).digest('hex').slice(0, 16)
const sourceText = readFileSync(source, 'utf8')
const iconsText = readFileSync(iconsSource, 'utf8')
const objectsText = readFileSync(objectsSource, 'utf8')
const out = buildAll(JSON.parse(sourceText), JSON.parse(iconsText),
  { sourceHash: sha(sourceText), iconsHash: sha(iconsText), objectsHash: sha(objectsText) }, JSON.parse(objectsText))

const targets = [
  ['src/styles/tokens.css', out.css],
  ['src/tokens/tokens.ts', out.ts],
  ['tokens/export/liquid-platinum.sketchpalette', out.palette],
  ['tokens/export/palette-index.txt', out.index],
  ['../linux/include/maryui/lp_tokens.h', out.c],
  ['../linux/include/maryui/lp_icons.h', out.iconsC],
  ['../linux/include/maryui/lp_objects.h', out.objectsC],
]

for (const [rel, content] of targets) {
  const file = outDir ? join(outDir, basename(rel)) : join(repo, rel)
  mkdirSync(dirname(file), { recursive: true })
  writeFileSync(file, content)
}

console.log(`tokens: ${out.tokens.length} tokens → ${targets.map(([rel]) => rel).join(', ')}`)
