# Button

A raised platinum capsule with an embossed label.

## Anatomy
`button` (gradient `--lp-btn-top/--lp-btn-bottom`, emboss, hairline) → `::before` brush grain → `icon` → `label`.

## Variants and sizes
`default` · `primary` (accent gradient, on-accent ink) · `quiet` (metal appears on hover) · `sm` / `md` · `iconOnly` (square, `--lp-radius-sm`).

## States
hover (lighter stops) · active (pressed stops + `--lp-shadow-emboss-pressed`, 0.5px sink) · disabled · focus-visible (accent ring).

## Tokens
`--lp-surface-raised-*`, `--lp-surface-pressed-*`, `--lp-accent-*`, `--lp-size-control-height(-sm)`, `--lp-radius-pill`, `--lp-shadow-emboss-*`, `--lp-edge-hairline`, `--lp-ink-*`.
