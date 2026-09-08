#!/usr/bin/env node
/**
 * Builds design/liquid-platinum.sketch from the tokens and the component
 * anatomy: Color Variables for every color token, a symbol master per
 * component variant, a Components overview, a Tokens sheet, and a Desktop
 * composition. The brushed-platinum overlay defaults to its maximum (0.8, the
 * top of the Gallery's brush slider).
 *
 *   node scripts/build-sketch.mjs [--brush=0.8] [--out=design/liquid-platinum.sketch]
 *
 * The brush tile and wallpaper are rendered with headless Chrome when it is
 * installed (the exact SVGs the web app uses); otherwise a procedural tile is
 * generated in Node and the desktop falls back to a gradient.
 */

import { execFileSync } from 'node:child_process'
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { flatten, parseColor, resolveAliases } from './tokens-lib.mjs'
import * as S from './sketch-lib.mjs'
import { brushedTextureSvg } from '../src/lib/brushSvg.ts'
import { wallpaperSvg } from '../src/lib/wallpaperSvg.ts'

const repo = join(dirname(fileURLToPath(import.meta.url)), '..')
const argv = Object.fromEntries(process.argv.slice(2).map((a) => a.replace(/^--/, '').split('=')))
const BRUSH = Math.min(1, Math.max(0, parseFloat(argv.brush ?? '0.8')))
const OUT = join(repo, argv.out ?? 'design/liquid-platinum.sketch')

// MARK: - Tokens

const tokens = resolveAliases(flatten(JSON.parse(readFileSync(join(repo, 'tokens/tokens.json'), 'utf8'))))
const T = new Map(tokens.map((t) => [t.path.join('.'), t]))
const val = (name) => {
  const t = T.get(name)
  if (!t) throw new Error(`No token ${name}`)
  return t.value
}
const num = (name) => parseFloat(val(name))

const swatches = new Map()
for (const t of tokens) {
  if (t.type !== 'color' || !parseColor(t.value)) continue
  const name = t.path.join('.')
  swatches.set(name, S.swatch(t.path.join('/'), S.color(t.value)))
}
const col = (name) => S.color(val(name), swatches.get(name)?.do_objectID)
const mix = (a, b, t) => ({ _class: 'color', alpha: a.alpha, red: a.red + (b.red - a.red) * t, green: a.green + (b.green - a.green) * t, blue: a.blue + (b.blue - a.blue) * t })

// MARK: - Assets

function chromePath() {
  const candidates = [
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
    '/Applications/Chromium.app/Contents/MacOS/Chromium',
    '/usr/bin/google-chrome',
    '/usr/bin/chromium',
  ]
  return candidates.find((c) => existsSync(c)) ?? null
}

function renderWithChrome(chrome, html, w, h, scale) {
  const dir = mkdtempSync(join(tmpdir(), 'lp-sketch-'))
  const file = join(dir, 'page.html')
  const out = join(dir, 'shot.png')
  writeFileSync(file, html)
  execFileSync(
    chrome,
    ['--headless=new', '--disable-gpu', '--hide-scrollbars', `--window-size=${w},${h}`, `--force-device-scale-factor=${scale}`, `--screenshot=${out}`, `file://${file}`],
    { stdio: 'ignore', timeout: 60_000 },
  )
  return readFileSync(out)
}

const wrap = (svg, w, h) =>
  `<!doctype html><html><body style="margin:0;width:${w}px;height:${h}px;overflow:hidden;background:#888">` +
  `<img src="data:image/svg+xml;utf8,${encodeURIComponent(svg)}" style="display:block;width:${w}px;height:${h}px;object-fit:cover"></body></html>`

/** Procedural fallback: anisotropic value noise, mid-gray, 512×512. */
function fallbackBrush() {
  const size = 512
  const px = Buffer.alloc(size * size * 4)
  const rows = Array.from({ length: size }, () => Math.random())
  for (let y = 0; y < size; y++) {
    const streak = (rows[y] + rows[(y + 1) % size] + rows[(y + size - 1) % size]) / 3
    let drift = Math.random()
    for (let x = 0; x < size; x++) {
      drift += (Math.random() - 0.5) * 0.08
      drift = Math.min(1, Math.max(0, drift))
      const n = 0.6 * streak + 0.4 * drift
      const g = Math.round(255 * (0.22 + 0.55 * n))
      const i = (y * size + x) * 4
      px[i] = px[i + 1] = px[i + 2] = g
      px[i + 3] = 255
    }
  }
  return S.encodePng(size, size, px)
}

const chrome = chromePath()
const brushParams = { freqX: num('brush.freq-x'), freqY: num('brush.freq-y'), octaves: num('brush.octaves'), seed: num('brush.seed'), tile: num('brush.tile'), rise: num('brush.angle.rise'), run: num('brush.angle.run'), contrast: num('brush.contrast') }
let brushPng
let wallpaperPng = null
if (chrome) {
  brushPng = renderWithChrome(chrome, wrap(brushedTextureSvg(brushParams), brushParams.tile, brushParams.tile), brushParams.tile, brushParams.tile, 2)
  wallpaperPng = renderWithChrome(chrome, wrap(wallpaperSvg(), 1440, 900), 1440, 900, 1)
} else {
  console.warn('sketch: Chrome not found — using a procedural brush tile and a gradient desktop')
  brushPng = fallbackBrush()
}
const BRUSH_REF = 'brush.png'
const WALLPAPER_REF = 'wallpaper.png'

// MARK: - Metal vocabulary

