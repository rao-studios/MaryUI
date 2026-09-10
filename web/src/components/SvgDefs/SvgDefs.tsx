/**
 * SvgDefs — the hidden <svg> that carries every shared filter and symbol:
 * the liquid-merge filters and the Rao monogram symbol. Mount once, in the
 * Desktop root.
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

export function SvgDefs() {
  return (
    <svg width="0" height="0" style={{ position: 'absolute', overflow: 'hidden' }} aria-hidden="true" focusable="false">
      <defs>
        {GOO_SIZES.map((size) => GOO_TENSIONS.map((tension) => <Goo key={`${size}-${tension}`} size={size} tension={tension} />))}

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
