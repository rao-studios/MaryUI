# Toolbar

The brushed strip under a title bar that holds a window's controls. Mirrors `web/src/components/Toolbar`.

## Anatomy
`Surface flat`, height 40, padding `0 space.3`, gap `space.2`; `inset 0 1px 0 edge.light` + `0 1px 0 edge.hairline`.

## C
`lp_rect inner = lp_toolbar(ctx, &area);` lays out its own children with `lp_layout_row` / `lp_rect_cut_*`.
