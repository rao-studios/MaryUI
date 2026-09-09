# Icon

Inline 24×24 stroked glyphs in `currentColor`. Add a glyph by appending its path data to `icons.json`
(one entry, a list of `d` strings); the name becomes available everywhere, in the Gallery, and — via
`npm run tokens` — in the C desktop's `linux/include/maryui/lp_icons.h`, which `Icon.tsx` and
`lp_icon_draw()` both render from the same file.
