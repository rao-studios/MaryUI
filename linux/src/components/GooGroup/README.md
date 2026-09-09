# GooGroup

Makes adjacent controls merge like drops of mercury. Mirrors `web/src/components/GooGroup`.

## Anatomy
A row of blank metal blobs (`surface.raised-top → platinum.5`, emboss-raised) under the real children; hovering a
child swells its blob (×1.22) and nudges its neighbours (×1.08); a fast drag smears the blobs sideways
progressively along the row (`vx × smear × index`).

## Sizes
`xs` · `sm` · `md` — the strength of the merge filter (blur 1.6/3/6, alpha slope 20/18/19, intercept −8/−7/−9 in
`SvgDefs.tsx`). ≈ The filter itself (blur + alpha contrast on the blob layer) arrives with motion; until then the
blobs sit unmerged, exactly as the web with `data-goo="off"`.

## C
`lp_goo_group(ctx, area, &spec)` paints the blob layer; `lp_goo_blob_rect(area, &spec, i)` places child i.
