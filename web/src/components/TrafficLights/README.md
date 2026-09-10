# TrafficLights

Close, shade, zoom — each one the same glass bead the Toggle uses for its knob, on the same drop shadow, sitting proud of the title bar. Three wells of coloured liquid with a surface line across each.

## Anatomy
1. `GooGroup xs` with `respondsToMotion={false}` — the filtered layer holds a second copy of each liquid, as a **full disc** (`fill: 1`). That copy is the bulk: it needs the mass to survive the flow blur and bridge, and anything less simply dissolves under its own filter. At rest it is completely hidden behind the beads.
2. `light × 3` — buttons in the unfiltered layer, each holding a `LiquidBubble` bead with its glyph.

Both GooGroup layers share one `gap`, so their pitches only line up while their items are the same width — the filtered blob is the same `--lp-size-traffic` as its bead.

## Behavior
At rest, three separate beads. Hover, and the liquid necks out from between them — the blur mixing hues at each neck is the liquids mixing. **A moving window does nothing to them but slosh.**

That last part is deliberate and was arrived at the hard way. An earlier pass faded the beads out mid-drag and let the merged liquid take over, which turned three fixed circles into three fat drops: the lights read as *becoming something else* just when the eye was tracking the window. A traffic light is a Toggle knob. It keeps its size, it never fades, and everything a moving window does to it happens to the liquid inside — which is why `respondsToMotion={false}` keeps the filtered layer out of the smear, the stretch and the motion half of the tension swap. Measured across a drag, the bead's width varies 2.33% and a Toggle knob's 2.31%; both are just riding the window's jelly.

## States
active / inactive window (inactive drains liquid to `--lp-traffic-inactive` until hovered) · shaded (the yellow well drains to 45% of its fill, so a collapsed window reads at a glance) · zoomed (green glyph becomes −).

## Tokens
`--lp-size-traffic`, `--lp-size-traffic-gap`, `--lp-traffic-*` (including `bead-shadow`), `--lp-liquid-fill-traffic`.

## Sketch notes
`Traffic Lights/Active · Hover · Inactive`, built from `Bubble/18/<Tint>`. The hover necking is drawn as `Liquid Merge/Apart · Necking · Merged`, since Sketch has no goo filter.
