# Toggle

An Aqua-style capsule whose knob is a LiquidBubble. Off, the knob is plain platinum in a dark well; on, the well
floods with the accent and the knob's liquid takes the tint and slides across. Mirrors `web/src/components/Toggle`.

## Anatomy
38×22 pill: off `platinum.5 → platinum.4`, on `accent.deep → accent.base`, `emboss-well`; knob 16px at (3,3)
(+16px when on), `0 1px 2px rgba(0,0,0,.35)`, a LiquidBubble `platinum` fill 0.5 / `accent` fill 0.72.

## States
disabled: 55% opacity · focus: 3px `accent.focus-ring`. ≈ The 200ms spring slide arrives with motion.

## C
`if (lp_toggle(ctx, LP_ID("ambient"), x, y, &on, 0)) …`
