# Spotlight

The floating search bar that is also the application dock — and, folded in the same way, the app's commands.
`Ctrl+Space` (or `Super+Space`) opens it centred on the desktop; typing filters apps, commands and open windows;
Enter launches. There is no menu bar: Rao / File / Edit / View / Go / Window / Help are pills under the dock
(`lp_desktop_build_menus` builds them). Mirrors `web/src/components/Spotlight`.

## Anatomy
A `flat` Surface, `size.spotlight-width` (560) wide, `radius.lg`, padding `space.2`, `shadow.menu` under a static
sheen. Inside, top to bottom: the **bar** — a `round`, `large` TextField `size.spotlight-bar-height` (44) tall with a
16px `search` icon, `text.lg`, the placeholder “Say “Hey Mary” or type something…” and the accent focus ring while
the panel is up; a hairline `edge.divider`; then either the **dock** (one centred row of 88×84 cells: a 56px
`platinum.0 → platinum.3` plate at `radius.md` with `emboss-raised`, a 28px `ink.secondary` icon, a `text.xs` label
and a 4px `accent.base` dot under running apps) or the **results** (up to 8 ListRows in a `surface.well`).

With the dock showing, a second hairline introduces the **commands**: a `Searching <app>` line (`text.xs`,
`ink.tertiary`, the app's name in `ink.secondary`) naming the focused window's app, then a wrapping row of 24px
**pills** at `radius.pill` — the 14px flat Monogram, then one per menu — and, under the open one, its entries drawn
by `lp_menu_list` inside a `surface.menu` panel at `radius.md` with `shadow.menu`. Unlike the old floating menu that
panel carries no `emboss-raised`: the web's `.cmdDropdown` asks for the shadow alone.

## States
Dock cell hover `rgba(0,0,0,.04)` · selected `accent.soft` with a 1px inset `accent.light` ring (the Finder's icon
tile) · a selected result row is the accent gradient with `ink.on-accent` · no matches: “No results for “q”.” in
`ink.tertiary` · the open pill takes the accent gradient with `ink.on-accent`; its entries carry MenuItem's own
states (active, checked, disabled).

## Behavior
Blank query → the dock (every app, hidden ones too, plus Terminal) and the commands beneath it; otherwise ranked
matches — title prefix, then a word prefix, then a substring — including open windows, capped at 8. ↑/↓ wrap, Enter
activates the selection, Esc or a press outside closes, hover moves the selection, a click on a tile or row
activates it. A **press** on a pill opens its entries (the web uses a click; this is the Mac pull-down, and it
matches the context menu), hovering another pill while one is open switches to it, and choosing an entry runs it and
closes the panel. Typing hides the commands, so it also closes the open pill. While a pill is open it owns ↑/↓
(entries), ←/→ (pills) and Enter; Esc closes the pill first and the panel second; every other key still reaches the
bar. Opening Spotlight suspends the window shortcuts; keys never reach clients while it is up.

A menu taller than the room below the panel is clipped rather than run off the screen (`view.max_h`), so a Window
menu with a dozen-odd windows in it can hide its last entries. Scrolling the list is the follow-up; the web has the
same limit.

## Motion
`lp-spotlight-in`: opacity 0 → 1 and scale .96 → 1 about the centre over `motion.fast` `ease-out` (the compositor's
tween); none under reduced motion.

## Tokens
`size.spotlight-width`, `size.spotlight-bar-height`, `size.spotlight-tile`, `z.spotlight`, `radius.lg/-md/-sm/-pill`,
`surface.well`, `surface.menu`, `shadow.menu`, `shadow.emboss-raised/-well`, `edge.divider`, `accent.*`, `ink.*`,
`text.lg/-sm/-xs`.

## C
Model: `lp_spotlight.h` (`lp_spotlight_items`, `lp_spotlight_results`, `lp_spotlight_move`); the commands are
`lp_menus.h`, and `lp_desktop_spotlight_view(d, results, n)` assembles the whole view — the one place that decides
which menus and which context line, so the compositor and `lp-render` cannot drift. Panel:
`lp_spotlight_panel(ctx, x, y, &view, &out)`, with `out.activated`, `out.hovered`, `out.query_changed`,
`out.menu_pressed`, `out.menu_hovered`, `out.entry_hovered`, `out.entry_selected`. The host sets
`ctx->focus = LP_SPOTLIGHT_QUERY_ID` when it opens the panel (src/compositor/spotlight.c) and fills in
`view.width`, `view.focus_bar` and `view.max_h`.

`lp_spotlight_max_size(&view)` is the panel at its tallest with **no** pill open, which is what the host allocates
once so that typing never reallocates — opening a pill is a deliberate click, and `mui_spotlight_resize` sizes the
chrome for it. That split matters: `mui_spotlight_sync` frees the chrome when Spotlight closes, so anything that
closes it must go through `mui_spotlight_request_sync` instead.
