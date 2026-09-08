# TrafficLights

Close, shade, zoom — three LiquidBubbles in platinum rims that merge like mercury.

## Anatomy
1. `GooGroup xs` — three rim blobs (`--lp-size-traffic` + 2px) in the goo layer; they swell on hover and smear with `--lp-vx`.
2. `light × 3` — buttons holding `LiquidBubble` tinted close / minimize / zoom.
3. Glyphs (× – +) inside each bubble, shown on group hover.

## States
active / inactive window (inactive drains liquid to `--lp-traffic-inactive` until hovered) · shaded (yellow bubble fill drops) · zoomed (green glyph becomes −).

## Tokens
`--lp-size-traffic`, `--lp-size-traffic-gap`, `--lp-traffic-*`.

## Sketch notes
One symbol with an active/inactive override and a hover variant.
