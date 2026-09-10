# MaryUI — Liquid Platinum

Rao's desktop-first design system, shown as a simulated desktop. Every surface is brushed platinum that
behaves like liquid: the specular sheen slides across a window as you drag it, the hairline scratches
lag behind the frame and spring back, all four corners flex independently between 8 and 16px, the
traffic lights are glass beads of water — the same bead the Toggle uses for its knob — that slosh as you
drag and whose glass fades mid-fling so the liquid bridges into one ribbon, and adjacent controls merge
like drops of mercury. No refraction shaders — the metal is solid; only the light and the liquid move.

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
  Liquid Merge (the goo filter), the wallpaper (molten / procedural / raster) and the molten grade
  (Platinum / Faithful), and the Blue / Graphite appearance.
- **Windows** — drag the title bar, resize from any edge or corner, red closes, yellow **shades**
  (collapses to the title bar, Mac OS 8/9 style), green zooms with a spring, double-click zooms.
  `⌘/Ctrl+W` close, `⌘/Ctrl+M` shade, `Ctrl+\`` cycle. Fling one sideways and watch the corners: the
  ones it leads with flatten, the ones it trails with round, and they arrive home one after another.
- **Rao (Finder)** — sidebar, icon and list views, search, status bar: every primitive inside real chrome.
- **Liquid Platinum (Gallery)** — every control in every state, the surfaces, the bubbles, the token
  swatches, and three tuning surfaces: **Motion** retunes the physics live, **Colors** recolours every
  bead's tint (both hand back a *Copy JSON patch* to paste into `tokens/tokens.json`), and **Debug**
  turns on the crest hairline, reveals the raw merge layer, and freezes the ambient wave.
- **About** — the platinum monogram.
- **Spotlight** — `Ctrl+Space` (or `⌘Space` where the browser lets it through). The pill-shaped search bar is
  also the dock: a blank query shows every app as a tile (running ones carry a dot); typing filters apps,
  commands and open windows; `↑/↓` move, `Enter` launches, `Esc` or a click outside closes.
- **TextEdit** — the first application, reachable only from Spotlight: a name field, Save, a TextArea, a
  status bar with words, characters and `Ln, Col`; `⌘/Ctrl+S` keeps the document in `localStorage`
  (the C desktop writes `~/Documents/<name>.txt`).

## Layout

```
tokens/tokens.json           SOURCE OF TRUTH (W3C Design Tokens). Edit this.
tokens/export/               liquid-platinum.sketchpalette + palette-index.txt (generated)
scripts/build-tokens.mjs     tokens.json → tokens.css, tokens.ts, .sketchpalette
scripts/build-sketch.mjs     tokens + component anatomy → design/liquid-platinum.sketch
src/styles/tokens.css        generated CSS custom properties (--lp-*)
src/tokens/tokens.ts         generated typed constants (the engine reads motion numbers here)
src/lib/                     framework-free: spring, slosh, radius, velocity, geometry, motionEngine, textures, moltenShader/moltenRenderer
src/hooks/                   useDrag, useResize, useMotionTarget, useOutsideClick, useDesktopKeys, useClock
src/components/<Name>/       <Name>.tsx · <Name>.module.css · index.ts · README.md
src/desktop/                 Desktop, Wallpaper, WindowLayer, settings, menus, wm/ (store + reducer), apps/
../linux/                    the same system in C: libmaryui + maryui-desktop (wlroots); see ../linux/README.md
```

## How the metal works

