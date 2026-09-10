# TrafficLights

Close, shade and zoom. Each is a `LiquidBubble` at `size.traffic` (18px), spaced by
`size.traffic-gap` (10px) — a 74×18 group at a 28px pitch.

The bead **is** the Toggle's knob: the same glass well, the same
`0 1px 2px traffic.bead-shadow` sitting it proud of the title bar. There is no platinum rim any
more; an earlier pass set each bubble in a metal disc, which at 18px read as a button around a
light rather than the light itself.

## The two layers

Behind the beads sits a second copy of the same three liquids, rendered `liquid_only` at
`fill = 1` — bare liquid, no glass, no gloss, no glyph — and put through GooGroup's merge filter.
At rest the beads cover it exactly. On hover the filter loosens to its flow tension *and* the hot
bead's blob swells to 1.22× while its neighbours lean `goo.attract` toward it, which is what
closes a 10px gap far enough for the liquid to bridge into one lit ribbon. The beads themselves
never move, never scale and never fade — an earlier pass faded them mid-drag, which turned three
fixed circles into three fat drops.

Measured across a drag, a bead's width varies about as much as a Toggle knob's: both are only
riding the window's jelly.

## Levels and states

`liquid.fill-traffic` (0.72) is the resting level. Shading a window drains the middle bead to
`0.72 × 0.45`, so a collapsed window reads at a glance. Phases 0 / 2.3 / 4.1 s keep the three
surfaces from rolling in lockstep. An inactive window drains all three to
`traffic.inactive` until you hover the group, as Aqua did.

The glyphs (`×`, `–`, `+`, and `−` when zoomed) fade in on group hover at `0.6 × size` in
`rgba(0,0,0,0.5)` with an emboss shadow — smaller and lighter than the bubble's own default,
because at 18px the bubble's 0.72 filled the well.

## API

`lp_traffic_lights(ctx, x, y, active, shaded, zoomed, &result)`; `lp_traffic_lights_size()` for
layout. The result carries the three clicks, whether the group is hovered, and its bounds — the
title bar excludes them from its drag region.
