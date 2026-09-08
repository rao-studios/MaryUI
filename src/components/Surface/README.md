# Surface

The brushed platinum every other component sits on. Compose it; never paint metal by hand.

## Anatomy (Sketch layers, bottom to top)
1. `surface` — vertical gradient from `--lp-s-top` to `--lp-s-bottom`, emboss shadow.
2. `surface::before` — brush grain: the baked tile in `--lp-brush-url`, `mix-blend-mode: overlay`, opacity `--lp-s-brush`.
3. `sheen` (optional) — a 200%-wide highlight band, `mix-blend-mode: screen`, positioned by `--lp-sheen-x`, rotated by `--lp-tilt`.
4. children.

## Variants
`raised` (buttons, thumbs) · `flat` (window chrome) · `titlebar` · `bar` (menu bar) · `well` (inset fields) · `body` (content area).
Each variant only swaps the two gradient stops (`surface.*-top/-bottom` tokens) and, for wells, the brush opacity.

## Tokens
`--lp-surface-*`, `--lp-brush-url`, `--lp-brush-tile`, `--lp-brush-opacity`, `--lp-sheen-color`, `--lp-sheen-alpha`, `--lp-sheen-angle`, `--lp-shadow-emboss-raised`, `--lp-shadow-emboss-well`.

## Motion
Consumes `--lp-sheen-x` (0..1) and `--lp-tilt` (deg) written by the motion engine on the nearest window frame. Static everywhere else.

## Sketch notes
One symbol per variant; the sheen is a separate layer so it can be toggled. Export the brush tile once (see `src/lib/textures.ts`) as an image fill with Overlay blending.
