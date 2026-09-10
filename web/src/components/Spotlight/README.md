# Spotlight

The floating search bar that is also the application dock. `Ctrl+Space` (or `⌘Space` where the browser
lets it through) opens it centred on the desktop; typing filters apps, commands and open windows; Enter launches.

## Anatomy
A `flat` Surface, `--lp-size-spotlight-width` (560px) wide, `--lp-radius-lg`, padding `--lp-space-2`,
`--lp-shadow-menu` and `backdrop-filter: blur(14px) saturate(1.1)` (blur, never refraction), a static sheen.
Inside, top to bottom: the **bar** — a `round` `large` TextField `--lp-size-spotlight-bar-height` (44px) tall
with a 16px `search` icon, `--lp-text-lg`, the placeholder “Say “Hey Mary” or type something…” and the accent
focus ring while the panel is up; a hairline `--lp-edge-divider`; then either the **dock** (one centred row of
88×84 cells: a bare 36px `--lp-ink-secondary` icon in a `--lp-size-spotlight-tile` box, a `--lp-text-xs` label
and a 4px `--lp-accent-base` dot under running apps) or the **results** (up to 8 ListRows in a
`--lp-surface-well`). The icon sits in no container of its own — hover and selection belong to the cell around
it, which is what the old raised plate was being mistaken for.

## States
Dock cell hover `rgba(0,0,0,.04)` · selected `--lp-accent-soft` with a 1px inset `--lp-accent-light` ring (the
Finder's icon tile) · a selected result row is the accent gradient with `--lp-ink-on-accent` · no matches:
“No results for “q”.” in `--lp-ink-tertiary`.

## Behavior
A blank query shows the dock (every app, hidden ones too, plus Terminal); otherwise the ranked matches — title
prefix, then a word prefix, then a substring — including open windows, capped at 8 (`desktop/spotlight.ts`).
↑/↓ wrap (←/→ too in the dock), Enter activates the selection, Esc or a press outside closes, hover moves the
selection, a click on a tile or row activates it. Opening closes an open menu and suspends the window shortcuts.

## Props
`SpotlightPanel`: `query`, `items`, `selection`, `placeholder?`, `onQueryChange`, `onHover`, `onActivate`,
`onMove`, `inputRef?`. `Spotlight` adds `open` and `onClose` and portals the panel to `<body>` at `--lp-z-spotlight`.

## Motion
`lp-spotlight-in`: opacity 0 → 1 and `scale(.96)` → 1 about the centre over `--lp-motion-fast`
`--lp-motion-ease-out`; none under reduced motion. Launching hands off to the window manager's own springs.

## Tokens
`--lp-size-spotlight-width/-bar-height/-tile`, `--lp-z-spotlight`, `--lp-radius-lg/-md/-sm/-pill`,
`--lp-surface-well`, `--lp-shadow-menu`, `--lp-shadow-emboss-raised/-well`, `--lp-edge-divider`,
`--lp-accent-*`, `--lp-ink-*`, `--lp-text-lg/-xs`.

## Sketch notes
One symbol, two states (dock, results); the bar is the TextField symbol's `round large` variant.