const brushFill = (opacity = BRUSH) => S.fillPattern(BRUSH_REF, { opacity, blendMode: S.BLEND.overlay, tileScale: 0.5, mode: 0 })
const metalFills = (top, bottom, brush = BRUSH) => [S.fillGradient([[0, col(top)], [1, col(bottom)]]), brushFill(brush)]
const emboss = () => [S.innerShadow(col('edge.light'), { y: 1 }), S.innerShadow(col('edge.dark'), { y: -1 })]
const wellShadows = () => [S.innerShadow(S.BLACK(0.28), { y: 1, blur: 3 }), S.innerShadow(S.BLACK(0.12), { spread: 1 })]
const hairline = () => S.border(col('edge.hairline'), 1, 1)
const embossText = () => [S.shadow(col('ink.emboss'), { y: 1 })]
const PILL = 999
const R = { xs: num('radius.xs'), sm: num('radius.sm'), md: num('radius.md'), lg: num('radius.lg'), window: num('radius.window') }
const CONTROL_H = num('size.control-height')

const f = (x, y, w, h) => ({ x, y, w, h })

function metal(name, frame, { top = 'surface.raised-top', bottom = 'surface.raised-bottom', radius = 0, brush = BRUSH, hairline: hl = false, shadows = [], borders = [], extra = {} } = {}) {
  return S.rectangle(name, frame, S.style({ fills: metalFills(top, bottom, brush), innerShadows: emboss(), borders: hl ? [hairline(), ...borders] : borders, shadows }), { radius, ...extra })
}

function well(name, frame, { radius = R.sm, fill = 'surface.well', fills, extra = {} } = {}) {
  return S.rectangle(name, frame, S.style({ fills: fills ?? [S.fillColor(col(fill))], innerShadows: wellShadows() }), { radius, ...extra })
}

function label(name, frame, str, { size = 13, font = S.FONT.medium, color = 'ink.primary', colorObj, align = 2, emboss: em = true, behaviour = 1, shadows, extra } = {}) {
  return S.text(name, frame, str, { size, font, color: colorObj ?? col(color), align, behaviour, shadows: shadows ?? (em ? embossText() : []), extra })
}

/** Bounding-box-fitted group, so Sketch never has to re-derive the frame. */
function fit(name, layers, extra = {}) {
  const minX = Math.min(...layers.map((l) => l.frame.x))
  const minY = Math.min(...layers.map((l) => l.frame.y))
  const maxX = Math.max(...layers.map((l) => l.frame.x + l.frame.width))
  const maxY = Math.max(...layers.map((l) => l.frame.y + l.frame.height))
  for (const l of layers) {
    l.frame.x -= minX
    l.frame.y -= minY
  }
  return S.group(name, f(minX, minY, maxX - minX, maxY - minY), layers, extra)
}

/** The room light on a bar: a screened band inside a mask of the bar's shape. */
function sheen(frame, radius, { alpha = num('sheen.alpha'), at = 0.35 } = {}) {
  const mask = S.rectangle('sheen mask', f(0, 0, frame.w, frame.h), S.style(), { radius, hasClippingMask: true })
  const bandW = frame.w * 0.28
  const band = S.rectangle(
    'sheen',
    f(frame.w * at - bandW / 2, -frame.h, bandW, frame.h * 3),
    S.style({
      fills: [S.fillGradient([[0, S.WHITE(0)], [0.5, col('sheen.color')], [1, S.WHITE(0)]], { from: S.pt(0, 0.5), to: S.pt(1, 0.5) }, { blendMode: S.BLEND.screen })],
      opacity: alpha,
    }),
    { rotation: -15 },
  )
  const g = S.group('sheen', f(frame.x, frame.y, frame.w, frame.h), [mask, band])
  return g
}

// MARK: - Symbols registry

const masters = []
const SYM = new Map()

function defineSymbol(name, w, h, layers, { background } = {}) {
  const symbolID = S.uuid()
  const master = S.symbolMaster(name, f(0, 0, w, h), layers, { symbolID, background })
  masters.push(master)
  SYM.set(name, { id: symbolID, w, h })
  return symbolID
}

function inst(name, x, y, { layerName, overrides = [], w, h, extra = {} } = {}) {
  const s = SYM.get(name)
  if (!s) throw new Error(`No symbol ${name}`)
  return Object.assign(S.symbolInstance(layerName ?? name, f(x, y, w ?? s.w, h ?? s.h), s.id, overrides), extra)
}

/** Sketch resizing constraints: a cleared bit turns a constraint on. */
const PIN = { all: 63, leftTopFixed: 63 - 4 - 32 - 2 - 16, topStretchX: 63 - 32 - 16 - 4 - 1, rightTopFixed: 63 - 1 - 32 - 2 - 16, fixedTop: 63 - 32 - 16 }

// MARK: - Bubbles

const TINTS = {
  close: { base: 'traffic.close.base', deep: 'traffic.close.deep', light: 'traffic.close.light' },
  minimize: { base: 'traffic.minimize.base', deep: 'traffic.minimize.deep', light: 'traffic.minimize.light' },
  zoom: { base: 'traffic.zoom.base', deep: 'traffic.zoom.deep', light: 'traffic.zoom.light' },
  accent: { base: 'accent.blue.base', deep: 'accent.blue.deep', light: 'accent.blue.light' },
  platinum: { base: 'platinum.4', deep: 'platinum.7', light: 'platinum.0' },
  inactive: { base: 'traffic.inactive', deep: 'traffic.inactive-deep', light: 'platinum.1' },
}
const cap = (s) => s[0].toUpperCase() + s.slice(1)

