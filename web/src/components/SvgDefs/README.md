# SvgDefs

The hidden `<svg>` mounted once in the Desktop. Holds:

- `#lp-goo-<size>-<tension>` — the liquid-merge filters, for sizes `xxs` / `xs` / `sm` / `md` and tensions `rest` / `flow`.
- `#lp-monogram` — the Rao mark as a `<symbol>` (serif R + mirrored R). Replace the `<text>` glyphs with traced `<path>` data when the mark is final.

## The merge filter

Blur-and-threshold on its own is not enough. It blurs the blobs' colour and emboss along with their alpha, and the merged result reads as flat putty with an aliased edge — which is why the blobs feeding this filter carry no lighting of their own. The chain instead builds a silhouette and then lights it:

```
SourceGraphic → feGaussianBlur(blur) → feColorMatrix(alpha slope/intercept)   = SHAPE
SHAPE  → feGaussianBlur → feSpecularLighting(feDistantLight) → composite in   = SPEC
SPEC + SHAPE (arithmetic add)                                                 = LIT
SHAPE minus SHAPE-offset-up, flooded black at goo.rim-shade                   = SHADE
feMerge(LIT, SHADE)
```

The specular light's azimuth and colour come from the same room light as the sheen and the emboss, so a merged bead is lit like every other surface in the system.

## Tensions

`rest` blurs little and thresholds steeply, so beads stay distinct. `flow` blurs further and thresholds shallowly, so they bridge. The two are paired so both cut at roughly the same alpha (~0.43): the silhouette keeps its size when a group starts flowing, only its reach changes, so nothing pops.

What decides whether a bridge forms is blur against the *gap*, not blur against the blob — `SIZE_SCALE` in `SvgDefs.tsx` compensates, which is why `xs` (18px drops, 10px apart) blurs more than its size alone suggests.

The same arithmetic runs the other way. `xxs` exists for shapes that are **already touching** and only need their join fused — a segmented control's thumb and the segment beside it, with no gap at all. Blur there buys nothing and costs the shape: at `sm` (σ≈4.2, and 10.4 while flowing) a 24px-tall pill has its semicircular caps rounded away until they taper, which reads as the control losing its corner radius. Match the blur to the gap you are closing, and when the gap is zero, keep it small.

## Tokens
`--lp-goo-blur-rest/-flow`, `--lp-goo-slope-rest/-flow`, `--lp-goo-intercept-rest/-flow`, `--lp-goo-specular-*`, `--lp-goo-rim-shade`, `--lp-sheen-color`.

Filter primitive attributes are not CSS and cannot read custom properties, so these are read from `tokens.ts` at render time: changing them needs a reload, unlike the motion knobs.
