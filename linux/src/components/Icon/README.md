# Icon

Inline 24×24 stroked glyphs in any ink. Mirrors `web/src/components/Icon`: the glyphs come from the same
`icons.json` (generated into `lp_icons.h`) and are stroked with round caps and joins at `LP_ICON_STROKE`.

## C
`lp_icon_widget(ctx, LP_ICON_SEARCH, x, y, 14, LP_INK_TERTIARY)`; lower level `lp_icon_draw(cr, …)`.
