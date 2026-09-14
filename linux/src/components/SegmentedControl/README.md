# SegmentedControl

A well holding a sliding platinum thumb. Mirrors `web/src/components/SegmentedControl`.

## Anatomy
track `size.control-height` (sm: `-sm`), padding 2, `radius.pill`, `platinum.3`, `emboss-well`; thumb one segment
wide with `surface.raised-*`, `emboss-raised` + `0 1px 2px rgba(0,0,0,.25)`; segments `text.sm`/500 (sm `text.xs`),
`ink.secondary`, selected `ink.primary` embossed; a hover blob (`platinum.1` at 85%, scaled 1.02×1.1) beside the
selection. The thumb springs to a new selection (settling in about `motion.normal`); the hovered segment's
bead eases in over `motion.fast` (opacity) and `motion.normal` (scale .6 → 1.02×1.1, leaning `goo.attract`
toward the thumb) and out over `motion.normal`. Thumb and bead are one merged layer, SvgDefs' xxs goo
filter in Cairo: blurred together, the alpha cut back to a silhouette by the tension's slope and
intercept (rest, or flow while the pointer is over the track), a specular dome lit from the room light, a
shaded band inside the lower rim. Every bead goes into the layer, so at a crossing the one falling and the
one rising merge rather than blink; beads and tension ease on critically damped springs, continuous through a
sweep or a reversal. The cut is anti-aliased: the slope is held to what keeps the silhouette's edge one device
pixel wide (at the tokens' slope it is a tenth of one, so a sliding thumb's edge crawled and a fading bead
popped in whole), with the intercept moved so the cut stays at the tokens' alpha. The label ink follows the
thumb's coverage of each segment.

## Behavior
click selects; ←/→ wrap when focused. Motion runs in the DRAW pass and asks for its frames with
`lp_want_motion_rect` for the track alone (the compositor serves those every refresh, not at the ambient
30 Hz, which showed a 200 ms slide as six steps); a control outside the damage clip paints nothing. The EVENT pass
measures the segments exactly as the DRAW pass draws them (no estimate), and a hover change or the pointer
crossing the track's edge damages the whole track, since the merge reshapes the thumb's cap as well as the
bead. With no clock (`now_ms` 0: a render, a test) or reduced motion the thumb simply sits at its index.

## C
`lp_segmented(ctx, id, x, y, options, n, &index, LP_CONTROL_MD)`; `lp_segmented_measure` for the natural size;
`lp_segmented_masked` with some segments disabled; `lp_segmented_thumb(id)` (tests, renders) says where the
thumb is in segment units and `lp_segmented_bead(id, i)` what bead a segment carried on the last frame. `lp-render --segmented` draws one at rest, one mid-slide and one with its bead.
