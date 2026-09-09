# LiquidBubble

A well of coloured liquid behind glass. The liquid swirls on its own and tilts with the window it lives in.
Mirrors `web/src/components/LiquidBubble` (which is pure CSS; here it is `lp_bubble_paint`).

## Anatomy
shell (radial well: deep lightened 30% → deep at 80%) → slosh frame (`rotate(slosh) translateY(slosh_y)`) →
liquid back (opacity `liquid.opacity-back`, brightened 15%, reverse swirl at 1.6× the period) + liquid front
(opacity `liquid.opacity-front`) — each a 200% blob with corner radii 42/45/40/44% (back 45/40/44/41%) sitting
at `(1 − fill) × 100%` (back 5% higher), filled `light → base 42% → deep` radially at 40% 30% — → gloss
(ellipse 18% 8% 46% × 32%, `traffic.gloss` fading down) → rim (`inset 0 1px 2px traffic.rim`,
`inset 0 0 0 .5px rgba(0,0,0,.25)`) → glyph (`size × 0.8`, 700, `rgba(0,0,0,.55)`, alpha 0 → 1 on hover).

## Tints
`close` · `minimize` · `zoom` · `inactive` · `accent` · `platinum` — `enum lp_bubble_tint`, three colours each
(`lp_bubble_tint_colors`).

## Motion
Consumes `ctx->slosh_deg`, `ctx->slosh_y` and `ctx->now_ms` (swirl period `motion.swirl-period`, phase per bubble).

## C
`lp_liquid_bubble(ctx, cx, cy, size, tint, phase_s, fill /* < 0 = token */, glyph, glyph_alpha)`.
