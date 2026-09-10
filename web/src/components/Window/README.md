# Window

A draggable, resizable, shadeable, zoomable frame. The React-owned **frame** holds layout; the engine-owned **chrome** holds `transform`.

## Anatomy
1. `frame` — absolute, `left/top/width/height` from the store, `z-index`, `data-focused`, `data-state`.
2. `chrome` — a `Surface flat`, four independently live corner radii, shadow `--lp-shadow-window(-focused)`; receives drag delta, jelly, and FLIP transforms.
3. `TitleBar` → `TrafficLights` + title.
4. `body` — the app's content.
5. `handle × 8` — invisible resize zones (`n ne e se s sw w nw`).

## States
`normal` · `shaded` (height collapses to the title bar) · `zoomed` (fills the desktop, square corners, remembers `prevRect`) · focused / inactive (flatter chrome, grey lights) · `[data-moving]` while being dragged · closing (scale 0.96 + fade).

## Behavior
- Drag the title bar: delta rides on the chrome transform, commits to the store on release.
- Double-click title or green light: zoom (FLIP flight). Yellow: shade. Red: close.
- Cmd/Ctrl+W close · Cmd/Ctrl+M shade · Ctrl+` cycle.

## Tokens
`--lp-radius-window` (rest) with `--lp-radius-window-min` / `--lp-radius-window-max` as the live range, `--lp-shadow-window`, `--lp-shadow-window-focused`, `--lp-size-titlebar-height`, `--lp-size-resize-grip`, `--lp-size-resize-corner`, `--lp-motion-slow`, `--lp-motion-fast`.

## Motion variables written on the frame
`--lp-sheen-x`, `--lp-tilt`, `--lp-vx`, `--lp-vx-lag`, `--lp-speed`, `--lp-slosh`, `--lp-slosh-y`, `--lp-grain-x`, `--lp-grain-y`, `--lp-radius-k`, and the four corners `--lp-r-tl` / `-tr` / `-br` / `-bl`. Children consume them through inheritance.

Every one of those is composited except the corner radii, which repaint — so the engine only writes them once a corner has moved a quarter-pixel.

Note that a control cannot pre-compute a helper from these on `:root`: a `var()` inside a custom property is substituted where the property is *declared*. See the note in `src/styles/base.css`.

## Liquid corners
Each corner runs its own spring, its frequency detuned off `motion.spring-radius` by `motion.radius-detune`, targeting `cornerTargets()` in `src/lib/radius.ts`: the velocity is projected onto each corner's outward normal, so the corners the window leads with flatten toward 8px and the ones it trails with round toward 16px. Because the springs are detuned they arrive home one after another, not together. A zoom's FLIP flight feeds the same input, so zooming deforms the corners exactly as a drag does.

The corners have their own `radius-flex.velocity-ref`, far below `motion.velocity-ref`. That one is scaled for the jelly, where full deflection should take a hard fling — but a careful drag peaks nearer 400 px/s, and against 2500 that spread the corners 1.6px of the 8 available, which is invisible. `radius-flex.gamma` curves the response so slow drags register at all.

`nudgeCorners()` kicks all four springs on grab and on release. Picking a window up and setting it down are impacts, and because the springs are detuned one kick sets four corners wobbling out of phase — visible at the two moments the eye is actually on the window. An impulse rather than a bias, so it costs none of the lead/trail range during the drag itself.

## Sketch notes
Symbol with overrides for title text, focused/inactive, and window state. The traffic lights are a nested symbol.
