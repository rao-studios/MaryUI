# Window

A draggable, resizable, shadeable, zoomable frame. The window manager owns layout; the compositor owns the
scene node's position (the drag delta rides on it, nothing repaints during a drag). Mirrors
`web/src/components/Window`.

## Anatomy
1. shadow — `shadow.window` / `shadow.window-focused` as a 9-slice sprite around the frame.
2. chrome — `Surface flat`, radius `radius.window` (0 when zoomed), `emboss-raised`.
3. TitleBar → TrafficLights + title.
4. body — the app's content (built-in apps paint here; Wayland clients sit in a scene subtree at this rect).
5. handle × 8 — invisible resize zones (`lp_window_handles`): edges `size.resize-grip` wide, corners
   `size.resize-corner`.

## States
`normal` · `shaded` (only the title bar, full radius) · `zoomed` (fills the desktop, remembers `prev_rect`) ·
focused / inactive (flatter chrome, grey lights) · closing (scale .96 + fade — with the motion milestone).

## Behavior
Drag the title bar (delta on the scene node, `MOVE` on release) · double-click the title or the green light:
zoom · yellow: shade · red: close · Super/Ctrl+W close · Super/Ctrl+M shade · Ctrl+` cycle.

## C
`body = lp_window_chrome(ctx, &view, &bar_result)`; the compositor's window.c drives it.