function bubbleLayers(s, tint, fill = num('liquid.fill')) {
  const t = TINTS[tint]
  const deep = col(t.deep)
  const base = col(t.base)
  const light = col(t.light)
  const radial = () => S.fillGradient([[0, light], [0.42, base], [1, deep]], { type: 1, from: S.pt(0.4, 0.3), to: S.pt(0.4, 0.95), elipseLength: 1 })
  const shell = S.oval(
    'shell',
    f(0, 0, s, s),
    S.style({
      fills: [S.fillGradient([[0, mix(deep, S.WHITE(), 0.3)], [0.8, deep]], { type: 1, from: S.pt(0.5, 0.35), to: S.pt(0.5, 1.15), elipseLength: 1 })],
      innerShadows: [S.innerShadow(col('traffic.rim'), { y: 1, blur: 2 }), S.innerShadow(S.BLACK(0.25), { spread: 0.5 })],
    }),
  )
  const mask = S.oval('mask', f(0, 0, s, s), S.style(), { hasClippingMask: true })
  const back = S.oval('liquid.back', f(-0.58 * s, (1 - fill) * s - 0.05 * s, 2 * s, 2 * s), S.style({ fills: [radial()], opacity: num('liquid.opacity-back') }))
  const front = S.oval('liquid.front', f(-0.5 * s, (1 - fill) * s, 2 * s, 2 * s), S.style({ fills: [radial()], opacity: num('liquid.opacity-front') }))
  const liquid = fit('liquid', [mask, back, front])
  const gloss = S.oval('gloss', f(0.18 * s, 0.08 * s, 0.46 * s, 0.32 * s), S.style({ fills: [S.fillGradient([[0, col('traffic.gloss')], [1, S.WHITE(0)]])] }))
  return [shell, liquid, gloss]
}

for (const size of [12, 16, 40]) {
  for (const tint of Object.keys(TINTS)) defineSymbol(`Bubble/${size}/${cap(tint)}`, size, size, bubbleLayers(size, tint))
}

// MARK: - Traffic lights, title bar, window chrome

const TRAFFIC = num('size.traffic')
const RIM = TRAFFIC + 2
const PITCH = TRAFFIC + num('size.traffic-gap')

function trafficLights(active) {
  const layers = []
  const tints = active ? ['close', 'minimize', 'zoom'] : ['inactive', 'inactive', 'inactive']
  tints.forEach((tint, i) => {
    layers.push(S.oval(`rim ${i + 1}`, f(i * PITCH, 0, RIM, RIM), S.style({ fills: metalFills('surface.raised-top', 'platinum.5'), innerShadows: emboss(), borders: [S.border(S.BLACK(0.2), 0.5, 1)] })))
    layers.push(inst(`Bubble/12/${cap(tint)}`, i * PITCH + 1, 1, { layerName: ['close', 'shade', 'zoom'][i] }))
  })
  return layers
}
defineSymbol('Traffic Lights/Active', 2 * PITCH + RIM, RIM, trafficLights(true))
defineSymbol('Traffic Lights/Inactive', 2 * PITCH + RIM, RIM, trafficLights(false))

const WIN_W = 720
const WIN_H = 460
const TITLE_H = num('size.titlebar-height')

function titleBar(active) {
  const top = active ? 'surface.titlebar-top' : 'surface.titlebar-inactive-top'
  const bottom = active ? 'surface.titlebar-bottom' : 'surface.titlebar-inactive-bottom'
  return [
    metal('bar', f(0, 0, WIN_W, TITLE_H), { top, bottom, radius: [R.window, R.window, 0, 0] }),
    sheen(f(0, 0, WIN_W, TITLE_H), [R.window, R.window, 0, 0], { alpha: active ? num('sheen.alpha') : num('sheen.alpha-inactive') }),
    S.rectangle('hairline', f(0, TITLE_H - 1, WIN_W, 1), S.style({ fills: [S.fillColor(col('edge.hairline'))] }), { extra: {}, resizingConstraint: PIN.topStretchX }),
    inst(active ? 'Traffic Lights/Active' : 'Traffic Lights/Inactive', 8, (TITLE_H - RIM) / 2, { layerName: 'traffic lights', extra: { resizingConstraint: PIN.leftTopFixed } }),
    label('title', f(84, (TITLE_H - 16) / 2, WIN_W - 168, 16), 'Window Title', { color: active ? 'ink.primary' : 'ink.tertiary', extra: { resizingConstraint: PIN.topStretchX } }),
  ]
}
defineSymbol('Title Bar/Active', WIN_W, TITLE_H, titleBar(true))
defineSymbol('Title Bar/Inactive', WIN_W, TITLE_H, titleBar(false))

// MARK: - Controls

function button(name, w, h, { variant = 'default', textStr = 'Button', size = 13 } = {}) {
  const layers = []
  if (variant === 'primary') {
    layers.push(
      S.rectangle('capsule', f(0, 0, w, h), S.style({ fills: [S.fillGradient([[0, col('accent.blue.light')], [1, col('accent.blue.base')]]), brushFill(BRUSH * 0.8)], innerShadows: emboss(), borders: [hairline()], shadows: [S.shadow(S.BLACK(0.18), { y: 1, blur: 2 })] }), { radius: PILL }),
    )
    layers.push(label('label', f(0, (h - 16) / 2, w, 16), textStr, { size, colorObj: col('ink.on-accent'), shadows: [S.shadow(S.BLACK(0.25), { y: 1 })] }))
  } else if (variant === 'quiet') {
    layers.push(label('label', f(0, (h - 16) / 2, w, 16), textStr, { size }))
  } else {
    layers.push(metal('capsule', f(0, 0, w, h), { radius: PILL, brush: BRUSH * 0.8, hairline: true, shadows: [S.shadow(S.BLACK(0.18), { y: 1, blur: 2 })] }))
    layers.push(label('label', f(0, (h - 16) / 2, w, 16), textStr, { size }))
  }
  defineSymbol(name, w, h, layers)
}
button('Button/Default', 96, CONTROL_H)
button('Button/Primary', 96, CONTROL_H, { variant: 'primary', textStr: 'Primary' })
button('Button/Quiet', 96, CONTROL_H, { variant: 'quiet', textStr: 'Quiet' })
button('Button/Small', 72, num('size.control-height-sm'), { textStr: 'Small', size: 12 })
defineSymbol('Button/Icon', CONTROL_H, CONTROL_H, [
  metal('square', f(0, 0, CONTROL_H, CONTROL_H), { radius: R.sm, brush: BRUSH * 0.8, hairline: true, shadows: [S.shadow(S.BLACK(0.18), { y: 1, blur: 2 })] }),
  label('glyph', f(0, 3, CONTROL_H, 16), '✦', { size: 12, color: 'ink.secondary' }),
])

