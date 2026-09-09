# MenuBar

The brushed strip across the top: the Rao monogram menu, File / Edit / View / Window / Help, and status on the
right with a live clock. Mirrors `web/src/components/MenuBar`.

## Anatomy
`Surface bar` with the sheen pinned at `sheen.light-x` (the room light), height `LP_SIZE_MENUBAR_HEIGHT` (24),
padding `0 space.2`, shadows `emboss-raised` + `0 1px 0 edge.hairline` + `0 2px 8px rgba(0,0,0,.18)` (the bar's
chrome buffer is `LP_MENU_BAR_SHADOW_EXTENT` taller than the bar to hold them). Triggers are `height - 4px` tall
with 2px margins, padding `0 space.2` (`0 space.3` for the monogram), radius `xs`, `text.md`/medium, embossed.

## States
`open` — the accent gradient (`accent.light → accent.base`) and `ink.on-accent` with a dark text shadow.

## Behavior
Press opens/toggles a menu (`lp_menu_bar_result.pressed`); while one is open, hovering another switches to it
(`hovered`); Esc / a click elsewhere closes; ←/→ step (the desktop model does this, `lp_desktop`).

## Tokens
`LP_SURFACE_MENUBAR_*`, `LP_SIZE_MENUBAR_HEIGHT`, `LP_SPACE_2/3`, `LP_RADIUS_XS`, `LP_TEXT_MD`, `LP_TEXT_WEIGHT_MEDIUM`,
`LP_INK_PRIMARY`, `LP_INK_EMBOSS`, `LP_INK_ON_ACCENT`, `LP_ACCENT_*`, `LP_EDGE_HAIRLINE`, `LP_SHEEN_LIGHT_X`.

## C
`lp_menu_bar(ctx, rect, &model, &result)`; the model carries labels, `open_index`, the clock and status strings.
