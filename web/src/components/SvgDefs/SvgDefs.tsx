/**
 * SvgDefs — the hidden <svg> that carries every shared filter and symbol:
 * the goo filters (three strengths) and the Rao monogram symbol. Mount once,
 * in the Desktop root.
 */

export const GOO_FILTER_IDS = { xs: 'lp-goo-xs', sm: 'lp-goo-sm', md: 'lp-goo-md' } as const
export const MONOGRAM_SYMBOL_ID = 'lp-monogram'

function Goo({ id, blur, slope, intercept }: { id: string; blur: number; slope: number; intercept: number }) {
  return (
    <filter id={id} x="-20%" y="-20%" width="140%" height="140%" colorInterpolationFilters="sRGB">
      <feGaussianBlur in="SourceGraphic" stdDeviation={blur} result="blur" />
      <feColorMatrix
        in="blur"
        type="matrix"
        values={`1 0 0 0 0  0 1 0 0 0  0 0 1 0 0  0 0 0 ${slope} ${intercept}`}
        result="goo"
      />
      <feComposite in="goo" in2="goo" operator="over" />
    </filter>
  )
}

export function SvgDefs() {
  return (
    <svg width="0" height="0" style={{ position: 'absolute', overflow: 'hidden' }} aria-hidden="true" focusable="false">
      <defs>
        <Goo id={GOO_FILTER_IDS.xs} blur={1.6} slope={20} intercept={-8} />
        <Goo id={GOO_FILTER_IDS.sm} blur={3} slope={18} intercept={-7} />
        <Goo id={GOO_FILTER_IDS.md} blur={6} slope={19} intercept={-9} />

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