// Segmented control
{
  const w = 240
  const seg = w / 3
  defineSymbol('Segmented Control', w, CONTROL_H, [
    S.rectangle('track', f(0, 0, w, CONTROL_H), S.style({ fills: [S.fillColor(col('platinum.3'))], innerShadows: wellShadows() }), { radius: PILL }),
    metal('thumb', f(2 + seg, 2, seg - 4, CONTROL_H - 4), { radius: PILL, shadows: [S.shadow(S.BLACK(0.25), { y: 1, blur: 2 })] }),
    ...['Day', 'Week', 'Month'].map((t, i) => label(t, f(i * seg, (CONTROL_H - 15) / 2, seg, 15), t, { size: 12, color: i === 1 ? 'ink.primary' : 'ink.secondary', emboss: i === 1 })),
  ])
}

// Toggle
for (const on of [true, false]) {
  defineSymbol(`Toggle/${on ? 'On' : 'Off'}`, 38, 22, [
    S.rectangle('track', f(0, 0, 38, 22), S.style({ fills: [S.fillGradient(on ? [[0, col('accent.blue.deep')], [1, col('accent.blue.base')]] : [[0, col('platinum.5')], [1, col('platinum.4')]])], innerShadows: wellShadows() }), { radius: PILL }),
    Object.assign(inst(`Bubble/16/${on ? 'Accent' : 'Platinum'}`, on ? 19 : 3, 3, { layerName: 'knob' }), { style: S.style({ shadows: [S.shadow(S.BLACK(0.35), { y: 1, blur: 2 })] }) }),
  ])
}

// Checkbox
for (const on of [true, false]) {
  const box = on
    ? S.rectangle('box', f(0, 0, 15, 15), S.style({ fills: [S.fillGradient([[0, col('accent.blue.light')], [1, col('accent.blue.base')]])], innerShadows: emboss(), borders: [hairline()] }), { radius: R.xs })
    : metal('box', f(0, 0, 15, 15), { radius: R.xs, hairline: true })
  const layers = [box]
  if (on) layers.push(label('check', f(0, 0, 15, 15), '✓', { size: 11, font: S.FONT.bold, colorObj: col('ink.on-accent'), emboss: false }))
  layers.push(label('label', f(23, -1, 100, 16), 'Label', { align: 0, font: S.FONT.regular, emboss: false }))
  defineSymbol(`Checkbox/${on ? 'On' : 'Off'}`, 123, 15, layers)
}

// Slider
{
  const w = 200
  const pct = 0.42
  defineSymbol('Slider', w, 16, [
    S.rectangle('rail', f(0, 5.5, w, 5), S.style({ fills: [S.fillColor(col('platinum.4'))], innerShadows: wellShadows() }), { radius: PILL }),
    S.rectangle('fill', f(0, 5.5, w * pct, 5), S.style({ fills: [S.fillColor(col('accent.blue.base'))] }), { radius: PILL }),
    S.oval(
      'thumb',
      f(w * pct - 8, 0, 16, 16),
      S.style({
        fills: [S.fillGradient([[0, col('platinum.0')], [0.7, col('platinum.3')], [1, col('platinum.5')]], { type: 1, from: S.pt(0.5, 0.3), to: S.pt(0.5, 1.1), elipseLength: 1 })],
        innerShadows: emboss(),
        borders: [hairline()],
        shadows: [S.shadow(S.BLACK(0.3), { y: 1, blur: 3 })],
      }),
    ),
  ])
}

// Text fields
defineSymbol('Text Field', 220, CONTROL_H, [well('well', f(0, 0, 220, CONTROL_H)), label('placeholder', f(8, (CONTROL_H - 16) / 2, 204, 16), 'Placeholder', { align: 0, font: S.FONT.regular, color: 'ink.tertiary', emboss: false })])
defineSymbol('Text Field/Round', 180, CONTROL_H, [
  well('well', f(0, 0, 180, CONTROL_H), { radius: PILL }),
  label('icon', f(10, (CONTROL_H - 16) / 2, 14, 16), '⌕', { align: 0, color: 'ink.tertiary', emboss: false }),
  label('placeholder', f(26, (CONTROL_H - 16) / 2, 146, 16), 'Search', { align: 0, font: S.FONT.regular, color: 'ink.tertiary', emboss: false }),
])

// Progress
defineSymbol('Progress Bar', 200, 8, [
  S.rectangle('rail', f(0, 0, 200, 8), S.style({ fills: [S.fillColor(col('platinum.4'))], innerShadows: wellShadows() }), { radius: PILL }),
  S.rectangle('fill', f(0, 0, 120, 8), S.style({ fills: [S.fillGradient([[0, col('accent.blue.light')], [0.55, col('accent.blue.base')], [1, col('accent.blue.deep')]])], innerShadows: [S.innerShadow(S.WHITE(0.4), { y: 1 })] }), { radius: PILL }),
])
{
  const mask = S.rectangle('rail', f(0, 0, 200, 8), S.style({ fills: [S.fillColor(col('platinum.4'))], innerShadows: wellShadows() }), { radius: PILL, hasClippingMask: true })
  const stripes = []
  for (let i = 0; i < 14; i++) {
    stripes.push(S.rectangle(`stripe ${i}`, f(i * 16 - 8, -8, 8, 24), S.style({ fills: [S.fillColor(col('platinum.1'))] }), { rotation: -35 }))
  }
  defineSymbol('Progress Bar/Indeterminate', 200, 8, [fit('pole', [mask, ...stripes])])
}

