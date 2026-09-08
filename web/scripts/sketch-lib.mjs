/**
 * A tiny, dependency-free writer for the Sketch file format: layer/style
 * builders that emit the JSON Sketch expects, a PNG encoder for the bitmap
 * assets, and a ZIP writer for the .sketch bundle itself.
 *
 * Only what the Liquid Platinum document needs is here; the shapes follow the
 * public sketch-file-format schema (document version 136).
 */

import { randomUUID } from 'node:crypto'
import { deflateRawSync, deflateSync } from 'node:zlib'
import { parseColor } from './tokens-lib.mjs'

export const uuid = () => randomUUID().toUpperCase()
export const pt = (x, y) => `{${x}, ${y}}`

// MARK: - Colors

export function color(input, swatchID) {
  const c = typeof input === 'string' ? parseColor(input) : input
  if (!c) throw new Error(`Bad color ${input}`)
  const out = { _class: 'color', alpha: c.alpha, blue: c.blue, green: c.green, red: c.red }
  if (swatchID) out.swatchID = swatchID
  return out
}

export const rgba = (r, g, b, a = 1) => ({ _class: 'color', alpha: a, red: r / 255, green: g / 255, blue: b / 255 })
export const WHITE = (a = 1) => rgba(255, 255, 255, a)
export const BLACK = (a = 1) => rgba(0, 0, 0, a)
export const CLEAR = rgba(0, 0, 0, 0)

// MARK: - Style pieces

export const ctx = (opacity = 1, blendMode = 0) => ({ _class: 'graphicsContextSettings', blendMode, opacity })
export const BLEND = { normal: 0, multiply: 2, screen: 5, overlay: 7, softLight: 8 }

export function gradient(stops, { from = pt(0.5, 0), to = pt(0.5, 1), type = 0, elipseLength = 0 } = {}) {
  return {
    _class: 'gradient',
    elipseLength,
    from,
    gradientType: type,
    to,
    stops: stops.map(([position, c]) => ({ _class: 'gradientStop', position, color: c })),
  }
}

const fillBase = () => ({
  _class: 'fill',
  isEnabled: true,
  contextSettings: ctx(),
  gradient: gradient([[0, WHITE()], [1, BLACK()]]),
  noiseIndex: 0,
  noiseIntensity: 0,
  patternFillType: 1,
  patternTileScale: 1,
  color: BLACK(),
})

export const fillColor = (c, opts = {}) => ({ ...fillBase(), fillType: 0, color: c, contextSettings: ctx(opts.opacity ?? 1, opts.blendMode ?? 0) })
export const fillGradient = (stops, gopts = {}, opts = {}) => ({ ...fillBase(), fillType: 1, gradient: gradient(stops, gopts), contextSettings: ctx(opts.opacity ?? 1, opts.blendMode ?? 0) })
export const fillPattern = (ref, { opacity = 1, blendMode = 0, tileScale = 1, mode = 0 } = {}) => ({
  ...fillBase(),
  fillType: 4,
  patternFillType: mode,
  patternTileScale: tileScale,
  image: imageRef(ref),
  contextSettings: ctx(opacity, blendMode),
})

export const border = (c, thickness = 1, position = 1) => ({
  _class: 'border',
  isEnabled: true,
  fillType: 0,
  color: c,
  contextSettings: ctx(),
  gradient: gradient([[0, WHITE()], [1, BLACK()]]),
  position,
  thickness,
})

const shadowBase = (cls, c, { x = 0, y = 0, blur = 0, spread = 0 } = {}) => ({
  _class: cls,
  isEnabled: true,
  blurRadius: blur,
  color: c,
  contextSettings: ctx(),
  offsetX: x,
  offsetY: y,
  spread,
})
export const shadow = (c, o) => shadowBase('shadow', c, o)
export const innerShadow = (c, o) => shadowBase('innerShadow', c, o)

export function style({ fills = [], borders = [], shadows = [], innerShadows = [], opacity = 1, blendMode = 0, textStyle, blur } = {}) {
  const s = {
    _class: 'style',
    do_objectID: uuid(),
    endMarkerType: 0,
    startMarkerType: 0,
    miterLimit: 10,
    windingRule: 1,
    borders,
    fills,
    shadows,
    innerShadows,
    contextSettings: ctx(opacity, blendMode),
    colorControls: { _class: 'colorControls', isEnabled: false, brightness: 0, contrast: 1, hue: 0, saturation: 1 },
    borderOptions: { _class: 'borderOptions', isEnabled: true, dashPattern: [], lineCapStyle: 0, lineJoinStyle: 0 },
  }
  if (textStyle) s.textStyle = textStyle
  if (blur) s.blur = blur
  return s
}

