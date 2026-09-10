# LiquidBubble

A bead of glass holding coloured liquid, with a visible surface line across it. The Toggle's knob, the Slider's thumb and a window's traffic lights are all this same object at different sizes, tints and fill levels. Rolls on its own; sloshes with the window it lives in. **No JavaScript.**

## Anatomy
1. `shell` — circle, `--lp-bubble-size`. The empty part of the well is *pale glass*, not dark liquid: a well painted in `--lp-b-deep` puts a dark cap over every bead and muddies the identity of three lights sitting side by side.
2. `sloshFrame` — `rotate(--lp-slosh × --lp-liquid-slosh-scale) translate(--lp-slosh-x, --lp-slosh-y)`, both offsets scaled by `--lp-bubble-scale`.
3. `liquid.back` — 200% rounded square, counter-rolling at 1.55× period, opacity `--lp-liquid-opacity-back`.
4. `liquid.front` — 200% rounded square, rolling at `--lp-liquid-wave-period`, opacity `--lp-liquid-opacity-front`. Its top edge is lit by `--lp-liquid-surface-light`, and *that* is the crest you read when the liquid tilts — unlike the meniscus hairline it survives the merge filter, whose threshold touches alpha, not colour.
5. `meniscus` — a painted hairline across the waterline, lit above and shadowed below. **Off by default**; the Gallery's Debug tab turns it on with `data-crest` on `<html>`. It reads as a stripe drawn on the liquid rather than as its surface, so it earns its keep only while tuning. Never drawn under `liquidOnly`, where it would alias against the threshold.
6. `gloss` — top-left highlight, `--lp-traffic-gloss`.
7. `glyph` — optional, revealed by the parent.

## Props
`tint` close | minimize | zoom | accent | platinum | inactive · `size` px · `phase` s (staggers neighbours) · `fill` 0..1 (default `--lp-liquid-fill`) · `glyph` · `liquidOnly` (drop the glass and the crest hairline, for a caller putting the liquid inside a merge filter).

## Tokens
`--lp-traffic-<tint>-base/-deep/-light`, `--lp-accent-*`, `--lp-liquid-*`, `--lp-traffic-rim`, `--lp-traffic-gloss`.

Each tint reads a bead-scoped `--lp-bead-<tint>-<role>` first and falls back to the shared token. The Gallery's Customize tab writes only those, so recolouring a knob cannot restyle buttons, toggle tracks and selection highlights — which is what editing `accent` or `platinum` directly would do, since they alias tokens the rest of the UI is built on.

## Motion
Reads `--lp-slosh` (deg), `--lp-slosh-x` and `--lp-slosh-y` (px) from the window frame; the engine integrates a damped pendulum from the window's motion (`src/lib/slosh.ts`). The ambient roll pauses under reduced motion.

Two things drive that pendulum, and the second is why it reads at all. Acceleration tips the liquid — but a dragged window spends almost all of its time at roughly constant velocity, where acceleration is zero, so acceleration alone leaves the liquid flat through the middle of every gesture and twitching only at its ends. So the liquid is also treated as viscous and dragged: `motion.slosh-shear-gain` acts on how far its own speed lags the well's, which sustains the lean for as long as the gesture lasts. `motion.slosh-velocity-ref` is what that shear is measured against, and it is deliberately far below `motion.velocity-ref` — that one is scaled for the jelly, where full deflection should take a hard fling.

The sideways pile-up (`--lp-slosh-x`, from `motion.slosh-shift`) is the strongest cue at 18px; rotation alone is only a couple of pixels of crest movement. `liquid.slosh-scale` adds a little display gain on top of correct physics.

`--lp-bubble-scale` is the diameter as a unitless multiple of 12, because CSS cannot divide a length by a length.

## Sketch notes
Symbol per tint per size; fill level is an override. Export the roll as a static frame.
