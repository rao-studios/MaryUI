# design/

`liquid-platinum.sketch` is generated — do not hand-edit it as a source of truth. Rebuild with:

```sh
npm run sketch                 # brush overlay at max (0.8)
npm run sketch -- --brush=0.3  # closer to the web default (tokens brush.opacity)
```

What the file contains:

- **Color Variables** — one per color token in `tokens/tokens.json`, named `platinum/2`, `surface/titlebar-top`, `traffic/close/base`, …
- **Symbols page** — a symbol master per component variant (65 in total): Window (at rest and In Motion), Title Bar, Menu Bar, Toolbar, Traffic Lights, Liquid Merge (Apart / Necking / Merged), Bubble (16/18/40 × six tints), Button, Segmented Control, Toggle, Checkbox, Slider, Text Field, Progress Bar, Menu + Menu Item, Sidebar Item, List Row, Surface variants, Monogram. Layer names match the anatomy in each component's README.
- **Liquid Platinum page** — four artboards: *Desktop* (the composed desktop over the rendered wallpaper), *Components* (every symbol with a label), *Tokens* (swatches, type ramp, radii, spacing), *Icons* (every mark in the system, plus a strip of objects on the molten desk).
- **images/** — the brushed tile (rendered from `src/lib/brushSvg.ts`, 512px, tiled at 0.5×) and the wallpaper (the molten shader in `src/lib/moltenShader.ts`, run once in headless WebGL at the Platinum grade; it falls back to `src/lib/wallpaperSvg.ts` where WebGL will not start). Both come from the exact sources the web app uses, so the metal matches.

The brushed grain is a pattern fill with Overlay blending on every metal layer; its opacity is the `--brush` flag. In the app the grain is one sheet anchored to the desktop — a moving window uncovers a different part of it, and the scratches run unbroken across surfaces (see `src/components/Surface/README.md`). Sketch tiles a pattern from each layer's own origin, so in this file the grain restarts at every layer and does not line up across a seam; that is a limit of the format, not the design. **Icons come in two forms, and the difference matters.** The *Icons* artboard is a browser render at 2×, placed as one bitmap — a reference, not a source. It is rendered rather than rebuilt in Sketch shapes because the object recipe leans on `overlay` and `screen` blending, and Sketch's blend maths differs from the browser's, so a faithful reconstruction would not actually look faithful.

The editable vectors live beside the file, in `icons/`, written by `npm run icons`: one `.svg` per mark, 81 glyphs and 69 objects. Drag one in and Sketch's own importer converts it to paths — which is why there is no shape emitter here; that importer already handles the arc decomposition and curve-mode reconstruction correctly. Every layer is named after the token its colour came from (`back · folder/top`, `keyline · edge/hairline`), so the shapes can be re-bound to the Color Variables above.

Two passes are absent from the `.svg` files on purpose: the brushed grain, which would arrive as a rasterised noise fill instead of the pattern Sketch should apply itself, and the contact shadow, which belongs on the layer as a real Sketch shadow. The Icons artboard shows both as the app draws them.

The monogram uses live text in Iowan Old Style; convert it to outlines once the mark is final.