export const imageRef = (name) => ({ _class: 'MSJSONFileReference', _ref_class: 'MSImageData', _ref: `images/${name}` })

// MARK: - Layers

export const rect = (x, y, w, h) => ({ _class: 'rect', constrainProportions: false, height: h, width: w, x, y })
const exportOptions = () => ({ _class: 'exportOptions', includedLayerIds: [], layerOptions: 0, shouldTrim: false, exportFormats: [] })
const rulers = () => ({ _class: 'rulerData', base: 0, guides: [] })

function base(cls, name, frame, extra = {}) {
  return {
    _class: cls,
    do_objectID: uuid(),
    booleanOperation: -1,
    exportOptions: exportOptions(),
    frame: rect(frame.x, frame.y, frame.w, frame.h),
    isFixedToViewport: false,
    isFlippedHorizontal: false,
    isFlippedVertical: false,
    isLocked: false,
    isVisible: true,
    layerListExpandedType: 0,
    name,
    nameIsFixed: false,
    resizingConstraint: 63,
    resizingType: 0,
    rotation: 0,
    shouldBreakMaskChain: false,
    clippingMaskMode: 0,
    hasClippingMask: false,
    style: style(),
    ...extra,
  }
}

function curvePoint(point, cornerRadius = 0) {
  return { _class: 'curvePoint', cornerRadius, curveFrom: point, curveMode: 1, curveTo: point, hasCurveFrom: false, hasCurveTo: false, point }
}

/** `radius` is a number or [topLeft, topRight, bottomRight, bottomLeft]. */
export function rectangle(name, frame, layerStyle = style(), { radius = 0, ...extra } = {}) {
  const r = Array.isArray(radius) ? radius : [radius, radius, radius, radius]
  return base('rectangle', name, frame, {
    style: layerStyle,
    edited: false,
    isClosed: true,
    pointRadiusBehaviour: 1,
    fixedRadius: r[0],
    hasConvertedToNewRoundCorners: true,
    needsConvertionToNewRoundCorners: false,
    points: [curvePoint(pt(0, 0), r[0]), curvePoint(pt(1, 0), r[1]), curvePoint(pt(1, 1), r[2]), curvePoint(pt(0, 1), r[3])],
    ...extra,
  })
}

const K = 0.22385762510000001
const K2 = 0.77614237490000004

export function oval(name, frame, layerStyle = style(), extra = {}) {
  const p = (point, from, to) => ({ _class: 'curvePoint', cornerRadius: 0, curveFrom: from, curveMode: 2, curveTo: to, hasCurveFrom: true, hasCurveTo: true, point })
  return base('oval', name, frame, {
    style: layerStyle,
    edited: false,
    isClosed: true,
    pointRadiusBehaviour: 1,
    points: [
      p(pt(0.5, 1), pt(K2, 1), pt(K, 1)),
      p(pt(1, 0.5), pt(1, K), pt(1, K2)),
      p(pt(0.5, 0), pt(K, 0), pt(K2, 0)),
      p(pt(0, 0.5), pt(0, K2), pt(0, K)),
    ],
    ...extra,
  })
}

export const FONT = { regular: 'HelveticaNeue', medium: 'HelveticaNeue-Medium', bold: 'HelveticaNeue-Bold', mono: 'Menlo-Regular', display: 'IowanOldStyle-Bold' }

/**
 * A text layer. `align`: 0 left, 1 right, 2 center. `behaviour`: 0 auto width,
 * 1 fixed width. Pass `shadows` for embossed labels.
 */
export function text(name, frame, string, { font = FONT.regular, size = 13, color: c = BLACK(), align = 0, behaviour = 1, shadows = [], fills = [], opacity = 1, extra = {} } = {}) {
  const attributes = {
    MSAttributedStringFontAttribute: { _class: 'fontDescriptor', attributes: { name: font, size } },
    MSAttributedStringColorAttribute: c,
    paragraphStyle: { _class: 'paragraphStyle', alignment: align },
    kerning: 0,
  }
  return base('text', name, frame, {
    style: style({ shadows, fills, opacity, textStyle: { _class: 'textStyle', encodedAttributes: attributes, verticalAlignment: 0 } }),
    attributedString: {
      _class: 'attributedString',
      string,
      attributes: [{ _class: 'stringAttribute', location: 0, length: string.length, attributes }],
    },
    automaticallyDrawOnUnderlyingPath: false,
    dontSynchroniseWithSymbol: false,
    glyphBounds: `{{0, 0}, {${frame.w}, ${frame.h}}}`,
    lineSpacingBehaviour: 2,
    textBehaviour: behaviour,
    ...extra,
  })
}

