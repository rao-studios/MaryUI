# GooGroup

Adjacent controls that merge like drops of mercury.

## Anatomy
1. `gooLayer` — `filter: url(#lp-goo-<size>-<tension>)`; holds `blob`s laid out with the same flex/grid rule as the content. **The blobs carry no lighting of their own** — no emboss, no border. Blurring an emboss along with the alpha is what used to leave a merged bead looking like flat putty; the filter thresholds the blobs into a silhouette and lights *that* instead (see `SvgDefs`).
2. `blob` — the slow, transitioned moves: swelling under the pointer, leaning toward a neighbour, staggered by `--lp-goo-lag × index`.
3. `blobMotion` — the painted element, carrying the per-frame smear. Separate from `blob` on purpose: with both on one transform, the 200ms hover transition smooths the live term away and the merge feels like syrup.
4. `content` — the real children, unfiltered so text stays crisp. Children set `data-goo-index`.

## Props
`size` xs | sm | md (blur strength) · `blobs { count, size, shape: circle | pill | fill, smear }` · `gap` · `blobClassName` · `renderBlob(i)` (put your own content in the filtered layer) · `flowing` (hold the tension open) · `respondsToMotion` (default true; false keeps the group's silhouette fixed while the window travels — no smear, no stretch, no motion-driven tension. Hover still works).

## Behavior
**Tension.** Hovering the group, or moving the window it is in (`[data-moving]` on the frame), swaps the filter from `rest` to `flow`. Both thresholds cut at about the same alpha, so the silhouette keeps its size when tension changes — only its reach grows. The swap happens in CSS off two custom properties, so a drag changes tension without React re-rendering.

**Attraction.** Hovering child *i* swells blob *i* (`scale 1.22`) and leans its neighbours toward it by `--lp-goo-attract`, so a neck forms rather than two circles overlapping.

**Smear.** Blobs trail against the travel by `(--lp-vx − --lp-vx-lag) × smear`, each a little further than the last, and stretch along it by `--lp-goo-stretch`. The lag term is the part of the velocity the slow spring has not caught up with, so it builds under acceleration and collapses as the window settles. The elongation is what actually closes the gaps; the filter bridges what is left.

`View › Liquid Merge` (`[data-goo='off']`) removes the filter; the group still reads as a shared row.

## Tokens
`--lp-surface-raised-top`, `--lp-platinum-5`, `--lp-goo-attract`, `--lp-goo-lag`, `--lp-goo-stretch`, `--lp-motion-normal`, `--lp-motion-ease-spring`.

## Where to use
Traffic lights, segmented controls, toolbar button groups, toggle tracks. Never around text-bearing layers.
