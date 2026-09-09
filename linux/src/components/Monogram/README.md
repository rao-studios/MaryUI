# Monogram

The Rao mark: a serif R and its mirror image sharing a stem. Mirrors `web/src/components/Monogram`.

## Variants
`flat` — one colour (the menu bar trigger). `platinum` — the metal ramp gradient under a drop shadow, the brushed
tile at 0.9 under OVERLAY, a diagonal specular under SCREEN, and a 1px `rgba(0,0,0,.35)` rim (About).

## Anatomy
Two "R" glyphs of the display font (`LP_FONT_DISPLAY_PANGO`, 700, 150 units in a 200-unit box, baseline 156), one
mirrored about x = 66 — exactly `SvgDefs.tsx`'s symbol. Trace to a path once the mark is final, as the web plans to.

## C
`lp_monogram(ctx, box, LP_MONOGRAM_FLAT, color)` / `lp_monogram_paint(cr, …)`.