// MARK: - Menus

const MENU_W = 200
const ITEM_W = MENU_W - 8
function menuItem(name, { active = false, checked = false, disabled = false, text: t = 'Menu Item', shortcut = '⌘N' } = {}) {
  const layers = []
  if (active) layers.push(S.rectangle('highlight', f(0, 0, ITEM_W, CONTROL_H), S.style({ fills: [S.fillGradient([[0, col('accent.blue.light')], [1, col('accent.blue.base')]])] }), { radius: R.xs }))
  const ink = active ? col('ink.on-accent') : disabled ? col('ink.disabled') : col('ink.primary')
  const sub = active ? col('ink.on-accent') : col('ink.tertiary')
  layers.push(label('check', f(4, 3, 18, 16), checked ? '✓' : '', { size: 12, font: S.FONT.bold, colorObj: ink, emboss: false }))
  layers.push(label('label', f(26, 3, ITEM_W - 72, 16), t, { align: 0, font: S.FONT.regular, colorObj: ink, emboss: false }))
  layers.push(label('shortcut', f(ITEM_W - 56, 3, 48, 16), shortcut, { align: 1, font: S.FONT.regular, size: 12, colorObj: sub, emboss: false }))
  defineSymbol(name, ITEM_W, CONTROL_H, layers)
}
menuItem('Menu Item/Default', { text: 'New Window' })
menuItem('Menu Item/Active', { active: true, text: 'Close Window', shortcut: '⌘W' })
menuItem('Menu Item/Checked', { checked: true, text: 'Liquid Merge', shortcut: '' })
menuItem('Menu Item/Disabled', { disabled: true, text: 'Get Info', shortcut: '⌘I' })
defineSymbol('Menu Separator', ITEM_W, 9, [S.rectangle('line', f(8, 4, ITEM_W - 16, 1), S.style({ fills: [S.fillColor(col('edge.divider'))] }))])
{
  const items = ['Menu Item/Default', 'Menu Item/Active', 'Menu Item/Checked', 'Menu Separator', 'Menu Item/Disabled']
  let y = 4
  const layers = []
  for (const name of items) {
    layers.push(inst(name, 4, y))
    y += SYM.get(name).h
  }
  const h = y + 4
  layers.unshift(
    S.rectangle('panel', f(0, 0, MENU_W, h), S.style({ fills: [S.fillColor(col('surface.menu'))], innerShadows: emboss(), shadows: [S.shadow(rgbaTok(20, 22, 28, 0.3), { y: 10, blur: 30 }), S.shadow(S.BLACK(0.22), { spread: 1 })] }), { radius: R.md }),
  )
  defineSymbol('Menu', MENU_W, h, layers)
}
function rgbaTok(r, g, b, a) {
  return S.rgba(r, g, b, a)
}

// MARK: - Monogram

function monogram(name, size, { platinum = false } = {}) {
  const font = size * 0.75
  const glyphW = font * 0.72
  const opts = platinum
    ? {
        fills: [S.fillGradient([[0, col('platinum.0')], [0.45, col('platinum.4')], [0.5, col('platinum.6')], [1, col('platinum.3')]]), brushFill(Math.min(1, BRUSH + 0.1))],
        shadows: [S.shadow(S.BLACK(0.35), { y: size * 0.025, blur: size * 0.025 })],
      }
    : { fills: [], shadows: [] }
  const left = S.text('Я', f(size * 0.06, size * 0.02, glyphW, font * 1.2), 'R', { font: S.FONT.display, size: font, color: col('ink.primary'), behaviour: 0, ...opts, extra: { isFlippedHorizontal: true } })
  const right = S.text('R', f(size * 0.94 - glyphW, size * 0.02, glyphW, font * 1.2), 'R', { font: S.FONT.display, size: font, color: col('ink.primary'), behaviour: 0, ...opts })
  defineSymbol(name, size, size, [left, right])
}
monogram('Monogram/Flat', 24)
monogram('Monogram/Platinum', 160, { platinum: true })

// MARK: - Bars, sidebar, rows

const MENUBAR_H = num('size.menubar-height')
{
  const w = 1440
  const layers = [
    metal('bar', f(0, 0, w, MENUBAR_H), { top: 'surface.menubar-top', bottom: 'surface.menubar-bottom' }),
    sheen(f(0, 0, w, MENUBAR_H), 0, { at: num('sheen.light-x') }),
    S.rectangle('hairline', f(0, MENUBAR_H - 1, w, 1), S.style({ fills: [S.fillColor(col('edge.hairline'))] })),
    inst('Monogram/Flat', 14, 3, { layerName: 'monogram', w: 18, h: 18 }),
    ...(() => {
      let x = 46
      return ['File', 'Edit', 'View', 'Window', 'Help'].map((m) => {
        const w = Math.round(m.length * 7.2 + 4)
        const t = label(m, f(x, 4, w, 16), m, { align: 0, behaviour: 0 })
        x += w + 18
        return t
      })
    })(),
    label('clock', f(w - 120, 4, 108, 16), 'Mon 5:42 PM', { align: 1 }),
  ]
  defineSymbol('Menu Bar', w, MENUBAR_H, layers)
}

defineSymbol('Toolbar', WIN_W, 40, [
  metal('bar', f(0, 0, WIN_W, 40), { top: 'surface.window-top', bottom: 'surface.window-bottom' }),
  S.rectangle('hairline', f(0, 39, WIN_W, 1), S.style({ fills: [S.fillColor(col('edge.hairline'))] })),
  label('back', f(12, 12, 16, 16), '‹', { size: 16, color: 'ink.disabled', emboss: false }),
  label('forward', f(32, 12, 16, 16), '›', { size: 16, color: 'ink.disabled', emboss: false }),
  inst('Segmented Control', 60, 9, { w: 120, h: CONTROL_H, layerName: 'view' }),
  inst('Text Field/Round', WIN_W - 192, 9, { layerName: 'search' }),
])

