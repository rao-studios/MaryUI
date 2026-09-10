# SegmentedControl

A well holding a sliding platinum thumb.

## Anatomy
1. `track` — well, `--lp-radius-pill`, `--lp-shadow-emboss-well`.
2. `gooLayer` — `thumb` (raised metal, `translateX(index × 100%)`, spring transition) + `hotBlob × n` (faint metal that swells toward the hovered segment).
3. `segments` — radio buttons; the selected one gets primary ink and emboss.

## Behavior
Click or ←/→. Hovering the segment next to the selection pulls a bead of metal toward it through the goo filter before the thumb snaps over.

## Tokens
`--lp-size-segmented-height(-sm)`, `--lp-platinum-1/-3`, `--lp-surface-raised-*`, `--lp-motion-normal`, `--lp-motion-ease-spring`, `--lp-text-md/sm`.

## Metrics
28px track / `--lp-size-segmented-pad` (3px) around the thumb / 16px segment padding / `--lp-text-md`; `sm` is 22 / 3 / 12 / `--lp-text-sm`. These are deliberately **not** `--lp-size-control-height`: a segmented control stacks a label inside a thumb inside a track inside the track padding, so a height sized for a button leaves it squished.

The merge layer uses the `xxs` filter. The thumb and the hover bead already touch, so a larger blur only erodes the pill's own caps — see `SvgDefs/README.md`.
