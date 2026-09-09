# ScrollArea

An overflow container with thin platinum scrollbars. Mirrors `web/src/components/ScrollArea`.

## Anatomy
10px lane, thumb with a 2px transparent border, `radius.pill`, `platinum.4 → platinum.6`, transparent track.

## C
`lp_rect content = lp_scroll_begin(ctx, id, viewport, (lp_size){ w, h }, &state); … lp_scroll_end(ctx);` — draw
children relative to `content`; the wheel scrolls in the EVENT pass.
