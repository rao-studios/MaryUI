# design/

`liquid-platinum.sketch` is generated — do not hand-edit it as a source of truth. Rebuild with:

```sh
npm run sketch                 # brush overlay at max (0.8)
npm run sketch -- --brush=0.3  # closer to the web default (tokens brush.opacity)
```

What the file contains:

- **Color Variables** — one per color token in `tokens/tokens.json`, named `platinum/2`, `surface/titlebar-top`, `traffic/close/base`, …
- **Symbols page** — a symbol master per component variant (60 in total): Window, Title Bar, Menu Bar, Toolbar, Traffic Lights, Bubble (12/16/40 × six tints), Button, Segmented Control, Toggle, Checkbox, Slider, Text Field, Progress Bar, Menu + Menu Item, Sidebar Item, List Row, Surface variants, Monogram. Layer names match the anatomy in each component's README.
- **Liquid Platinum page** — three artboards: *Desktop* (the composed desktop over the rendered wallpaper), *Components* (every symbol with a label), *Tokens* (swatches, type ramp, radii, spacing).
- **images/** — the brushed tile (rendered from `src/lib/brushSvg.ts`, 512px, tiled at 0.5×) and the wallpaper (`src/lib/wallpaperSvg.ts`). Both are the exact SVGs the web app uses, so the metal matches.

The brushed grain is a pattern fill with Overlay blending on every metal layer; its opacity is the `--brush` flag. Icons are not in the file — paste the path data from `src/components/Icon/Icon.tsx` as SVG when you need them. The monogram uses live text in Iowan Old Style; convert it to outlines once the mark is final.
