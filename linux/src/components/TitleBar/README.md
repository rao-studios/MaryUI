# TitleBar

The brushed grab handle of a window: traffic lights on the left, an embossed centred title, the sliding sheen
behind both. Double-click zooms. Mirrors `web/src/components/TitleBar`.

## Anatomy
`Surface titlebar` with sheen, height `size.titlebar-height` (28), padding `0 space.2`, `emboss-raised` +
`0 1px 0 edge.hairline` below; TrafficLights at x = 8; the title absolutely centred between 84px insets
(`text.md`/600, embossed, ellipsis).

## States
inactive: `surface.titlebar-inactive-*` stops, `sheen.alpha-inactive`, `ink.tertiary` title.

## Behavior
Press outside the lights → `drag_start`; double-click → `double_click`; the lights report close / shade / zoom.

## C
`lp_title_bar(ctx, rect, &(lp_title_bar_model){ .title, .active, .shaded, .zoomed, .radius_top, .radius_bottom }, &result)`.
