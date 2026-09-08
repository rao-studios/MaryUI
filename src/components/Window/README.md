# Window

A draggable, resizable, shadeable, zoomable frame. The React-owned **frame** holds layout; the engine-owned **chrome** holds `transform`.

## Anatomy
1. `frame` — absolute, `left/top/width/height` from the store, `z-index`, `data-focused`, `data-state`.
2. `chrome` — a `Surface flat`, radius `--lp-radius-window`, shadow `--lp-shadow-window(-focused)`; receives drag delta, jelly, and FLIP transforms.
3. `TitleBar` → `TrafficLights` + title.
4. `body` — the app's content.
5. `handle × 8` — invisible resize zones (`n ne e se s sw w nw`).

## States
`normal` · `shaded` (height collapses to the title bar) · `zoomed` (fills the desktop, remembers `prevRect`) · focused / inactive (flatter chrome, grey lights) · closing (scale 0.96 + fade).

## Behavior
- Drag the title bar: delta rides on the chrome transform, commits to the store on release.
- Double-click title or green light: zoom (FLIP flight). Yellow: shade. Red: close.
- Cmd/Ctrl+W close · Cmd/Ctrl+M shade · Ctrl+` cycle.

## Tokens
`--lp-radius-window`, `--lp-shadow-window`, `--lp-shadow-window-focused`, `--lp-size-titlebar-height`, `--lp-size-resize-grip`, `--lp-size-resize-corner`, `--lp-motion-slow`, `--lp-motion-fast`.

## Motion variables written on the frame
`--lp-sheen-x`, `--lp-tilt`, `--lp-vx`, `--lp-slosh`, `--lp-slosh-y`. Children consume them through inheritance.

## Sketch notes
Symbol with overrides for title text, focused/inactive, and window state. The traffic lights are a nested symbol.
