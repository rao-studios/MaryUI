# TitleBar

The brushed grab handle of a window.

## Anatomy
1. `bar` — `Surface titlebar` with sheen, height `--lp-size-titlebar-height` (42px), padding `0 --lp-space-3`, hairline below.
2. `TrafficLights` — leading.
3. `title` — centered, embossed (`text-shadow: 0 1px 0 --lp-ink-emboss`).
4. `accessory` — optional trailing slot.

## States
active / inactive (`data-active`): inactive swaps to `--lp-surface-titlebar-inactive-*`, title ink to tertiary, sheen alpha to `--lp-sheen-alpha-inactive`.

## Tokens
`--lp-surface-titlebar-*`, `--lp-size-titlebar-height`, `--lp-size-traffic`, `--lp-size-traffic-gap`, `--lp-text-lg`, `--lp-text-weight-semibold`, `--lp-ink-primary`, `--lp-ink-tertiary`, `--lp-ink-emboss`, `--lp-edge-hairline`.

The title is inset by `calc(traffic × 3 + traffic-gap × 2 + space-6)` on both sides, so it stays optically centered whatever the lights grow to. The top corners follow the window's live `--lp-r-tl` / `--lp-r-tr`.

## Sketch notes
Overrides: title, active/inactive.
