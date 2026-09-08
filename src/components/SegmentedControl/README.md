# SegmentedControl

A well holding a sliding platinum thumb.

## Anatomy
1. `track` — well, `--lp-radius-pill`, `--lp-shadow-emboss-well`.
2. `gooLayer` — `thumb` (raised metal, `translateX(index × 100%)`, spring transition) + `hotBlob × n` (faint metal that swells toward the hovered segment).
3. `segments` — radio buttons; the selected one gets primary ink and emboss.

## Behavior
Click or ←/→. Hovering the segment next to the selection pulls a bead of metal toward it through the goo filter before the thumb snaps over.

## Tokens
`--lp-size-control-height(-sm)`, `--lp-platinum-1/-3`, `--lp-surface-raised-*`, `--lp-motion-normal`, `--lp-motion-ease-spring`, `--lp-text-sm/xs`.
