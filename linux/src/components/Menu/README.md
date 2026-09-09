# Menu

A dropdown panel of MenuItems. Mirrors `web/src/components/Menu`; the rows are `MenuItem`.

## Anatomy
padding `space.1`, `radius.md`, `surface.menu` (94% platinum), `shadow.menu` (9-slice sprite) + `emboss-raised`,
min-width 200. ≈ `backdrop-filter: blur(14px)` is not reproduced (no blend/mask nodes in wlr_scene); the 94%
surface alone stands in. The `lp-menu-in` entry animation (120ms, opacity + translateY(−4px) scaleY(.96))
arrives with the motion milestone.

## Behavior
Hover highlights (`hovered`), release selects (`selected`); the desktop model owns ↑/↓/Enter/Esc/←/→ and which
menu is open.

## C
`lp_menu(ctx, rect, &model, active_index, &result)`; `lp_menu_measure(cr, &model)` sizes the panel.
