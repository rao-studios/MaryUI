# TitleBar

The brushed grab handle of a window.

## Anatomy
1. `bar` — `Surface titlebar` with sheen, height `--lp-size-titlebar-height`, hairline below.
2. `TrafficLights` — leading.
3. `title` — centered, embossed (`text-shadow: 0 1px 0 --lp-ink-emboss`).
4. `accessory` — optional trailing slot.

## States
active / inactive (`data-active`): inactive swaps to `--lp-surface-titlebar-inactive-*`, title ink to tertiary, sheen alpha to `--lp-sheen-alpha-inactive`.

## Tokens
`--lp-surface-titlebar-*`, `--lp-size-titlebar-height`, `--lp-text-md`, `--lp-text-weight-semibold`, `--lp-ink-primary`, `--lp-ink-tertiary`, `--lp-ink-emboss`, `--lp-edge-hairline`.

## Sketch notes
Overrides: title, active/inactive.
