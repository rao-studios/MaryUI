# Spotlight

The floating search bar that is also the application dock. `Ctrl+Space` (or `Super+Space`) opens it centred
on the desktop; typing filters apps, commands and open windows; Enter launches. Mirrors `web/src/components/Spotlight`.

## Anatomy
A `flat` Surface, `size.spotlight-width` (560) wide, `radius.lg`, padding `space.2`, `shadow.menu` under a static
sheen. Inside, top to bottom: the **bar** — a `round`, `large` TextField `size.spotlight-bar-height` (44) tall with a
16px `search` icon, `text.lg`, the placeholder “Say “Hey Mary” or type something…” and the accent focus ring while
the panel is up; a hairline `edge.divider`; then either the **dock** (one centred row of 88×84 cells: a 56px
`platinum.0 → platinum.3` plate at `radius.md` with `emboss-raised`, a 28px `ink.secondary` icon, a `text.xs` label
and a 4px `accent.base` dot under running apps) or the **results** (up to 8 ListRows in a `surface.well`).

## States
Dock cell hover `rgba(0,0,0,.04)` · selected `accent.soft` with a 1px inset `accent.light` ring (the Finder's icon
tile) · a selected result row is the accent gradient with `ink.on-accent` · no matches: “No results for “q”.” in
`ink.tertiary`.

## Behavior
Blank query → the dock (every app, hidden ones too, plus Terminal); otherwise ranked matches — title prefix, then a
word prefix, then a substring — including open windows, capped at 8. ↑/↓ wrap, Enter activates the selection, Esc or
a press outside closes, hover moves the selection, a click on a tile or row activates it. Opening closes an open menu
and suspends the window shortcuts; keys never reach clients while it is up.

## Motion
`lp-spotlight-in`: opacity 0 → 1 and scale .96 → 1 about the centre over `motion.fast` `ease-out` (the compositor's
tween); none under reduced motion.

## Tokens
`size.spotlight-width`, `size.spotlight-bar-height`, `size.spotlight-tile`, `z.spotlight`, `radius.lg/-md/-sm/-pill`,
`surface.well`, `shadow.menu`, `shadow.emboss-raised/-well`, `edge.divider`, `accent.*`, `ink.*`, `text.lg/-xs`.

## C
Model: `lp_spotlight.h` (`lp_spotlight_items`, `lp_spotlight_results`, `lp_spotlight_move`). Panel:
`lp_spotlight_panel(ctx, x, y, &(lp_spotlight_view){ .query = &d->spotlight.query, .items = results, .count = n,
.selection = d->spotlight.selection }, &out)`; `out.activated`, `out.hovered`, `out.query_changed`. The host sets
`ctx->focus = LP_SPOTLIGHT_QUERY_ID` when it opens the panel (src/compositor/spotlight.c).
