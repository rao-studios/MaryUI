# Surface

The brushed platinum every other component sits on. Compose it; never paint metal by hand.

## Anatomy (Sketch layers, bottom to top)
1. `surface` — vertical gradient from `--lp-s-top` to `--lp-s-bottom`, emboss shadow.
2. `grain` — the baked tile in `--lp-brush-url`, `mix-blend-mode: overlay`. Its `background-position` is the frame's page position (`--lp-grain-x/y`, from the engine) less this surface's offset inside that frame (`--lp-s-x/y`, measured on layout in `Surface.tsx`). The two together put the tile's origin at the page origin for every surface, so the sheet is anchored to the page and runs unbroken across surfaces.
3. `sheen` (optional) — a 200%-wide highlight band, `mix-blend-mode: screen`, positioned by `--lp-sheen-x`, rotated by `--lp-tilt`.
4. children.

## Variants
`raised` (buttons, thumbs) · `flat` (window chrome) · `titlebar` · `bar` (menu bar) · `well` (inset fields) · `body` (content area).
Each variant only swaps the two gradient stops (`surface.*-top/-bottom` tokens) and, for wells, the brush opacity.

## Tokens
`--lp-surface-*`, `--lp-brush-url`, `--lp-brush-tile`, `--lp-brush-opacity`, `--lp-brush-glint`, `--lp-sheen-color`, `--lp-sheen-alpha`, `--lp-sheen-angle`, `--lp-shadow-emboss-raised`, `--lp-shadow-emboss-well`.

## Motion
Consumes `--lp-sheen-x` (0..1) and `--lp-tilt` (deg) written by the motion engine on the nearest window frame. Static everywhere else.

The grain moves too, and it does not belong to the surface at all: the metal is one sheet the whole page is cut out of, and a window uncovers a different part of it as it moves. The offset is simply the frame's page position, so the scratches travel one-for-one with the drag, without limit, and they line up across a window's own surfaces and across neighbouring windows alike. No spring, no easing, no lag — earlier passes tried all three and each of them made the metal look attached to the window instead of behind it. `brush.glint` lifts the grain's opacity with `--lp-speed` so the scratches catch the light while moving. Travel is clamped well inside the overhang and the tile is seamless, so no edge ever enters the surface.

## Sketch notes
One symbol per variant; the sheen is a separate layer so it can be toggled. Export the brush tile once (see `src/lib/textures.ts`) as an image fill with Overlay blending.
