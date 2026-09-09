# ProgressBar

An inset rail with a liquid accent fill. Indeterminate shows Aqua's barber pole, in platinum.
Mirrors `web/src/components/ProgressBar`.

## Anatomy
rail 8px `radius.pill` `platinum.4` `emboss-well`; fill `accent.light → accent.base 55% → accent.deep` with an
inset 1px highlight and a 40%-wide glint sweeping every 2.8s; indeterminate: `-55deg` stripes `platinum.1`/
`platinum.4` 8px each, travelling 22.6px per 1.1s. Both ask the host for frames (`lp_want_frame`).

## C
`lp_progress(ctx, rect /* h 8 */, 0.42f)`; `lp_progress(ctx, rect, -1)` for indeterminate.