export function group(name, frame, layers, extra = {}) {
  return base('group', name, frame, { hasClickThrough: false, groupLayout: { _class: 'MSImmutableFreeformGroupLayout' }, layers, ...extra })
}

export function bitmap(name, frame, ref) {
  return base('bitmap', name, frame, { clippingMask: '{{0, 0}, {1, 1}}', fillReplacesImage: false, image: imageRef(ref), intendedDPI: 72 })
}

function boardFields(background) {
  return {
    hasBackgroundColor: Boolean(background),
    backgroundColor: background ?? WHITE(),
    includeBackgroundColorInExport: true,
    includeInCloudUpload: true,
    isFlowHome: false,
    resizesContent: false,
    presetDictionary: {},
    hasClickThrough: false,
    groupLayout: { _class: 'MSImmutableFreeformGroupLayout' },
    horizontalRulerData: rulers(),
    verticalRulerData: rulers(),
  }
}

export function artboard(name, frame, layers, { background } = {}) {
  return base('artboard', name, frame, { ...boardFields(background), layers })
}

export function symbolMaster(name, frame, layers, { background, symbolID = uuid() } = {}) {
  return base('symbolMaster', name, frame, {
    ...boardFields(background),
    layers,
    symbolID,
    allowsOverrides: true,
    overrideProperties: [],
    changeIdentifier: 0,
    includeBackgroundColorInInstance: false,
  })
}

export function symbolInstance(name, frame, symbolID, overrideValues = []) {
  return base('symbolInstance', name, frame, { symbolID, overrideValues, scale: 1, verticalSpacing: 0, horizontalSpacing: 0 })
}

export function page(name, layers) {
  return base('page', name, { x: 0, y: 0, w: 0, h: 0 }, {
    hasClickThrough: true,
    groupLayout: { _class: 'MSImmutableFreeformGroupLayout' },
    horizontalRulerData: rulers(),
    verticalRulerData: rulers(),
    includeInCloudUpload: true,
    layers,
  })
}

// MARK: - Document

export function swatch(name, c) {
  return { _class: 'swatch', do_objectID: uuid(), name, value: c }
}

export function documentJson({ pages, swatches = [] }) {
  return {
    _class: 'document',
    do_objectID: uuid(),
    assets: {
      _class: 'assetCollection',
      do_objectID: uuid(),
      imageCollection: { _class: 'imageCollection', images: {} },
      colorAssets: swatches.map((s) => ({ _class: 'MSImmutableColorAsset', do_objectID: uuid(), name: s.name, color: { ...s.value, swatchID: undefined } })),
      gradientAssets: [],
      images: [],
      colors: [],
      gradients: [],
      exportPresets: [],
    },
    colorSpace: 1,
    currentPageIndex: 0,
    foreignLayerStyles: [],
    foreignSymbols: [],
    foreignTextStyles: [],
    foreignSwatches: [],
    layerStyles: { _class: 'sharedStyleContainer', do_objectID: uuid(), objects: [] },
    layerSymbols: { _class: 'symbolContainer', do_objectID: uuid(), objects: [] },
    layerTextStyles: { _class: 'sharedTextStyleContainer', do_objectID: uuid(), objects: [] },
    sharedSwatches: { _class: 'swatchContainer', do_objectID: uuid(), objects: swatches },
    pages: pages.map((p) => ({ _class: 'MSJSONFileReference', _ref_class: 'MSImmutablePage', _ref: `pages/${p.do_objectID}` })),
    documentState: { _class: 'documentState' },
    fontReferences: [],
    perDocumentLibraries: [],
    userInfo: {},
  }
}

export function metaJson(pages) {
  const pagesAndArtboards = {}
  for (const p of pages) {
    const artboards = {}
    for (const l of p.layers) if (l._class === 'artboard' || l._class === 'symbolMaster') artboards[l.do_objectID] = { name: l.name }
    pagesAndArtboards[p.do_objectID] = { name: p.name, artboards }
  }
  const created = { commit: '', appVersion: '84', build: 0, app: 'com.bohemiancoding.sketch3', compatibilityVersion: 99, version: 136, variant: 'NONAPPSTORE' }
  return {
    commit: '',
    pagesAndArtboards,
    version: 136,
    compatibilityVersion: 99,
    app: 'com.bohemiancoding.sketch3',
    autosaved: 0,
    variant: 'NONAPPSTORE',
    created,
    saveHistory: ['NONAPPSTORE.84'],
    appVersion: '84',
    build: 0,
  }
}

