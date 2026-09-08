# GooGroup

Adjacent controls that merge like drops of mercury.

## Anatomy
1. `gooLayer` — `filter: url(#lp-goo-<size>)`; holds blank `blob`s laid out with the same flex/grid rule as the content.
2. `content` — the real children, unfiltered so text stays crisp. Children set `data-goo-index`.

## Props
`size` xs | sm | md (blur strength) · `blobs { count, size, shape: circle | pill | fill, smear }` · `gap`.

## Behavior
Hovering child *i* swells blob *i* (`scale 1.22`) and nudges neighbors (`1.08`). `smear` shifts blob *i* by `--lp-vx × smear × i`, so a fast drag pulls the group along like liquid. `View › Liquid Merge` (`[data-goo='off']`) removes the filter; the group still reads as a shared row.

## Tokens
`--lp-surface-raised-top`, `--lp-platinum-5`, `--lp-shadow-emboss-raised`, `--lp-motion-normal`, `--lp-motion-ease-spring`.

## Where to use
Traffic lights, segmented controls, toolbar button groups, toggle tracks. Never around text-bearing layers.
