# LiquidBubble

A well of colored liquid behind glass. Swirls on its own; sloshes with the window it lives in. **No JavaScript.**

## Anatomy
1. `shell` — circle, `--lp-bubble-size`, radial well from `--lp-b-deep`, inset rim `--lp-traffic-rim`.
2. `sloshFrame` — `rotate(--lp-slosh) translateY(--lp-slosh-y)`.
3. `liquid.back` — 200% rounded square, reverse swirl at 1.6× period, opacity `--lp-liquid-opacity-back`.
4. `liquid.front` — 200% rounded square, swirl at `--lp-motion-swirl-period`, opacity `--lp-liquid-opacity-front`.
5. `gloss` — top-left highlight, `--lp-traffic-gloss`.
6. `glyph` — optional, revealed by the parent.

## Props
`tint` close | minimize | zoom | accent | platinum | inactive · `size` px · `phase` s (staggers neighbors) · `fill` 0..1 (default `--lp-liquid-fill`) · `glyph`.

## Tokens
`--lp-traffic-<tint>-base/-deep/-light`, `--lp-accent-*`, `--lp-liquid-*`, `--lp-motion-swirl-period`, `--lp-traffic-rim`, `--lp-traffic-gloss`.

## Motion
Reads `--lp-slosh` (deg) and `--lp-slosh-y` (px) from the window frame; the engine integrates a damped pendulum from the window's acceleration (`src/lib/slosh.ts`). Ambient swirl pauses under reduced motion.

## Sketch notes
Symbol per tint; fill level is an override. Export the swirl as a static frame.