export function userJson(pages) {
  const out = { document: { pageListHeight: 110, pageListCollapsed: 0 } }
  for (const p of pages) out[p.do_objectID] = { scrollOrigin: pt(0, 0), zoomValue: 0.5 }
  return out
}

// MARK: - PNG

const CRC_TABLE = new Int32Array(256).map((_, n) => {
  let c = n
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1
  return c
})

export function crc32(buf, seed = 0) {
  let c = seed ^ -1
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8)
  return (c ^ -1) >>> 0
}

/** Encodes RGBA pixels (width*height*4 bytes) as a PNG. */
export function encodePng(width, height, rgba) {
  const raw = Buffer.alloc((width * 4 + 1) * height)
  for (let y = 0; y < height; y++) {
    raw[y * (width * 4 + 1)] = 0
    rgba.copy?.(raw, y * (width * 4 + 1) + 1, y * width * 4, (y + 1) * width * 4) ??
      raw.set(rgba.subarray(y * width * 4, (y + 1) * width * 4), y * (width * 4 + 1) + 1)
  }
  const chunk = (type, data) => {
    const len = Buffer.alloc(4)
    len.writeUInt32BE(data.length)
    const body = Buffer.concat([Buffer.from(type, 'ascii'), data])
    const crc = Buffer.alloc(4)
    crc.writeUInt32BE(crc32(body))
    return Buffer.concat([len, body, crc])
  }
  const ihdr = Buffer.alloc(13)
  ihdr.writeUInt32BE(width, 0)
  ihdr.writeUInt32BE(height, 4)
  ihdr[8] = 8
  ihdr[9] = 6
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', ihdr),
    chunk('IDAT', deflateSync(raw)),
    chunk('IEND', Buffer.alloc(0)),
  ])
}

// MARK: - ZIP

/** entries: [{ name, data: Buffer }] → a deflated zip archive. */
export function zip(entries) {
  const locals = []
  const centrals = []
  let offset = 0
  for (const { name, data } of entries) {
    const nameBuf = Buffer.from(name, 'utf8')
    const compressed = deflateRawSync(data)
    const crc = crc32(data)
    const local = Buffer.alloc(30)
    local.writeUInt32LE(0x04034b50, 0)
    local.writeUInt16LE(20, 4)
    local.writeUInt16LE(0, 6)
    local.writeUInt16LE(8, 8)
    local.writeUInt16LE(0, 10)
    local.writeUInt16LE(0, 12)
    local.writeUInt32LE(crc, 14)
    local.writeUInt32LE(compressed.length, 18)
    local.writeUInt32LE(data.length, 22)
    local.writeUInt16LE(nameBuf.length, 26)
    local.writeUInt16LE(0, 28)
    locals.push(local, nameBuf, compressed)

    const central = Buffer.alloc(46)
    central.writeUInt32LE(0x02014b50, 0)
    central.writeUInt16LE(20, 4)
    central.writeUInt16LE(20, 6)
    central.writeUInt16LE(0, 8)
    central.writeUInt16LE(8, 10)
    central.writeUInt16LE(0, 12)
    central.writeUInt16LE(0, 14)
    central.writeUInt32LE(crc, 16)
    central.writeUInt32LE(compressed.length, 20)
    central.writeUInt32LE(data.length, 24)
    central.writeUInt16LE(nameBuf.length, 28)
    central.writeUInt16LE(0, 30)
    central.writeUInt16LE(0, 32)
    central.writeUInt16LE(0, 34)
    central.writeUInt16LE(0, 36)
    central.writeUInt32LE(0, 38)
    central.writeUInt32LE(offset, 42)
    centrals.push(central, nameBuf)
    offset += local.length + nameBuf.length + compressed.length
  }
  const centralDir = Buffer.concat(centrals)
  const end = Buffer.alloc(22)
  end.writeUInt32LE(0x06054b50, 0)
  end.writeUInt16LE(0, 4)
  end.writeUInt16LE(0, 6)
  end.writeUInt16LE(entries.length, 8)
  end.writeUInt16LE(entries.length, 10)
  end.writeUInt32LE(centralDir.length, 12)
  end.writeUInt32LE(offset, 16)
  end.writeUInt16LE(0, 20)
  return Buffer.concat([...locals, centralDir, end])
}
