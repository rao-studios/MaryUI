# Surface

The brushed platinum every other component sits on. Compose it; never paint metal by hand.
Mirrors `web/src/components/Surface` (README sections kept in the same order).

## Anatomy (bottom to top)
1. `surface` — vertical gradient from the variant's top to bottom stop, emboss shadow (`lp_fill_vgradient` + `lp_draw_inset_shadows`).
2. brush grain — the baked tile (`lp_brush_tile`) under `CAIRO_OPERATOR_OVERLAY`, opacity per variant, slid by the
   window's grain lag and brightened by its speed (`lp_draw_brush`).
3. `sheen` (optional) — a 200%-wide highlight band, `CAIRO_OPERATOR_SCREEN`, positioned by `sheen_x`, rotated by `tilt` (`lp_draw_sheen`).
4. children.

## Variants
`raised` · `flat` · `titlebar` · `bar` · `well` · `body` — `enum lp_surface_variant`. Each only swaps the two gradient
stops (`LP_SURFACE_*`) and, for wells and bodies, the brush opacity (`lp_surface_stops`).

## Tokens
`LP_SURFACE_*`, `LP_BRUSH_OPACITY`, `LP_SHEEN_COLOR`, `LP_SHEEN_ALPHA`, `LP_SHEEN_ALPHA_INACTIVE`, `LP_SHEEN_ANGLE_DEG`,
`LP_SHADOW_EMBOSS_RAISED`, `LP_SHADOW_EMBOSS_WELL`.

## Motion
Everything the enclosing window does to a surface travels in `lp_surface_motion`: `sheen_x` (0..1)
and `tilt` (deg) place the highlight band, `grain_x`/`grain_y` slide the brushed skin behind the
frame (up to `brush.lag`, so the metal reads as a real thing being dragged rather than a pattern
painted onto a moving box), and `speed` adds up to `brush.glint` more grain at full tilt. The
motion engine writes all five onto the context.

It is a separate argument rather than a field of `lp_surface_opts` on purpose: as an option it was
easy to leave out of a designated initialiser, and every window surface did — the grain sat still
while the window moved.

## C
`lp_surface(ctx, rect, (lp_surface_opts){ .variant, .radius, .sheen })` in the DRAW pass takes the
motion from the context. `lp_surface_paint(cr, rect, opts, motion)` for code that has a cairo
context but no ctx (the compositor's chrome): pass `lp_surface_motion_of(ctx)` inside a window, or
a zeroed struct for chrome that does not move with one, like the menu bar.
