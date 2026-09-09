# TrafficLights

Close / shade / zoom, each a LiquidBubble set in a platinum rim. Mirrors `web/src/components/TrafficLights`.

## Anatomy
A `GooGroup xs` of three 14px rims (`size.traffic + 2`) at gap `size.traffic-gap − 2` (pitch 20), smear 2.5;
bubbles of `size.traffic` (12) tinted close / minimize / zoom with phases 0 / 2.3 / 4.1 s; glyphs × – + (− when
zoomed) at full opacity while the group is hovered.

## States
shaded: the yellow bubble's fill drops to 0.38 · inactive and not hovered: every bubble drains to the `inactive`
tint · hovered: glyphs appear, the hovered rim swells.

## C
`lp_traffic_lights(ctx, x, y, active, shaded, zoomed, &result)` → `result.close/shade/zoom` clicked, `result.bounds`.
