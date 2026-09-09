# Button

A raised platinum capsule with an embossed label. Mirrors `web/src/components/Button`.

## Variants
`default` (raised metal) · `primary` (accent gradient, `ink.on-accent`) · `quiet` (transparent until hovered).
Sizes `md` (22) · `sm` (18, `text.sm`, padding `space.2`). `icon_only` makes it square with `radius.sm`.

## States
hover: `platinum.0 → platinum.2` (primary: `accent.light → accent.deep`) · active: `surface.pressed-*` inverted
gradient, `emboss-pressed`, sunk 0.5px (primary: `accent.deep → accent.base`) · disabled: `ink.disabled`, 70%
opacity · focus: 3px `accent.focus-ring`.

## Anatomy
outer `0 0 0 1px edge.hairline` + `0 1px 2px rgba(0,0,0,.18)` → gradient → brush at `brush.opacity × 0.8` →
`emboss-raised` → icon (16, `ink.secondary`) + label (`text.md`/500, embossed).

## C
`lp_button(ctx, id, rect, "Label", (lp_button_opts){ .variant, .size, .icon, .icon_only, .disabled })` → clicked;
`lp_button_measure` gives the natural size.
