# GooGroup

Metal blobs behind a row of controls. The hot one swells, its immediate neighbours lean
`goo.attract` toward it, and unless the group is marked `still` each blob trails against travel by
the *shear* — the velocity the liquid has not caught up with, `ctx->vx - ctx->vx_lag` — and
elongates by `goo.stretch`.

Each blob is drawn discrete, lit by its own emboss. The web instead runs the whole group through an
SVG filter that blurs the blobs together, thresholds them into a single silhouette and lights that,
so they flow into one another. That is **not** ported — see **D8** in `../../../PARITY.md`. It was,
briefly: the filter is a Gaussian blur and a `pow()` per pixel with no clip test, so it ran on every
repaint of every window, and at the sizes this system uses the merged result read as putty rather
than mercury.

## API

`lp_goo_group(ctx, area, &spec)` paints the blobs; `lp_goo_blob_rect(area, &spec, i)` places child i
(before hover scaling), for hit-testing.
