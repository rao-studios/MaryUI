# MenuBar

The brushed strip across the top of the desktop.

## Anatomy
1. `bar` — `Surface bar` with sheen, height `--lp-size-menubar-height`.
2. `trigger × n` — monogram (opens the Rao menu) then File / Edit / View / Window / Help; open trigger takes the accent gradient.
3. `right` — status slot and clock.
4. `Menu` — portaled dropdown for the open trigger.

## Behavior
Click opens; hovering another trigger while open switches; Esc or outside click closes; ←/→ move between menus, ↑/↓ within.

## Tokens
`--lp-surface-menubar-*`, `--lp-size-menubar-height`, `--lp-accent-*`, `--lp-ink-primary`, `--lp-ink-emboss`, `--lp-z-menubar`.

Models live in `src/desktop/menus.ts`.
