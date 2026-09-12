# MenuItem

One row of a menu: check column, label, shortcut; `separator` renders the hairline instead.
Mirrors `web/src/components/MenuItem`, which the Spotlight panel now renders inline — there is no
menu bar, and no floating `Menu` component on either side.

## Anatomy
grid `18px 1fr auto`, gap `space.2`, height `size.control-height` (22), padding `0 space.3 0 space.1`,
`radius.xs`, `text.md`; check "✓" `text.sm`/700; shortcut `text.sm`, `ink.tertiary`, letter-spacing 0.04em;
separator 1px `edge.divider` with `space.1` margins.

## States
active: `accent.light → accent.base`, `ink.on-accent`, dark text shadow, shortcut at 85% · disabled: `ink.disabled`.

## Behavior
Hover highlights (`hovered`), release selects (`selected`); disabled rows do neither. Which menu is open, and
which entry is active, belongs to the desktop model (`lp_desktop.open_menu` / `menu_active`).

## C
`components/lp_menu_item.h`. `lp_menu_list(ctx, content_rect, &model, active, &out)` paints and hit-tests the rows
and nothing else — the container around them belongs to the caller, which is Spotlight's `.cmdDropdown` panel for
the app commands. `lp_menu_list_measure(cr, &model)` sizes them (`cr` may be NULL: the text layer keeps a scratch
context, so an EVENT pass measures exactly what DRAW will). `lp_menu_item_height(&entry)` is one row.

`lp_menu_popup(ctx, x, y, &model, active, &out)` is the one floating panel left on this side — `shadow.menu`,
`surface.menu` at `radius.md` with `emboss-raised`, rows inside — and it draws the Finder's context menu, which the
web has no counterpart for.