| Effect | Mechanism | Where |
| --- | --- | --- |
| Brushed grain | One seamless 512px `feTurbulence` tile with diagonal strokes (lattice angle `brush.angle`, tan θ = rise/run) baked to a data URI at startup, painted with `mix-blend-mode: overlay` | `src/lib/textures.ts`, `Surface.module.css` |
| Moving scratches | The grain is an oversized element, not a pseudo-element, so it can be translated: its offset is a straight proportion of speed (`-clamp(v / brush.velocity-ref) * brush.lag`), rate-limited by `brush.settle` and never sprung or eased, while `brush.glint` lifts its opacity with speed | `motionEngine.ts`, `Surface.module.css` |
| Liquid corners | Velocity projected onto each corner's outward normal — leading corners flatten, trailing ones round — driving four springs detuned against each other so they never settle in step | `src/lib/radius.ts`, `Window.module.css` |
| Sliding sheen | A highlight band positioned by `--lp-sheen-x`; the light is fixed to the *room* (`sheen.light-x`), so moving a window slides the highlight across it, spring-lagged | `motionEngine.ts`, `Surface.module.css` |
| Jelly | Velocity → springs → `skewX` / `scale` on the window chrome, origin at the grab point | `motionEngine.ts` |
| Slosh | A damped pendulum driven by the window's acceleration **and by shear** — how far the liquid's speed lags the well's — writes `--lp-slosh` / `-x` / `-y`; beads tilt and bank their liquid with it in pure CSS. Acceleration alone leaves the liquid flat through the middle of a drag, where there is none | `src/lib/slosh.ts`, `LiquidBubble.module.css` |
| Liquid merge | Unlit blobs are blurred and thresholded into one silhouette, which is then lit by `feSpecularLighting` from the room light and shaded along its lower rim; the blobs trail and elongate with the velocity lag until their ends meet. Content stays crisp above | `GooGroup`, `SegmentedControl`, `SvgDefs` |
| Wallpaper | A domain-warped height field lit by two lights, in WebGL. Its clock is *window motion*: it registers with the engine as a target that draws when stepped but never asks for a frame of its own, so the metal flows while a window is dragged and freezes when everything settles. Resolution follows suit — below native while moving, native once settled | `lib/moltenShader.ts`, `lib/moltenRenderer.ts`, `desktop/Wallpaper.tsx` |
| Wallpaper, fallback | The older still: fractal noise lit by `feDiffuseLighting` + `feSpecularLighting`, rendered once to a canvas. Also what shows when WebGL will not start | `lib/wallpaperSvg.ts`, `desktop/Wallpaper.tsx` |

Two rules keep it smooth: **React owns layout, the engine owns `transform`** (nothing re-renders during a
drag), and **only `transform`/`opacity` animate per frame** (filters are baked or tiny). The one
exception is the four corner radii, which repaint — so they are only written once a corner has actually
moved a quarter-pixel. An idle desktop schedules no animation frames at all.

One CSS trap worth knowing before you extend this: a `var()` inside a custom property is substituted
where that property is **declared**, not where it is used. A `--lp-radius-scale` defined once on `:root`
would capture `:root`'s `--lp-radius-k` — zero — and never change again, so live expressions are spelled
out at the point of use. `src/styles/base.css` carries the note and the canonical expression.

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
- **Linux**: `../linux` is the design system as a C library plus a Wayland compositor — the whole desktop, drawn by Cairo from the same tokens. `npm run tokens` also writes `../linux/include/maryui/lp_tokens.h` and `lp_icons.h` (the `toC` and `toIconsC` emitters in `scripts/tokens-lib.mjs`); `../linux/PARITY.md` lists every file here and its C twin. MaryOS boots it.
- **Swift / other codebases**: `tokens.json` is the contract; add an emitter to `scripts/tokens-lib.mjs` (a `Color(red:green:blue:opacity:)` writer is ~20 lines; the C one is the model). The physics in `src/lib/spring.ts` and `src/lib/slosh.ts` are a dozen lines each and port directly.
- **Wallpaper**: drop `public/wallpaper/platinum.jpg` and choose View › Raster Wallpaper. The molten
  shader is `src/lib/moltenShader.ts` — plain GLSL ES 1.00 with a uniform grade, so it ports to any
  surface that can run a fragment shader. What it is *for* is written up in
  [`../docs/design-direction.md`](../docs/design-direction.md).

Apache-2.0.
