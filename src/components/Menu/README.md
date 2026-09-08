# Menu

A dropdown panel of MenuItems, portaled to `<body>` so no surface clips it.

## Anatomy
`menu` — `--lp-surface-menu` (94% platinum) with `backdrop-filter: blur` (blur, never refraction), radius `--lp-radius-md`, shadow `--lp-shadow-menu`, padding `--lp-space-1`.

## Behavior
Roving highlight with ↑/↓, Enter/Space selects, Esc closes, ←/→ hand off to the MenuBar. Entries: `{ id, label, shortcut?, checked?, disabled?, onSelect }` or `{ separator: true }`.
