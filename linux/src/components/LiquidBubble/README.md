# LiquidBubble

A well of coloured liquid behind pale glass. The liquid rolls on its own and banks with the window
it lives in.

The waterline is the boundary between the pale glass above and the colour below, not a painted line.
An earlier pass painted the empty well in the tint's `deep` colour, which put a dark cap over every
bead and muddied the identity of all three traffic lights.

Anatomy, in paint order: shell (the glass well, clipped to a circle) → slosh frame → liquid back
(opacity `liquid.opacity-back`, brightened 15%, running against the front at 1.55× the period) +
liquid front (opacity `liquid.opacity-front`, with a `liquid.surface-light` band down its top
`liquid.surface-depth`) → gloss → rim (`traffic.rim`) → glyph.

Each liquid layer is a 200% blob with four unequal corner radii. It rolls sideways by
`liquid.wave-amplitude` of the diameter and rocks ±4° over `liquid.wave-period`, eased `ease-in-out`
and ping-ponged; `phase_s` keeps neighbouring bubbles out of lockstep.

`liquid_only` paints the liquid alone — no glass, no gloss, no rim, no glyph. That is the layer
GooGroup's filter merges; see `../TrafficLights/README.md`.

## Response (Linux only)

Every orb a person can point at answers the same way — the traffic lights, the Toggle's knob, the
Gallery's tint specimens:

- **Hover** fills the well to the brim over `motion.slow` (`motion.ease-out`). The level goes to
  1.12 rather than 1, because the rolling surface would otherwise bare a sliver of glass at the
  top. Moving off drains it back over `motion.slow` from wherever it had reached.
- **Holding it down** (active and under the pointer) darkens the liquid over `motion.fast`, and
  releasing or dragging off lightens it again over `motion.normal`. Each stop of the tint moves
  35 % toward the next one down (light → base → deep → black), so the colour deepens without
  greying.

The timing comes from `lp_ctx` — `hot_since_ms` / `last_hot*` and `held` / `held_since_ms` /
`last_held*`, recorded whenever the hover or the press moves — so an orb keeps no state of its own.
It asks for frames (`lp_want_frame_rect`) only while a look is still changing. The web has no
equivalent (**D17** in `../../../PARITY.md`).

## API

`lp_liquid_bubble(ctx, id, cx, cy, size, tint, phase_s, fill, glyph, glyph_alpha)` for the common
case: `id` is the widget the orb belongs to, which gives it the response, or 0 for a decorative orb.
Or `lp_liquid_bubble_spec(...)` to fill in what the context owns, set the rest yourself, and call
`lp_liquid_bubble_respond(ctx, id, rect, &spec)` before `lp_bubble_paint`.

Consumes `ctx->slosh_deg`, `ctx->slosh_x`, `ctx->slosh_y` and `ctx->now_ms`. Everything the slosh
moves is scaled by `size / 12`, so an 18px bead banks half again as far as the 12px bead the motion
tokens were tuned against; the rotation additionally carries `liquid.slosh-scale`.
