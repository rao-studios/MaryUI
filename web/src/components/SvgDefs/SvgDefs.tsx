/**
 * SvgDefs — the hidden <svg> that carries every shared filter, gradient,
 * pattern and symbol: the liquid-merge filters, the object-icon recipe, the
 * brushed tile and the Rao monogram symbol. Mount once, in the Desktop root.
 *
 * The object-icon defs are here rather than in each icon because they are
 * finite and identical everywhere: six materials by four facets is
 * twenty-four body ramps, plus one key wash, one grain tile and one contact
 * shadow. They stay a fixed set because a part's `tone` is a blend pass and
 * not a gradient of its own — that is the decision that keeps this from
 * growing with the icon set.
 *
 * The merge filter does more than blur-and-threshold. That pair, on its own,
 * blurs the blobs' *colour and emboss* along with their alpha, and the merged
 * result reads as flat putty with an aliased edge. So the blobs now carry no
 * lighting at all: the filter thresholds them into a silhouette, then lights
 * that silhouette — a specular dome from the same room light as the sheen, and
 * a shaded lower rim — which is what gives a merged bead its volume.
 *
 * Each size comes in two tensions. `rest` blurs little and thresholds steeply,
 * so beads stay distinct; `flow` blurs more and thresholds shallowly, so they
 * bridge readily. A group swaps between them when it is hovered or when its
 * window is moving, which is a state change per interaction, not per frame.
 */

import { tokens } from '@/tokens/tokens'
import { sharedBrushDataUri } from '@/lib/textures'
import {
  BRUSH_PATTERN_ID,
  CONTACT_FILTER_ID,
  FACETS,
  GLOSS_GRADIENT_ID,
  GLOSS_STOPS,
  KEY_GRADIENT_ID,
  KEY_STOPS,
  RECIPE,
  STATIC_MATERIALS,
  bodyGradientId,
  facetStops,
} from '@/components/ObjectIcon'

export const GOO_SIZES = ['xxs', 'xs', 'sm', 'md'] as const
export const GOO_TENSIONS = ['rest', 'flow'] as const

export type GooSize = (typeof GOO_SIZES)[number]
export type GooTension = (typeof GOO_TENSIONS)[number]

/** e.g. gooFilterId('xs', 'flow') → 'lp-goo-xs-flow'. */
export function gooFilterId(size: GooSize, tension: GooTension): string {
  return `lp-goo-${size}-${tension}`
}

export const MONOGRAM_SYMBOL_ID = 'lp-monogram'

/**
 * Blur scales with the group's size; tension picks how readily beads merge.
 * What matters is blur against the *gap*: a bridge forms once the blurred alpha
 * at the midpoint clears the threshold, so `xs` (18px drops, 10px apart) needs
 * more blur than its size alone suggests.
 *
 * `xxs` is for shapes that are already touching and only need their join fused —
 * a segmented control's thumb and the segment beside it, with no gap between
 * them. Blur there is pure cost: it rounds off the pill's own semicircular caps
 * until they taper, which reads as the control losing its radius.
 */
const SIZE_SCALE: Record<GooSize, number> = { xxs: 0.55, xs: 1.2, sm: 1.9, md: 3.75 }

const g = tokens.goo

function Goo({ size, tension }: { size: GooSize; tension: GooTension }) {
  const blur = (tension === 'rest' ? g.blurRest : g.blurFlow) * SIZE_SCALE[size]
  const slope = tension === 'rest' ? g.slopeRest : g.slopeFlow
  const intercept = tension === 'rest' ? g.interceptRest : g.interceptFlow
  const rim = Math.max(1, blur * 0.5)

  return (
    <filter
      id={gooFilterId(size, tension)}
      x="-25%"
      y="-45%"
      width="150%"
      height="190%"
      colorInterpolationFilters="sRGB"
    >
      {/* Silhouette: blur the drops together, then cut a hard edge back out of the alpha. */}
      <feGaussianBlur in="SourceGraphic" stdDeviation={blur} result="blurred" />
      <feColorMatrix
        in="blurred"
        type="matrix"
        values={`1 0 0 0 0  0 1 0 0 0  0 0 1 0 0  0 0 0 ${slope} ${intercept}`}
        result="shape"
      />

      {/* Volume: a specular dome raised off the silhouette's own alpha. */}
      <feGaussianBlur in="shape" stdDeviation={rim} result="dome" />
      <feSpecularLighting
        in="dome"
        surfaceScale={g.specularScale}
        specularConstant={g.specularConstant}
        specularExponent={g.specularExponent}
        lightingColor={tokens.sheen.color}
        result="specular"
      >
        <feDistantLight azimuth={g.specularAzimuth} elevation={g.specularElevation} />
      </feSpecularLighting>
      <feComposite in="specular" in2="shape" operator="in" result="highlight" />
      <feComposite in="highlight" in2="shape" operator="arithmetic" k1="0" k2="1" k3="1" k4="0" result="lit" />

      {/* A shaded band inside the lower rim: the silhouette minus itself, shifted up. */}
      <feOffset in="shape" dy={-rim} result="lifted" />
      <feComposite in="shape" in2="lifted" operator="out" result="rimBand" />
      <feFlood floodColor="#000000" floodOpacity={g.rimShade} result="dark" />
      <feComposite in="dark" in2="rimBand" operator="in" result="shade" />

      <feMerge>
        <feMergeNode in="lit" />
        <feMergeNode in="shade" />
      </feMerge>
    </filter>
  )
}