for (const selected of [false, true]) {
  const layers = []
  if (selected) layers.push(S.rectangle('highlight', f(0, 0, 164, 24), S.style({ fills: [S.fillGradient([[0, col('accent.blue.light')], [1, col('accent.blue.base')]])], innerShadows: [S.innerShadow(S.WHITE(0.35), { y: 1 })] }), { radius: R.sm }))
  layers.push(label('icon', f(8, 4, 15, 16), '▢', { size: 12, colorObj: selected ? col('ink.on-accent') : col('accent.blue.base'), emboss: false, align: 0 }))
  layers.push(label('label', f(30, 4, 126, 16), 'Repositories', { align: 0, font: S.FONT.regular, colorObj: selected ? col('ink.on-accent') : col('ink.primary'), emboss: false }))
  defineSymbol(`Sidebar Item/${selected ? 'Selected' : 'Default'}`, 164, 24, layers)
}

const ROW_W = WIN_W - 180
function listRow(name, { selected = false, header = false, alt = false } = {}) {
  const layers = []
  if (header) layers.push(S.rectangle('bg', f(0, 0, ROW_W, 20), S.style({ fills: [S.fillGradient([[0, col('platinum.1')], [1, col('platinum.2')]])], shadows: [S.shadow(col('edge.divider'), { y: 1 })] })))
  else if (selected) layers.push(S.rectangle('bg', f(0, 0, ROW_W, 22), S.style({ fills: [S.fillGradient([[0, col('accent.blue.light')], [1, col('accent.blue.base')]])] })))
  else if (alt) layers.push(S.rectangle('bg', f(0, 0, ROW_W, 22), S.style({ fills: [S.fillColor(S.BLACK(0.035))] })))
  const ink = header ? col('ink.secondary') : selected ? col('ink.on-accent') : col('ink.primary')
  const sub = selected ? col('ink.on-accent') : col('ink.secondary')
  const h = header ? 20 : 22
  const font = header ? S.FONT.medium : S.FONT.regular
  const size = header ? 11 : 12
  const cols = header ? ['Name', 'Date Modified', 'Size', 'Kind'] : ['Liquid Platinum.sketch', 'Today, 4:12 PM', '18.4 MB', 'Document']
  layers.push(label('name', f(12, (h - 15) / 2, 220, 15), cols[0], { align: 0, font, size, colorObj: ink, emboss: false }))
  layers.push(label('modified', f(244, (h - 15) / 2, 120, 15), cols[1], { align: 0, font, size, colorObj: header ? ink : sub, emboss: false }))
  layers.push(label('size', f(376, (h - 15) / 2, 70, 15), cols[2], { align: 0, font, size, colorObj: header ? ink : sub, emboss: false }))
  layers.push(label('kind', f(458, (h - 15) / 2, 70, 15), cols[3], { align: 0, font, size, colorObj: header ? ink : sub, emboss: false }))
  defineSymbol(name, ROW_W, h, layers)
}
listRow('List Row/Header', { header: true })
listRow('List Row/Default')
listRow('List Row/Alternate', { alt: true })
listRow('List Row/Selected', { selected: true })

// MARK: - Surfaces

for (const [name, top, bottom, brush] of [
  ['Surface/Raised', 'surface.raised-top', 'surface.raised-bottom', BRUSH],
  ['Surface/Flat', 'surface.window-top', 'surface.window-bottom', BRUSH],
  ['Surface/Title Bar', 'surface.titlebar-top', 'surface.titlebar-bottom', BRUSH],
  ['Surface/Menu Bar', 'surface.menubar-top', 'surface.menubar-bottom', BRUSH],
  ['Surface/Body', 'surface.body', 'surface.body', BRUSH * 0.25],
]) {
  defineSymbol(name, 200, 96, [metal('surface', f(0, 0, 200, 96), { top, bottom, brush, radius: R.md, hairline: true }), sheen(f(0, 0, 200, 96), R.md)])
}
defineSymbol('Surface/Well', 200, 96, [well('well', f(0, 0, 200, 96), { radius: R.md, fills: [S.fillColor(col('surface.well')), brushFill(BRUSH * 0.35)] })])

// MARK: - Window

{
  const layers = [
    metal('chrome', f(0, 0, WIN_W, WIN_H), {
      top: 'surface.window-top',
      bottom: 'surface.window-bottom',
      radius: R.window,
      shadows: [S.shadow(rgbaTok(20, 22, 28, 0.4), { y: 24, blur: 56 })],
      borders: [S.border(S.BLACK(0.34), 1, 2)],
    }),
    S.rectangle('body', f(0, TITLE_H, WIN_W, WIN_H - TITLE_H), S.style({ fills: [S.fillColor(col('surface.body')), brushFill(BRUSH * 0.25)] }), { radius: [0, 0, R.window, R.window] }),
    inst('Title Bar/Active', 0, 0, { layerName: 'title bar', extra: { resizingConstraint: PIN.topStretchX } }),
    inst('Toolbar', 0, TITLE_H, { layerName: 'toolbar', extra: { resizingConstraint: PIN.topStretchX } }),
    S.rectangle('sidebar', f(0, TITLE_H + 40, 180, WIN_H - TITLE_H - 40 - 22), S.style({ fills: [S.fillColor(col('surface.sidebar'))], borders: [S.border(col('edge.divider'), 1, 1)] }), { radius: [0, 0, 0, R.window] }),
    label('favorites', f(16, TITLE_H + 48, 120, 14), 'FAVORITES', { size: 10, align: 0, color: 'ink.tertiary', emboss: false }),
    inst('Sidebar Item/Default', 8, TITLE_H + 64, { layerName: 'Desktop' }),
    inst('Sidebar Item/Selected', 8, TITLE_H + 88, { layerName: 'Repositories' }),
    inst('Sidebar Item/Default', 8, TITLE_H + 112, { layerName: 'Documents' }),
    inst('Sidebar Item/Default', 8, TITLE_H + 136, { layerName: 'Downloads' }),
    inst('List Row/Header', 180, TITLE_H + 40),
    inst('List Row/Default', 180, TITLE_H + 60),
    inst('List Row/Alternate', 180, TITLE_H + 82),
    inst('List Row/Selected', 180, TITLE_H + 104),
    inst('List Row/Alternate', 180, TITLE_H + 126),
    inst('List Row/Default', 180, TITLE_H + 148),
    S.rectangle('status', f(0, WIN_H - 22, WIN_W, 22), S.style({ fills: [S.fillGradient([[0, col('platinum.2')], [1, col('platinum.3')]])], innerShadows: [S.innerShadow(col('edge.light'), { y: 1 })] }), { radius: [0, 0, R.window, R.window] }),
    label('status text', f(0, WIN_H - 18, WIN_W, 14), '8 items, 412.8 GB available', { size: 11, font: S.FONT.regular, color: 'ink.secondary', emboss: false }),
  ]
  defineSymbol('Window', WIN_W, WIN_H, layers)
}

