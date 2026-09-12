# ObjectIcon

The lit tier: an icon drawn as an object rather than diagrammed as an outline. Mirrors
`web/src/components/ObjectIcon`. Geometry is `objects.json`, generated into `lp_objects.h` and
carrying no colour at all; every pass is the shared recipe under `object` in `tokens.json`, so the
whole set regrades from the tokens the way the wallpaper regrades from `molten`.

## Tiers
`object.tier-glyph-max` (19) and `object.tier-plain-max` (27), of the **logical** size only — a
dpr-dependent tier would draw the same icon differently on two monitors. At or below the first, the
object's declared `glyph` is stroked through `lp_icon_draw` instead. To the second, the body is
drawn flat: the ramps, tone, bevels and creases, without the contact shadow, the grain, the key or
the gloss. Above it, the whole recipe.

## Anatomy
In order: contact shadow (`object.contact-*`), the body ramp per part (the facet picks which stretch
of the material's ramp the face occupies, slid down the ramp by `object.headroom`), tone, the
brushed grain at `object.grain-tile`, the 1px bevel pair (`object.rim` on the lit edge,
`object.occlusion` on the away edge; a `well` flips them), creases as the 1px emboss pair, the broad
key scaled by the material's `gloss`, Aqua's sweep for a `glossy` finish, and the keyline last.

Two numbers are load-bearing. **`object.headroom`** starts a lit body below the material's finished
tone so the key has somewhere to go — at zero, a white key screened over a near-white body does
nothing and every icon reads flat. **`object.grain-tile`** is in grid units, deliberately finer than
`brush.tile`'s 512px: matching the window's density would fit a tenth of a tile behind a 48px icon
and the grain would vanish. Neither is a mistake to be tidied up.

## Materials
The eleven of `object.material.*`. Two resolve at paint time rather than from the token: `accent`
follows the Blue / Graphite preference, and `folder` follows Manila / Slate
(`lp_settings_folders`) — note `object.material.folder` *aliases manila* in `tokens.json`, so the
setting picks the ramp and those defines do not.

## States
None: nothing here animates or samples a clock, so an idle desktop still schedules no frames.

## C
`lp_object_icon.h`. `lp_icon_tier(size)` and `lp_icon_paint_object(cr, icon, box, stroke, color,
settings)` live here (tiers.ts plus Icon.tsx's branch); the passes are `src/draw/lp_object_icon.c`,
the way `ObjectIcon.tsx` sits under `Icon.tsx`. `lp_object_icon_draw(cr, obj, box, settings)` draws
a named object directly, and `lp_object_by_name` looks one up. `lp_file_icon_paint` and Spotlight's
dock both go through `lp_icon_paint_object`, so the Finder's tiles and the dock cannot drift.

An app whose mark is object-only names it with `lp_app.object` (the Finder's `appFinder`), since
`lp_app.icon` is a glyph enum and cannot.
