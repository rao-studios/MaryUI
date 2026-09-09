# MenuItem

One row of a dropdown: check column, label, shortcut; `separator` renders the hairline instead.
Mirrors `web/src/components/MenuItem`; drawn by `lp_menu` (Menu.c) row by row.

## Anatomy
grid `18px 1fr auto`, gap `space.2`, height `size.control-height` (22), padding `0 space.3 0 space.1`,
`radius.xs`, `text.md`; check "✓" `text.sm`/700; shortcut `text.sm`, `ink.tertiary`, letter-spacing 0.04em;
separator 1px `edge.divider` with `space.1 space.2` margins.

## States
active: `accent.light → accent.base`, `ink.on-accent`, dark text shadow, shortcut at 85% · disabled: `ink.disabled`.
