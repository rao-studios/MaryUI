# ObjectIcon

The lit tier: an icon drawn as an object rather than diagrammed as an outline.

Reach for it through `<Icon variant="object">` rather than directly — `Icon` owns the small-size
fallback, and an object that cannot be drawn should quietly become its glyph rather than fail.

## Anatomy

`objects.json` holds geometry and nothing else — no colour, no gradient, no shadow anywhere in the
file. An author supplies, per part:

| field | meaning |
|---|---|
| `d` | the outline, on a 32 grid |
| `facet` | `top` · `front` · `under` · `flat` — which way the face is turned to the key. This is what makes geometry into lighting without a per-icon gradient |
| `tone` | a signed lighting offset in `object.tone-step` units, screened or multiplied. Not a colour |
| `bevel` | `raised` · `well` · `none` — the 1px pair on that part's own outline |
| `role` | `body` (default) fills; `crease` draws the emboss pair; `mark` strokes on top |
| `material` | overrides the icon's material for one part (a pencil's ferrule) |
| `tint` | a dotted tokens.json path — a flat fill for a small detail that is not made of the material |
| `min` | drop this part below this rendered size |

Per icon: a `material`, which part is the `silhouette`, and — only for names with no glyph of their
own — a `glyph` to fall back to.

## Tokens

Everything else is the `object` group in `tokens/tokens.json`, so the whole set regrades from one
place the way the wallpaper regrades from `molten`. `object.light-azimuth` aliases
`goo.specular-azimuth`, which means an icon is lit by the same value as the traffic-light beads, the
sliding sheen and every 1px emboss in the system.

**`object.headroom` is the one that will bite you.** A lit body starts that far below the material's
finished tone so the key light has somewhere to go. Set it to zero and a white key screened over an
already near-white body does nothing at all — the recipe collapses to a flat fill and the object
reads as painted rather than lit, which is exactly what `docs/design-direction.md` argues against.

## Two finishes

`lit` is the default: the eight passes below. `glossy` adds Aqua's sweep over the top of them —
`object.gloss-*` — and is for **application icons only**. The body underneath stays a lit material,
so a glossy icon still regrades; the gloss is a finish on the recipe, not a replacement for it.

`object.gloss-alpha` is screened, so it is *how far the body is driven toward white*. Above about
0.6 a saturated material comes out colourless, which is not what Aqua actually did.

## Materials with more than one appearance

`accent` follows Blue / Graphite; `folder` follows Manila / Slate (View › Folders). Their ramps are
CSS variables, not literals, so they cannot be shared from `SvgDefs` — a `var()` inside a gradient
stop resolves against the *gradient element's* own context, and `SvgDefs` sits at the app root, so a
shared gradient would always report the root's appearance. Those two get a gradient per instance
instead: one extra node on the icons that use them, and it buys both the global switch and
side-by-side comparison, because any element carrying `data-folders` re-roots the appearance for its
subtree.

## The passes

Contact shadow → body ramp → tone → brushed grain → the broad key → 1px rim on the lit edge → 1px
occlusion on the away edge → keyline, last. Bodies from soft light, edges from hard light.

Layered gradients, not `feSpecularLighting`. A lighting filter reads alpha as a height field, and at
these sizes a 1px bump has no gradient to light — it collapses to a flat wash or an aliased ring.
Filters also allocate an offscreen surface per element, which is real cost when Finder shows twenty,
and they quantise the keyline's antialiasing. The one filter kept is the contact shadow.

## Sizes

| tier | size | passes |
|---|---|---|
| `glyph` | ≤ `object.tier-glyph-max` (19px) | the stroked mark from `icons.json` |
| `plain` | ≤ `object.tier-plain-max` (27px) | body, tone, bevel, keyline |
| `lit` | above that | all of them |

Tier is a function of the rendered CSS size only, never `devicePixelRatio` — that would draw the
same icon differently on two monitors and would need a resize listener, and a listener costs frames.

Below 20px the rim and the keyline land on the same physical pixel and cancel to grey, the ramp
compresses to two perceptual steps, and the grain's hairlines fall below a pixel and turn to noise.
So the sidebar, the list rows, Spotlight's results and the icons inside controls all stay on the
glyph tier and look exactly as they always did.

## Motion

None. Nothing here animates and nothing samples a clock, so an idle desktop still schedules zero
frames.

## Sketch notes

Layer names follow the part ids. The gradients are the `object.material.*` ramps; the brushed grain
is `Surface`'s tile at Overlay, but at `object.grain-tile` density rather than `brush.tile` — see
below.

## The one deliberate departure

An object's grain is not the window's grain. `Surface` paints one tile across 512px; an icon paints
one across `object.grain-tile` grid units, which at 48px is about 36px — roughly fourteen times
finer. Matching the window would put a tenth of a tile behind an icon and the grain would flatten
into a wash. It reads as the same metal without being the same sheet, and that is a choice rather
than an oversight.

## Portability

`objects.json` is web-only for now, and `scripts/build-tokens.mjs` must never read it: doing so
would change `lp_icons.h`, bump `LP_ICON_COUNT` and break `linux/tests/test_icons.c`. There is a
test in `objects.test.ts` that holds that line. The C port is a direct transliteration when it comes
— every pass is a fill, so `cairo_pattern_create_linear` plus `CAIRO_OPERATOR_SCREEN`/`OVERLAY`
covers it, and `lp_svgpath.c` already parses the full path grammar.