/**
 * One ramp per material and facet. The facet is what turns geometry into
 * lighting without any per-icon gradient: it picks which stretch of the
 * material's ramp a face occupies, given where the key is.
 *
 * Only the materials with a literal ramp are shared here. `accent` and `folder`
 * resolve through CSS variables, and a var() inside a gradient stop is read
 * against the gradient element's own context — which would be this one, at the
 * app root — so those are emitted per instance by ObjectIcon instead.
 */
function ObjectDefs() {
  return (
    <>
      {STATIC_MATERIALS.map((material) =>
        FACETS.map((facet) => {
          const [from, to] = facetStops(material, facet)
          return (
            <linearGradient key={`${material}-${facet}`} id={bodyGradientId(material, facet)} x1="0" y1="0" x2="0.85" y2="1">
              <stop offset="0" stopColor={from} />
              <stop offset="1" stopColor={to} />
            </linearGradient>
          )
        }),
      )}

      {/* The broad key, along the same axis as the ramps: upper-left to lower-right. */}
      <linearGradient id={KEY_GRADIENT_ID} x1="0" y1="0" x2="0.9" y2="1">
        {KEY_STOPS.map((stop) => (
          <stop key={stop.offset} offset={stop.offset} stopColor={tokens.sheen.color} stopOpacity={stop.opacity} />
        ))}
      </linearGradient>

      {/* Aqua's sweep, for the glossy finish. Pure white, so it shares. */}
      <linearGradient id={GLOSS_GRADIENT_ID} x1="0" y1="0" x2="0" y2="1">
        {GLOSS_STOPS.map((stop, i) => (
          <stop key={i} offset={stop.offset} stopColor={tokens.sheen.color} stopOpacity={stop.opacity} />
        ))}
      </linearGradient>

      {/*
       * The surfaces' brushed tile, at the object tier's own density rather
       * than the window's. Surface paints one tile across 512px; matching that
       * would fit about a tenth of a tile behind a 48px icon and the grain
       * would flatten into a wash. So an object runs a much finer grain — near
       * enough to read as the same metal, not literally the same sheet, which
       * is the one place the icons knowingly depart from `brush`.
       */}
      <pattern id={BRUSH_PATTERN_ID} width={RECIPE.grainTile} height={RECIPE.grainTile} patternUnits="userSpaceOnUse">
        <image
          href={sharedBrushDataUri()}
          width={RECIPE.grainTile}
          height={RECIPE.grainTile}
          preserveAspectRatio="none"
        />
      </pattern>

      <filter id={CONTACT_FILTER_ID} x="-40%" y="-40%" width="180%" height="180%" colorInterpolationFilters="sRGB">
        <feDropShadow
          dx="0"
          dy={RECIPE.contactDy}
          stdDeviation={RECIPE.contactBlur}
          floodColor={RECIPE.contactColor}
        />
      </filter>
    </>
  )
}

export function SvgDefs() {
  return (
    <svg width="0" height="0" style={{ position: 'absolute', overflow: 'hidden' }} aria-hidden="true" focusable="false">
      <defs>
        {GOO_SIZES.map((size) => GOO_TENSIONS.map((tension) => <Goo key={`${size}-${tension}`} size={size} tension={tension} />))}

        <ObjectDefs />

        {/* The Rao mark: a serif R and its mirror sharing a stem. Traced paths can replace the text later. */}
        <symbol id={MONOGRAM_SYMBOL_ID} viewBox="0 0 200 200">
          <g
            fontFamily='"Iowan Old Style", "Palatino Linotype", Palatino, Georgia, serif'
            fontWeight="700"
            fontSize="150"
            textAnchor="middle"
          >
            <text x="66" y="156" transform="translate(132 0) scale(-1 1)">
              R
            </text>
            <text x="134" y="156">R</text>
          </g>
        </symbol>
      </defs>
    </svg>
  )
}
