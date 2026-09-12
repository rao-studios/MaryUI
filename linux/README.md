# MaryUI for Linux — Liquid Platinum in C

The design system in `../web` (React, the reference) rebuilt as a C library, `libmaryui`,
plus `maryui-desktop`, a Wayland compositor (wlroots 0.17) that *is* the desktop: the
wallpaper, an ambient clock, window frames with liquid traffic lights, menus, Spotlight (the
search bar that is also the dock *and* the app's commands, `Ctrl+Space`) and the built-in apps
are drawn by the library with Cairo and Pango; other Wayland programs open as Liquid Platinum
windows with server-side decorations. There is no menu bar: it folded into Spotlight, the way
the dock did before it.

`../web/tokens/tokens.json` is the contract. `npm run tokens` in `../web` regenerates
`include/maryui/lp_tokens.h`, `lp_icons.h` and `lp_objects.h`; the physics
(`src/core/lp_spring.c`, `lp_slosh.c`, `lp_velocity.c`, `lp_motion.c`), the geometry and
the window manager (`src/core/lp_geometry.c`, `lp_wm.c`) are line-for-line ports of
`../web/src/lib` and `../web/src/desktop/wm`, and their tests carry the same case names
as the vitest suites so `make test` can be read next to `npm test`. `PARITY.md` tracks
every web file, its C counterpart, and every deliberate deviation.

```sh
make check-deps        # cairo pangocairo fontconfig pixman-1 xkbcommon, wlroots wayland-server wayland-protocols, egl glesv2
make                   # build/libmaryui.a (+ .so), build/lp-render, build/lp-input, build/maryui-desktop
make test              # tests/test_*.c (140 cases)
make parity            # tools/parity-check.sh: every web component and ported file has its C twin
make gen-check         # the generated headers match tokens.json, icons.json and objects.json
make install DESTDIR=/tmp/root PREFIX=/usr
build/maryui-desktop   # from a tty with a logind/seatd seat, or nested inside another Wayland/X11 session
```

Ubuntu 24.04 build dependencies: `build-essential pkg-config libwlroots-dev libwayland-dev
wayland-protocols libxkbcommon-dev libpixman-1-dev libcairo2-dev libpango1.0-dev
libfontconfig1-dev libseat-dev libdrm-dev`, plus `libegl-dev libgles2-mesa-dev` for the molten
wallpaper. Runtime: `libwlroots12t64 libseat1 libcairo2 libpango-1.0-0 libpangocairo-1.0-0
libegl1 libegl-mesa0 libgles2 libgl1-mesa-dri fonts-inter fonts-urw-base35
fonts-jetbrains-mono`, and `polkitd` when the session comes from logind.

EGL is optional at build time: without it `lp_molten.c` compiles to stubs, `make` says so, and the
wallpaper falls back to the procedural filter chain. That is how this tree builds on macOS, where
`lp-render` and the library are the only targets anyway.

## Using it in your own distro

`make install` puts `maryui-desktop`, `lp-render` and `lp-input` in `bin/`, the library and
`maryui.pc` in `lib/`, and the headers in `include/maryui/`. Start `maryui-desktop` from a
session on a seat: a systemd unit with a PAM service name, `TTYPath=/dev/tty1` and
`Conflicts=getty@tty1.service`, or a greeter. It reads `/etc/os-release` for the distro's
name, `$XDG_CONFIG_HOME/maryui/settings.conf` for appearance settings (accent, folder
appearance, liquid merge, wallpaper, reduced motion, and `clock=on|off` for the ambient clock;
Spotlight's View pill writes it), and `MARYUI_DATA_DIR` for prerendered
assets (`wallpaper-WxH.png`). MaryOS (`github.com/rao-studios/MaryPi`, `linux/`) is the
reference integration: its builder compiles this directory, its image boots it, and
`linux/docs/09-the-desktop.md` there walks through the wiring.

Environment: `WLR_RENDERER=pixman` (software; the reference integration sets it on
virtio-gpu), `MARYUI_DEBUG=1` (verbose wlroots log), `MARYUI_DEBUG=frames` (logs `motion:
wake` / `motion: idle` and a frame-time histogram every five seconds while frames run).
`Ctrl+Alt+Backspace` ends the session.

## How the desktop draws

Everything the library paints is a *chrome*: a Cairo ARGB32 buffer shown as a
`wlr_scene_buffer`. A chrome's paint function runs twice — an EVENT pass on input, with no
Cairo, that hit-tests and mutates state, and a DRAW pass when something is dirty, clipped
to the damage — so a hover repaints one button and a moving sheen repaints one title bar.
Windows are scene subtrees (frame chrome plus, for clients, the surface tree at the body);
the motion engine's per-window target turns pointer velocity into the sheen position, the
tilt, the jelly scale and the slosh, and the compositor applies them as node position,
buffer scale and title-bar damage — the web writes a `transform` and `--lp-*` variables to
the same numbers. An idle desktop schedules no frames (an ambient animation such as the
indeterminate progress bar repaints its own rectangle at 30 Hz while it is on screen); motion
frames are paced to the output's refresh rate because a virtual GPU flips at once, and
`MARYUI_DEBUG=frames` reports where each frame's time went. Clients keep their pixels: a flight
scales the frame and clips the client, the jelly leaves clients alone (see `PARITY.md`).

## Layout

```
include/maryui/     public headers (lp_*.h); lp_tokens.h, lp_icons.h and lp_objects.h are generated
src/core/           geometry, spring, slosh, velocity, motion (engine + window motion), wm, settings, desktop, spotlight, files (the filesystem model), drag
src/draw/           noise (feTurbulence), blur, brush tile, wallpaper, Cairo primitives, Pango text (+ wrapped layouts), icons (stroked glyphs and the lit object tier), shadows, bubbles
src/ui/             the immediate-mode context and layout helpers
src/components/     one directory per web component, README.md mirrors the web one
src/apps/           About, Finder (a real file manager; finder_mock.c for previews), Gallery, TextEdit (Spotlight-only), Info (internal)
src/compositor/     maryui-desktop (wlroots): server, output (frames), input (+ key repeat for chromes), cursor, chrome, window, xdg, desktop, spotlight, drag (the ghost + grab), files (inotify)
tools/lp-render.c   headless PNG renders for parity checks (--all DIR renders everything; --spotlight [QUERY], --spotlight-menu NAME, --clock W, --textedit)
tools/lp-input.c    a uinput pointer + keyboard for scripted tests inside a VM
tests/              C tests; case names mirror the vitest suites
PARITY.md           web file → C file, status, deviations, constants not yet in tokens.json
LICENSE NOTICE      GPL-3.0-or-later, and the attribution the wallpaper's shader still needs
```

## Licence

GPL-3.0-or-later, like the web: `LICENSE` here, `../web/LICENSE` there, and both are installed to
`$PREFIX/share/maryui/` with the library.

One caveat is recorded in `NOTICE`: the molten wallpaper's GLSL is derived from a shader by
Mårten Rånge published on Shadertoy, whose terms have not been established. Shadertoy's default
is CC BY-NC-SA 3.0, which the GPL cannot absorb, so that needs settling before the shader
ships under this licence.

## Performance notes

The library is drawing software: it spends its time in per-pixel loops, so it is built `-O3` and
the expensive things are cached rather than recomputed.

Three caches carry most of it, and all three are keyed on values that must stay *quantised* or they
degrade into nothing:

- **Shadow sprites** (`lp_shadow.c`) — a sprite is a blur at the layer's sigma, tens of milliseconds
  for a window shadow. The key rounds the corner radius, because it used to be fed a live spring
  value and missed on every frame of every drag. Never pass an unrounded animated float here.
- **Pango layouts** (`lp_text.c`) — shaping costs more than drawing, and the desktop draws the same
  strings every frame. Layouts are handed out *borrowed*; the cache owns them, so callers must not
  unref.
- **The brushed tile** (`lp_texture.c`) — rendered once and shared.

Two structural rules matter as much as the caches. Chrome repaints are clipped to the damage
region, and a subtree that falls outside it should be skipped *before* it draws — use
`lp_clip_intersects`, not `cairo_clip_extents`, whose bounding box says "yes" for a rect lying in
the gap between two damage strips. And `mui_chrome_repaint` restores a buffer from the one on
screen only where that buffer is actually behind (`chrome->stale`), rather than copying the whole
surface every frame.

`MARYUI_DEBUG=frames` prints a frame-time histogram and a breakdown every five seconds.