// MARK: - Pages

const INK = (name) => col(name)

/** Lays items out in rows; each item is { w, h, make(x, y) → layers[] }. */
function flow(items, { x0 = 40, y0 = 40, maxW = 1400, gapX = 40, gapY = 56, labelH = 22 } = {}) {
  const layers = []
  let x = x0
  let y = y0
  let rowH = 0
  for (const item of items) {
    if (x + item.w > x0 + maxW && x > x0) {
      x = x0
      y += rowH + gapY
      rowH = 0
    }
    layers.push(...item.make(x, y + labelH))
    rowH = Math.max(rowH, item.h + labelH)
    x += item.w + gapX
  }
  return { layers, height: y + rowH + gapY }
}

// Symbols page: masters with name labels, grouped by prefix.
const symbolsLayers = []
{
  const byGroup = new Map()
  for (const m of masters) {
    const key = m.name.split('/')[0]
    if (!byGroup.has(key)) byGroup.set(key, [])
    byGroup.get(key).push(m)
  }
  let y = 40
  for (const [groupName, list] of byGroup) {
    symbolsLayers.push(S.text(groupName, f(40, y, 400, 22), groupName, { font: S.FONT.bold, size: 18, color: S.WHITE(0.9), behaviour: 0 }))
    const { layers, height } = flow(
      list.map((m) => ({
        w: Math.max(m.frame.width, 80),
        h: m.frame.height,
        make: (x, yy) => {
          m.frame.x = x
          m.frame.y = yy
          return [S.text(`${m.name} label`, f(x, yy - 18, Math.max(m.frame.width, 160), 14), m.name, { font: S.FONT.mono, size: 10, color: S.WHITE(0.7), behaviour: 0 }), m]
        },
      })),
      { y0: y + 30, maxW: 1600 },
    )
    symbolsLayers.push(...layers)
    y = height + 20
  }
}
const symbolsPage = S.page('Symbols', symbolsLayers)

// Components overview.
const componentsLayers = []
{
  const order = ['Window', 'Title Bar', 'Menu Bar', 'Toolbar', 'Traffic Lights', 'Bubble', 'Button', 'Segmented Control', 'Toggle', 'Checkbox', 'Slider', 'Text Field', 'Progress Bar', 'Menu', 'Menu Item', 'Menu Separator', 'Sidebar Item', 'List Row', 'Surface', 'Monogram']
  let y = 48
  componentsLayers.push(S.text('heading', f(40, y, 800, 34), 'Liquid Platinum — Components', { font: S.FONT.display, size: 28, color: INK('ink.primary'), behaviour: 0, shadows: embossText() }))
  componentsLayers.push(S.text('sub', f(40, y + 40, 900, 18), `Brushed platinum overlay at ${BRUSH} · every color is a Color Variable from tokens/tokens.json · symbols live on the Symbols page`, { font: S.FONT.regular, size: 13, color: INK('ink.secondary'), behaviour: 0 }))
  y += 90
  for (const groupName of order) {
    const list = masters.filter((m) => m.name === groupName || m.name.startsWith(`${groupName}/`))
    if (!list.length) continue
    componentsLayers.push(S.text(groupName, f(40, y, 400, 20), groupName, { font: S.FONT.bold, size: 15, color: INK('ink.primary'), behaviour: 0, shadows: embossText() }))
    const { layers, height } = flow(
      list.map((m) => ({
        w: Math.max(m.frame.width, 72),
        h: m.frame.height,
        make: (x, yy) => [S.text(`${m.name} label`, f(x, yy - 16, Math.max(m.frame.width, 160), 13), m.name.split('/').slice(1).join(' / ') || m.name, { font: S.FONT.mono, size: 10, color: INK('ink.tertiary'), behaviour: 0 }), inst(m.name, x, yy)],
      })),
      { y0: y + 26, maxW: 1360, gapY: 44 },
    )
    componentsLayers.push(...layers)
    y = height + 8
  }
  var componentsH = y + 40
}
const componentsBoard = S.artboard('Components', f(0, 0, 1440, componentsH), componentsLayers, { background: col('surface.body') })

