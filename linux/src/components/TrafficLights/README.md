# TrafficLights

Close, shade and zoom. Each is a `LiquidBubble` at `size.traffic` (18px), spaced by
`size.traffic-gap` (10px) — a 74×18 group at a 28px pitch.

The bead **is** the Toggle's knob: the same glass well, the same
`0 1px 2px traffic.bead-shadow` sitting it proud of the title bar. There is no platinum rim any
more; an earlier pass set each bubble in a metal disc, which at 18px read as a button around a
light rather than the light itself.

## No merge layer

The web puts a second copy of the three liquids behind the beads, at full fill and with no glass,
and merges them through the goo filter so that hovering the group bridges them into one lit ribbon.
That is not ported (**D8** in `../../../PARITY.md`): the filter cost a blur and a `pow()` per pixel
on every window repaint, and the bridge read as putty at 18px. The beads still swell and lean
toward the one under the pointer; they never move otherwise, never scale and never fade — an
earlier pass faded them mid-drag, which turned three fixed circles into three fat drops.

## Levels and states

`liquid.fill-traffic` (0.72) is the resting level. Shading a window drains the middle bead to
`0.72 × 0.45`, so a collapsed window reads at a glance. Phases 0 / 2.3 / 4.1 s keep the three
surfaces from rolling in lockstep. An inactive window drains all three to
`traffic.inactive` until you hover the group, as Aqua did.

The glyphs (`×`, `–`, `+`, and `−` when zoomed) fade in on group hover at `0.6 × size` in
`rgba(0,0,0,0.5)` with an emboss shadow — smaller and lighter than the bubble's own default,
because at 18px the bubble's 0.72 filled the well.

## Hover and press (Linux only)

Each bead is an interactive orb, so it takes LiquidBubble's response
(`lp_liquid_bubble_respond`, see `../LiquidBubble/README.md`): pointing at a bead fills it to the
brim, and holding it down darkens the liquid a shade. Sweeping across the three lights drains each
one as the next fills. The web has no equivalent (**D17**).

## API

`lp_traffic_lights(ctx, x, y, active, shaded, zoomed, &result)`; `lp_traffic_lights_size()` for
layout. The result carries the three clicks, whether the group is hovered, and its bounds — the
title bar excludes them from its drag region.
