# TextField

An inset well with an optional leading icon. Focus lights the accent ring; the well itself never changes colour.
Mirrors `web/src/components/TextField`.

## Anatomy
height `size.control-height`, padding `0 space.2` (`round`: `0 space.3`, `radius.pill`), `radius.sm`,
`surface.well`, `emboss-well`; icon 14px `ink.tertiary`; text `text.md`; placeholder `ink.tertiary`; a 1px caret.

## States
focus: `0 0 0 3px accent.focus-ring`, the caret, typing edits the buffer (Backspace, Delete, ←/→, Home/End) ·
disabled `ink.disabled`.

## C
`lp_text_field(ctx, id, rect, &buffer, (lp_text_field_opts){ .placeholder = "Search", .icon = LP_ICON_SEARCH, .round = 1 })`.
