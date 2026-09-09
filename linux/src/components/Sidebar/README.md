# Sidebar

A source list: section headers and selectable rows with icons. Mirrors `web/src/components/Sidebar`.

## Anatomy
180px, padding `space.2`, `surface.sidebar`, `inset -1px 0 0 edge.divider`; headings `text.xs`/600 uppercase
`ink.tertiary` letter-spacing 0.04em; items 24px, padding `0 space.2`, `radius.sm`, `text.md`, icon 15px
`accent.base`; hover `rgba(0,0,0,.05)`; selected accent gradient, `ink.on-accent`, `inset 0 1px 0 rgba(255,255,255,.35)`.

## C
`lp_rect col = lp_sidebar(ctx, &area); lp_sidebar_section(ctx, &col, "Favorites"); if (lp_sidebar_item(ctx, id, &col, LP_ICON_FOLDER, "Repositories", sel)) …`
