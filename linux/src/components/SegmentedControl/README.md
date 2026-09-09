# SegmentedControl

A well holding a sliding platinum thumb. Mirrors `web/src/components/SegmentedControl`.

## Anatomy
track `size.control-height` (sm: `-sm`), padding 2, `radius.pill`, `platinum.3`, `emboss-well`; thumb one segment
wide with `surface.raised-*`, `emboss-raised` + `0 1px 2px rgba(0,0,0,.25)`; segments `text.sm`/500 (sm `text.xs`),
`ink.secondary`, selected `ink.primary` embossed; a hover blob (`platinum.1` at 85%, scaled 1.02×1.1) beside the
selection. ≈ The thumb's 200ms spring slide and the goo merge between thumb and blob arrive with motion.

## Behavior
click selects; ←/→ wrap when focused.

## C
`lp_segmented(ctx, id, x, y, options, n, &index, LP_CONTROL_MD)`; `lp_segmented_measure` for the natural size.
