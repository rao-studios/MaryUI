# Slider

An inset rail with a platinum thumb; the filled portion takes the accent. Mirrors `web/src/components/Slider`
(a native range input there; the same look and keyboard behaviour here).

## Anatomy
grid `auto 1fr auto`, gap `space.2`: label (`text.sm`, `ink.secondary`) · rail 5px `radius.pill`, accent to the
value then `platinum.4`, `emboss-well` · thumb 16px radial `platinum.0 → platinum.3 70% → platinum.5`,
`emboss-raised` + hairline + `0 1px 3px rgba(0,0,0,.3)` · value (`font.mono`, `text.xs`, `ink.tertiary`, tabular).

## States
disabled 50% · focus ring on the thumb · ←/→ step by `step`.

## C
`lp_slider(ctx, id, rect /* h 16 */, &value, (lp_slider_opts){ .min, .max, .step, .label, .show_value, .format })`.
