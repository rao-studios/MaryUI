# Surface

The brushed platinum every other component sits on. Compose it; never paint metal by hand.

## Anatomy (Sketch layers, bottom to top)
1. `surface` — vertical gradient from `--lp-s-top` to `--lp-s-bottom`, emboss shadow.
2. `grain` — the baked tile in `--lp-brush-url`, `mix-blend-mode: overlay`. A real element, not a pseudo-element, so it can be **transformed** rather than repainted: it is oversized by `--lp-brush-lag` on every side and translated by `--lp-grain-x/y`.
3. `sheen` (optional) — a 200%-wide highlight band, `mix-blend-mode: screen`, positioned by `--lp-sheen-x`, rotated by `--lp-tilt`.
4. children.

## Variants
`raised` (buttons, thumbs) · `flat` (window chrome) · `titlebar` · `bar` (menu bar) · `well` (inset fields) · `body` (content area).
Each variant only swaps the two gradient stops (`surface.*-top/-bottom` tokens) and, for wells, the brush opacity.

## Tokens
`--lp-surface-*`, `--lp-brush-url`, `--lp-brush-tile`, `--lp-brush-opacity`, `--lp-brush-lag`, `--lp-brush-glint`, `--lp-sheen-color`, `--lp-sheen-alpha`, `--lp-sheen-angle`, `--lp-shadow-emboss-raised`, `--lp-shadow-emboss-well`.

## Motion
Consumes `--lp-sheen-x` (0..1) and `--lp-tilt` (deg) written by the motion engine on the nearest window frame. Static everywhere else.

The grain moves too: the engine runs a first-order lag behind the drag (`motion.grain-follow`), so the metal skin trails the frame by up to `brush.lag` px while it moves and glides back when it stops — the hairline scratches visibly slide under the light as a window is flung. It is a lag rather than a spring on purpose: a spring sprang the grain back past the frame when a drag stopped, and the metal read as elastic instead of dragged. `brush.glint` lifts the grain's opacity with `--lp-speed` so the scratches catch the light while moving. Travel is clamped well inside the overhang and the tile is seamless, so no edge ever enters the surface.

## Sketch notes
One symbol per variant; the sheen is a separate layer so it can be toggled. Export the brush tile once (see `src/lib/textures.ts`) as an image fill with Overlay blending.
