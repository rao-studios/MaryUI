# Surface

The brushed platinum every other component sits on. Compose it; never paint metal by hand.
Mirrors `web/src/components/Surface` (README sections kept in the same order).

## Anatomy (bottom to top)
1. `surface` — vertical gradient from the variant's top to bottom stop, emboss shadow (`lp_fill_vgradient` + `lp_draw_inset_shadows`).
2. brush grain — the baked tile (`lp_brush_tile`) under `CAIRO_OPERATOR_OVERLAY`, opacity per variant (`lp_draw_brush`).
3. `sheen` (optional) — a 200%-wide highlight band, `CAIRO_OPERATOR_SCREEN`, positioned by `sheen_x`, rotated by `tilt` (`lp_draw_sheen`).
4. children.

## Variants
`raised` · `flat` · `titlebar` · `bar` · `well` · `body` — `enum lp_surface_variant`. Each only swaps the two gradient
stops (`LP_SURFACE_*`) and, for wells and bodies, the brush opacity (`lp_surface_stops`).

## Tokens
`LP_SURFACE_*`, `LP_BRUSH_OPACITY`, `LP_SHEEN_COLOR`, `LP_SHEEN_ALPHA`, `LP_SHEEN_ALPHA_INACTIVE`, `LP_SHEEN_ANGLE_DEG`,
`LP_SHADOW_EMBOSS_RAISED`, `LP_SHADOW_EMBOSS_WELL`.

## Motion
Consumes `ctx->sheen_x` (0..1) and `ctx->tilt` (deg) written by the motion engine on the enclosing window.

## C
`lp_surface(ctx, rect, (lp_surface_opts){ .variant, .radius, .sheen })` in the DRAW pass;
`lp_surface_paint(cr, …)` for code that has a cairo context but no ctx (the compositor's chrome).
