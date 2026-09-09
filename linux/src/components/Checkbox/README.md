# Checkbox

A small raised platinum box; checked, it floods with the accent. Mirrors `web/src/components/Checkbox`.

## Anatomy
15×15 `radius.xs` box: `surface.raised-*` gradient (checked `accent.light → accent.base`), `emboss-raised` +
`0 0 0 1px edge.hairline`; the `check` icon at 11px, stroke 2.6, `ink.on-accent`; label `text.md` at gap `space.2`.

## States
disabled label `ink.disabled` · focus 3px ring. ≈ The mark's 120/200ms fade + spring scale arrives with motion.

## C
`lp_checkbox(ctx, id, x, y, &checked, "Remember through Thread", 0)`; `lp_checkbox_measure` for layout.
