# Spotlight

The floating search bar that is also the application dock — and, folded in the same way, the app's
commands. `Ctrl+Space` (or `⌘Space` where the browser lets it through) opens it centred on the desktop;
typing filters apps, commands and open windows; Enter launches. There is no standalone menu bar: File /
Edit / View / Window / Help live here, as pills under the dock (`desktop/menus.ts` builds the models).

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

When the dock is showing and commands are supplied, a second hairline below it introduces the **commands**
section: an optional `Searching <app>` line (`--lp-text-xs`, `--lp-ink-tertiary`) naming the focused window's
app, then a row of **pills** — the Rao monogram (14px `Monogram`) followed by File / Edit / View / Window /
Help, each a 24px-tall `999px`-radius pill. Opening one drops a `MenuItem`/`MenuSeparator` list directly below
the pill row, inside the panel — the same translucent `--lp-surface-menu` treatment the old portaled `Menu`
used, just no longer a second floating object.

## States
Dock cell hover `rgba(0,0,0,.04)` · selected `--lp-accent-soft` with a 1px inset `--lp-accent-light` ring (the
Finder's icon tile) · a selected result row is the accent gradient with `--lp-ink-on-accent` · no matches:
“No results for “q”.” in `--lp-ink-tertiary`. A command pill takes the accent gradient (`data-open`) while its
entries are showing; entries reuse `MenuItem`'s own states (active, checked, disabled).

## Behavior
A blank query shows the dock (every app, hidden ones too, plus Terminal) and, if `menus` is passed, the
commands section beneath it; otherwise the ranked matches — title prefix, then a word prefix, then a
substring — including open windows, capped at 8 (`desktop/spotlight.ts`). ↑/↓ wrap (←/→ too in the dock),
Enter activates the selection, Esc or a press outside closes, hover moves the selection, a click on a tile or
row activates it. Clicking a pill opens its entries (hovering another pill while one is open switches to it,
matching the old menu bar); picking an entry runs it and closes Spotlight; typing anything closes the open
menu, since the commands section only shows on a blank query. Opening Spotlight suspends the window shortcuts.

## Props
`SpotlightPanel`: `query`, `items`, `selection`, `placeholder?`, `menus?`, `context?`, `onQueryChange`,
`onHover`, `onActivate`, `onMove`, `onActivateCommand?`, `inputRef?`. `Spotlight` adds `open` and `onClose`
and portals the panel to `<body>` at `--lp-z-spotlight`. `menus: MenuModel[]` and the `MenuEntry` shape
(`{ id, label, shortcut?, checked?, disabled?, onSelect }` or `{ separator: true }`) live in `desktop/menus.ts`.

## Motion
`lp-spotlight-in`: opacity 0 → 1 and `scale(.96)` → 1 about the centre over `--lp-motion-fast`
`--lp-motion-ease-out`; none under reduced motion. Launching hands off to the window manager's own springs.

## Tokens
`--lp-size-spotlight-width/-bar-height/-tile`, `--lp-z-spotlight`, `--lp-radius-lg/-md/-sm/-pill`,
`--lp-surface-well`, `--lp-surface-menu`, `--lp-shadow-menu`, `--lp-shadow-emboss-raised/-well`,
`--lp-edge-divider`, `--lp-accent-*`, `--lp-ink-*`, `--lp-text-lg/-xs/-sm`.

## Sketch notes
One symbol, with the commands section (pills + one open dropdown) as part of its dock state — see the
`Spotlight` symbol in `design/liquid-platinum.sketch`, placed on the Desktop artboard where the old
standalone `Menu Bar` instance used to sit. `Surface/Menu Bar` remains in the symbols library as a brushed-strip
treatment; the `Menu Bar` symbol is a record of what this replaced, on both targets — the C desktop dropped its
menu bar too (`linux/PARITY.md`), keeping only an ambient clock in the corner, which its View pill can hide.
