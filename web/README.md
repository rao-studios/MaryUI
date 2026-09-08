# MaryUI — Liquid Platinum

Rao's desktop-first design system, shown as a simulated desktop. Every surface is brushed platinum that
behaves like liquid: the specular sheen slides across a window as you drag it, the chrome stretches and
springs back, the traffic-light bubbles hold water that sloshes with the window's acceleration, and
adjacent controls merge like drops of mercury. No refraction shaders — the metal is solid; only the
light and the liquid move.

```sh
npm install
npm run dev        # http://127.0.0.1:8140 — hot reloads code, CSS, and tokens
npm test           # physics, geometry, window manager, token pipeline
npm run typecheck
npm run build
npm run sketch     # design/liquid-platinum.sketch (needs Chrome for the textures; falls back otherwise)
```

## What you are looking at

- **Menu bar** — the Rao monogram menu, File / Edit / View / Window / Help, a live clock. View toggles
  Liquid Merge (the goo filter), procedural vs. raster wallpaper, and the Blue / Graphite appearance.
- **Windows** — drag the title bar, resize from any edge or corner, red closes, yellow **shades**
  (collapses to the title bar, Mac OS 8/9 style), green zooms with a spring, double-click zooms.
  `⌘/Ctrl+W` close, `⌘/Ctrl+M` shade, `Ctrl+\`` cycle.
- **Rao (Finder)** — sidebar, icon and list views, search, status bar: every primitive inside real chrome.
- **Liquid Platinum (Gallery)** — every control in every state, the surfaces, the bubbles, the token
  swatches, and a **Motion** tab whose sliders retune the physics live. *Copy JSON patch* gives you the
  values to paste back into `tokens/tokens.json`.
- **About** — the platinum monogram.

## Layout

```
tokens/tokens.json           SOURCE OF TRUTH (W3C Design Tokens). Edit this.
tokens/export/               liquid-platinum.sketchpalette + palette-index.txt (generated)
scripts/build-tokens.mjs     tokens.json → tokens.css, tokens.ts, .sketchpalette
scripts/build-sketch.mjs     tokens + component anatomy → design/liquid-platinum.sketch
src/styles/tokens.css        generated CSS custom properties (--lp-*)
src/tokens/tokens.ts         generated typed constants (the engine reads motion numbers here)
src/lib/                     framework-free: spring, slosh, velocity, geometry, motionEngine, textures
src/hooks/                   useDrag, useResize, useMotionTarget, useOutsideClick, useDesktopKeys, useClock
src/components/<Name>/       <Name>.tsx · <Name>.module.css · index.ts · README.md
src/desktop/                 Desktop, Wallpaper, WindowLayer, settings, menus, wm/ (store + reducer), apps/
```

## How the metal works

| Effect | Mechanism | Where |
| --- | --- | --- |
| Brushed grain | One seamless 512px `feTurbulence` tile with diagonal strokes (lattice angle `brush.angle`, tan θ = rise/run) baked to a data URI at startup, painted with `mix-blend-mode: overlay` | `src/lib/textures.ts`, `Surface.module.css` |
| Sliding sheen | A highlight band positioned by `--lp-sheen-x`; the light is fixed to the *room* (`sheen.light-x`), so moving a window slides the highlight across it, spring-lagged | `motionEngine.ts`, `Surface.module.css` |
| Jelly | Velocity → springs → `skewX` / `scale` on the window chrome, origin at the grab point | `motionEngine.ts` |
| Slosh | A damped pendulum driven by the window's acceleration writes `--lp-slosh` / `--lp-slosh-y`; bubbles rotate their liquid with it in pure CSS | `src/lib/slosh.ts`, `LiquidBubble.module.css` |
| Liquid merge | Blank metal blobs behind a control pass through `feGaussianBlur` + an alpha-contrast matrix; content stays crisp above | `GooGroup`, `SegmentedControl`, `SvgDefs` |
| Wallpaper | Fractal noise lit by `feDiffuseLighting` + `feSpecularLighting`, rendered once to a canvas; a CSS soft-light highlight drifts over it | `desktop/Wallpaper.tsx` |

Two rules keep it smooth: **React owns layout, the engine owns `transform`** (nothing re-renders during a
drag), and **only `transform`/`opacity` animate per frame** (filters are baked or tiny). An idle desktop
schedules no animation frames at all.

## Tokens

`tokens/tokens.json` is DTCG-shaped: groups may set `$type`, values may alias with `{platinum.3}`. The
build emits three things, and the Vite dev server re-runs it whenever the file changes:

- `--lp-<path-with-dashes>` custom properties (`--lp-surface-titlebar-top`, `--lp-motion-spring-jelly-frequency`).
- `tokens.ts` with the same tree as camelCase constants plus `cssVar('platinum-2')`.
- A Sketch Palettes file. The format carries no names, so `palette-index.txt` lists them in the same order.

Motion constants (`motion.spring-*`, `motion.slosh-*`, `motion.jelly-*`) live in the same file so the
engine and the designer read the same numbers. The Blue/Graphite accent mapping is in `src/styles/base.css`.

## Adding a component

1. `src/components/Thing/Thing.tsx`, `Thing.module.css`, `index.ts`, `README.md`.
2. Style with `var(--lp-*)` only; compose `Surface` for any metal.
3. Wrap adjacent controls in `GooGroup` if they should merge; keep text out of the filtered layer.
4. If it should react to window motion, consume `--lp-sheen-x`, `--lp-tilt`, `--lp-vx`, `--lp-slosh`, `--lp-slosh-y` — never run your own loop.
5. Add it to the Gallery's Controls tab, and document anatomy → tokens → states in the README so it can become a Sketch symbol.

## Taking it elsewhere

- **Sketch**: `npm run sketch` writes `design/liquid-platinum.sketch` — Color Variables for every token, a symbol per component variant, Components/Tokens/Desktop artboards, brushed overlay at max (`--brush=` to change). See `design/README.md`. The palette-only route is `tokens/export/liquid-platinum.sketchpalette`.
- **Swift / other codebases**: `tokens.json` is the contract; add an emitter to `scripts/tokens-lib.mjs` (a `Color(red:green:blue:opacity:)` writer is ~20 lines). The physics in `src/lib/spring.ts` and `src/lib/slosh.ts` are a dozen lines each and port directly.
- **Wallpaper**: drop `public/wallpaper/platinum.jpg` and choose View › Raster Wallpaper.

Apache-2.0.
