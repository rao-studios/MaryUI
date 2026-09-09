# ListRow

One line of a list view: icon, name, then trailing columns. Rows alternate tint. Mirrors `web/src/components/ListRow`.

## Anatomy
grid `minmax(160px, 2fr) repeat(minmax(90px, 1fr))` (`lp_list_columns`), gap `space.3`, height 22, padding
`0 space.3`, `text.sm`; even rows `rgba(0,0,0,.035)`; selected accent gradient with `ink.on-accent`; icon 14px
`accent.base`; columns `ink.secondary`. Header: 20px, `platinum.1 → platinum.2`, `0 1px 0 edge.divider`, `text.xs`/500.

## C
`lp_list_header(ctx, rect, cols, n)`; `lp_list_row(ctx, id, rect, icon, name, cols, n, selected, even)` → 1 click, 2 double-click.
