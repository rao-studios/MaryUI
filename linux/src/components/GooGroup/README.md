# GooGroup

Blobs behind a row of controls that swell on hover and merge like drops of mercury.

The blobs carry **no lighting of their own** — no emboss, no border. They are painted into an
offscreen layer, and `lp_goo_filter` (`src/draw/lp_goo.c`) thresholds them into one silhouette and
lights *that*: a specular dome raised off the silhouette's own alpha, and a shaded band inside its
lower rim. Blur-and-threshold alone blurs the blobs' colour and emboss along with their alpha, and
the result reads as flat putty with an aliased edge. This is the design direction's rule at
component scale — light the surface, do not paint it — and it is why merged metal stopped looking
like putty.

## Sizes and tensions

Blur scales with the group's size (`xxs` .55, `xs` 1.2, `sm` 1.9, `md` 3.75 × the `goo.blur-*`
tokens). What matters is blur against the *gap*: a bridge forms once the blurred alpha at the
midpoint clears the threshold. `xxs` is for shapes already touching that only need their join
fused — a segmented control's thumb and the segment beside it — where blur is pure cost.

Each size comes in two tensions. `rest` blurs little and thresholds steeply, so blobs stay
distinct; `flow` blurs more and thresholds shallowly, so they bridge readily. A group swaps
between them when it is hovered or when its window is moving — a state change per interaction,
not per frame.

## Motion

The hot blob scales 1.22×, its immediate neighbours 1.08× and lean `goo.attract` px toward it.
Unless the group is marked `still`, each blob also trails against travel by the *shear* — the
velocity the liquid has not caught up with, `ctx->vx - ctx->vx_lag` — scaled by `smear` and its
index, and elongates by `goo.stretch`. The traffic lights are `still`: only slosh moves them.

## API

`lp_goo_group(ctx, area, &spec)` paints the layer; `lp_goo_blob_rect(area, &spec, i)` places
child i. Set `spec.render_blob` to paint something other than blank metal into each blob — the
traffic lights use it for their hidden liquid.