// Tokens sheet.
const tokensLayers = []
{
  let y = 48
  tokensLayers.push(S.text('heading', f(40, y, 800, 34), 'Liquid Platinum — Tokens', { font: S.FONT.display, size: 28, color: INK('ink.primary'), behaviour: 0, shadows: embossText() }))
  y += 64
  const groups = new Map()
  for (const [name, sw] of swatches) {
    const g = name.split('.')[0]
    if (!groups.has(g)) groups.set(g, [])
    groups.get(g).push([name, sw])
  }
  for (const [g, list] of groups) {
    tokensLayers.push(S.text(g, f(40, y, 400, 20), g, { font: S.FONT.bold, size: 15, color: INK('ink.primary'), behaviour: 0, shadows: embossText() }))
    y += 28
    let x = 40
    for (const [name, sw] of list) {
      if (x + 100 > 1400) {
        x = 40
        y += 74
      }
      tokensLayers.push(S.rectangle(name, f(x, y, 96, 44), S.style({ fills: [S.fillColor(col(name))], innerShadows: emboss(), borders: [S.border(S.BLACK(0.12), 1, 1)] }), { radius: R.sm }))
      tokensLayers.push(S.text(`${name} label`, f(x, y + 48, 96, 12), name.slice(name.indexOf('.') + 1), { font: S.FONT.mono, size: 9, color: INK('ink.secondary'), behaviour: 1 }))
      x += 108
      void sw
    }
    y += 90
  }
  tokensLayers.push(S.text('type', f(40, y, 400, 20), 'type', { font: S.FONT.bold, size: 15, color: INK('ink.primary'), behaviour: 0, shadows: embossText() }))
  y += 30
  for (const key of ['xs', 'sm', 'md', 'lg', 'xl', 'xxl']) {
    const size = num(`text.${key}`)
    tokensLayers.push(S.text(`text-${key}`, f(40, y, 900, size * 1.3), `text.${key} · ${size}px — Liquid Platinum moves like metal and reads like glass`, { font: S.FONT.regular, size, color: INK('ink.primary'), behaviour: 0 }))
    y += size * 1.3 + 10
  }
  y += 20
  tokensLayers.push(S.text('radius', f(40, y, 400, 20), 'radius · space', { font: S.FONT.bold, size: 15, color: INK('ink.primary'), behaviour: 0, shadows: embossText() }))
  y += 30
  let x = 40
  for (const key of ['xs', 'sm', 'md', 'lg', 'window']) {
    tokensLayers.push(metal(`radius-${key}`, f(x, y, 56, 56), { radius: R[key], hairline: true }))
    tokensLayers.push(S.text(`radius-${key} label`, f(x, y + 60, 80, 12), `${key} ${R[key]}`, { font: S.FONT.mono, size: 9, color: INK('ink.secondary'), behaviour: 0 }))
    x += 80
  }
  x += 40
  for (let i = 1; i <= 8; i++) {
    const s = num(`space.${i}`)
    tokensLayers.push(S.rectangle(`space-${i}`, f(x, y, s, 56), S.style({ fills: [S.fillColor(col('accent.blue.base'))] }), { radius: 2 }))
    tokensLayers.push(S.text(`space-${i} label`, f(x, y + 60, 40, 12), `${s}`, { font: S.FONT.mono, size: 9, color: INK('ink.secondary'), behaviour: 0 }))
    x += s + 16
  }
  var tokensH = y + 120
}
const tokensBoard = S.artboard('Tokens', f(0, 0, 1440, tokensH), tokensLayers, { background: col('surface.body') })

// Desktop composition.
const desktopLayers = []
{
  if (wallpaperPng) desktopLayers.push(S.bitmap('wallpaper', f(0, 0, 1440, 900), WALLPAPER_REF))
  else desktopLayers.push(S.rectangle('wallpaper', f(0, 0, 1440, 900), S.style({ fills: [S.fillGradient([[0, col('platinum.3')], [0.55, col('platinum.6')], [1, col('platinum.8')]], { from: S.pt(0, 0), to: S.pt(1, 1) })] })))
  desktopLayers.push(S.rectangle('vignette', f(0, 0, 1440, 900), S.style({ fills: [S.fillGradient([[0.55, S.BLACK(0)], [1, rgbaTok(20, 22, 28, 0.35)]], { type: 1, from: S.pt(0.5, 0.4), to: S.pt(0.5, 1.1), elipseLength: 1.6 })] })))
  desktopLayers.push(inst('Menu Bar', 0, 0))
  desktopLayers.push(inst('Window', 72, 72, { layerName: 'Rao' }))
  desktopLayers.push(inst('Window', 560, 200, { layerName: 'Liquid Platinum' }))
}
const desktopBoard = S.artboard('Desktop', f(0, 0, 1440, 900), desktopLayers, { background: col('platinum.6') })

const boardsPage = S.page('Liquid Platinum', [
  Object.assign(desktopBoard, { frame: S.rect(0, 0, 1440, 900) }),
  Object.assign(componentsBoard, { frame: S.rect(1520, 0, 1440, componentsH) }),
  Object.assign(tokensBoard, { frame: S.rect(3040, 0, 1440, tokensH) }),
])

// MARK: - Write

const pages = [boardsPage, symbolsPage]
const document = S.documentJson({ pages, swatches: [...swatches.values()] })
const entries = [
  { name: 'document.json', data: Buffer.from(JSON.stringify(document)) },
  { name: 'meta.json', data: Buffer.from(JSON.stringify(S.metaJson(pages))) },
  { name: 'user.json', data: Buffer.from(JSON.stringify(S.userJson(pages))) },
  ...pages.map((p) => ({ name: `pages/${p.do_objectID}.json`, data: Buffer.from(JSON.stringify(p)) })),
  { name: `images/${BRUSH_REF}`, data: brushPng },
]
if (wallpaperPng) entries.push({ name: `images/${WALLPAPER_REF}`, data: wallpaperPng })

mkdirSync(dirname(OUT), { recursive: true })
writeFileSync(OUT, S.zip(entries))
console.log(`sketch: ${masters.length} symbols, ${swatches.size} color variables, brush ${BRUSH} → ${OUT.replace(repo + '/', '')}`)
