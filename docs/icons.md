# Icons — the symbol inventory

Every mark the system draws, named once. This is the source list the icon artwork, the Sketch sheet
and the application roadmap are all built from.

It exists because the chrome and the icons disagree. Windows are brushed platinum with 1px emboss,
the traffic lights are glass beads with liquid in them, the desk is a molten shader — and the icons
are 27 monochrome hairline outlines. The glyphs never became objects. A folder today is one stroked
path with `fill="none"`: a diagram of a folder, not a folder.

Fixing that means drawing a lot of icons, and drawing a lot of icons means naming every object the
system has. That turns out to be the more valuable half. **An app icon is a commitment to an
application**, so the list below is also the roster of what MaryOS is.

## Two tiers

Mac OS shipped two kinds of mark and so does this. The tier is a *finish*, not a separate
vocabulary — one `IconName` type covers both.

|  | **Glyph** | **Object** |
|---|---|---|
| Source | [`icons.json`](../web/src/components/Icon/icons.json) | `objects.json` (web-only) |
| Grid | 24 | 32 — divides cleanly into 16/32/48/128 |
| Look | stroked 1.7px in `currentColor` | filled, lit, multi-pass |
| Shared with the C desktop | yes, via `lp_icons.h` | not yet |
| Used at | ≤19px, and anywhere a mark is a mark | ≥20px, where an object is an object |

A glossy chevron in a menu would be wrong, and a wireframe folder on a desk is wrong. That is the
whole reason for the split.

Object icons are *lit*, never painted: geometry plus a material, run through one shared recipe under
`object` in [`tokens.json`](../web/tokens/tokens.json), so the entire set regrades from one place
the way the wallpaper regrades from `molten.*`. See
[design-direction.md](design-direction.md) — "light the field, do not paint it."

**Materials:** `platinum` · `manila` · `slate` · `paper` · `graphite` · `accent` · `ruby` ·
`amber` · `verdant` · `glass`, plus `folder`.

`ruby`, `amber` and `verdant` are the three hues the system already ships in its glass traffic-light
beads, reused. A vivid application icon therefore adds no new colour to the palette.

`folder` is not a ramp of its own. Like `accent`, it has two appearances and resolves at paint time:
**Manila**, warm card stock in the System 7 lineage, and **Slate**, the cool blue-grey Mac OS 8/9
drew. Switchable from View › Folders, and deliberately *not* the `platinum` chrome ramp — a folder
tinted with the window's own metal disappears into the window behind it. **Slate is the default.**

## Two finishes

`lit` is the default and the whole of the recipe above. `glossy` lays Aqua's sweep *over* the lit
passes — bright at the crown, a hard waterline at `object.gloss-break`, light bounced back along the
bottom edge. The body underneath is still a lit material, so a glossy icon still regrades and is
still lit by the same lamp as everything else; the gloss is a finish on it, not a replacement for
it.

**`glossy` is for application icons only.** A glossy folder is a category error: the shell's own
objects are things on a desk, and applications are products on it. That line is what the finish
exists to draw.

Below ~20px every object falls back to a glyph, because four tonal layers and a 1px keyline in a
16px box turn to grey mush. Object names with no glyph of their own declare a `glyph` fallback; it
is listed in the tables as **↳**.

## Waves

- **Wave 1** — everything the running desktop renders today, plus the full application roster.
  Enough to make Spotlight and Finder read correctly.
- **Wave 2** — the long tail, drawn against a recipe already proven by Wave 1.
- **Later** — needs the C target in scope, because it grows `icons.json` and `LP_ICON_COUNT`.

---

## Object tier · Wave 1

### Repaints — the 16 existing names that are objects

Their names and glyph fallbacks are themselves, so no call site changes.

| Name | Material | The object |
|---|---|---|
| `folder` | folder | Back flap, slanted tab, front face, a crease where they meet. Manila or Slate |
| `document` | paper | A sheet with a folded top-right corner |
| `image` | paper | A photo print — white border, image field, a horizon and a sun |
| `music` | paper | A sheet with a beamed pair of notes in accent |
| `code` | paper | A sheet with angle brackets pressed into it |
| `drive` | platinum | A machined slab, front bezel, an activity pin-light |
| `cloud` | glass | A blown-glass cloud, lit from behind |
| `trash` | platinum | The wire mesh basket, empty — banded metal, open mouth |
| `terminal` | graphite | A dark screen in a metal bezel, a prompt caret glowing |
| `drop` | glass | The Liquid Platinum mark: a bead of mercury with a meniscus |
| `home` | folder | A folder with a house embossed on the front face |
| `desktop` | platinum | A display on a stand, screen dark and reflective |
| `download` | accent | An arrow descending into an open tray |
| `star` | accent | A five-point star, faceted so each arm catches the key light |
| `info` | accent | A glass disc with a letterpress *i* |
| `pencil` | graphite | Hex barrel, painted ferrule, sharpened graphite tip |

### Shell and system

| Name | Material | ↳ | The object |
|---|---|---|---|
| `appFinder` | platinum | `folder` | The house mark — the shell's own face |
| `appSpotlight` | glass | `search` | A glass lens over a lit field |
| `appControlPanels` | platinum | `gear` | A sliders panel, knurled thumbwheels |
| `appGetInfo` | paper | `info` | An info sheet with a small object inset |
| `appAbout` | glass | `info` | The Rao monogram in platinum |
| `appActivity` | graphite | `desktop` | A dark screen with a green pulse trace |
| `appDiskSetup` | platinum | `drive` | A drive slab with a screwdriver across it |
| `appFontBook` | paper | `document` | An open book, a serif *A* on the recto |
| `appGrab` | platinum | `image` | A camera body with a marquee corner |
| `appInstaller` | manila | `download` | A carton with an arrow entering the top |
| `appConsole` | graphite | `terminal` | A dark screen scrolling faint fixed-width lines |
| `appKeychain` | accent | `star` | A brass key ring, keys fanned |
| `trashFull` | platinum | `trash` | The basket with paper mounded above the rim |

### Applications

The bundled roster. Each of these is a real commitment — see [the roadmap](#what-this-implies-about-maryos).

| Name | Material | ↳ | The object |
|---|---|---|---|
| `appTextEdit` | paper | `pencil` | A sheet with ruled text and a nib resting on it |
| `appNotes` | manila | `document` | A bound pad, torn perforation at the head |
| `appScriptEditor` | paper | `code` | A scroll with a stepping arrow |
| `appCalculator` | graphite · **glossy** | `plus` | A desk calculator: lit readout, three columns of keys and an amber operator column |
| `appCalendar` | paper · **glossy** | `document` | A tear-off block, ruby header, a month of days with today picked out |
| `appClock` | platinum | `info` | An analogue face under domed glass |
| `appReminders` | paper | `check` | A checklist, one item struck through |
| `appContacts` | manila | `document` | A card index with a visible tab |
| `appMail` | paper | `document` | A sealed envelope, flap catching the key light |
| `appMessages` | glass | `document` | A speech bubble in blown glass |
| `appPreview` | glass | `image` | A loupe resting on a photograph |
| `appPhotos` | paper | `image` | A fan of prints, corners overlapping |
| `appMusic` | ruby · **glossy** | `music` | A beamed pair of notes on a ruby tile |
| `appVideo` | graphite | `image` | A film frame with sprocket perforations |
| `appVoiceMemos` | platinum | `music` | A capsule microphone on a stand |
| `appPaint` | accent | `pencil` | A palette with wet pigment and a loaded brush |
| `appDraw` | platinum | `code` | Compasses and a set square over a grid |
| `appBrowser` | glass | `cloud` | A glass globe with a meridian |
| `appTransfer` | platinum | `download` | Two arrows crossing between drives |
| `appChess` | graphite | `star` | A knight, carved and polished |
| `appPuzzle` | platinum | `grid` | The sliding 15-tile square, one cell empty |

**Wave 1: 16 repaints + 13 shell/system + 21 applications = 50 objects.**

---

## Object tier · Wave 2

### Folders

`folder` itself ships in Wave 1. All fall back to `folder`.

`folderOpen` · `folderHome` · `folderDocuments` · `folderDownloads` · `folderDesktop` ·
`folderApplications` · `folderUtilities` · `folderSystem` · `folderShared` · `folderDropBox` ·
`folderFavourites` · `folderRecent` · `folderSmart` · `folderBurn`

All on the `folder` material — so they follow the Manila / Slate preference together — except
`folderSystem` (platinum) and `folderSmart` (accent — it is a query, not a place, and should not
pretend to be one). Each carries an emboss on the front face rather than a separate
badge, so the silhouette stays one shape. **14 objects.**

### Documents

`document` itself ships in Wave 1. All fall back to `document` unless noted.

`docText` · `docRich` · `docPdf` · `docImage` ↳`image` · `docVector` · `docSheet` · `docSlides` ·
`docAudio` ↳`music` · `docVideo` · `docArchive` · `docDiskImage` ↳`drive` · `docFont` ·
`docScript` ↳`code` · `docApp` · `docGeneric` · `docAlias` · `docClipping` · `docStationery`

All paper. One sheet silhouette, one folded corner, one embossed mark — the kind is carried by the
mark, never by changing the sheet. `docStationery` is the exception: its corner is a stack, because
a template makes copies. **18 objects.**

### Volumes and devices

| Name | Material | ↳ | Note |
|---|---|---|---|
| `volumeInternal` | platinum | `drive` | |
| `volumeExternal` | platinum | `drive` | Enclosure seam and a cable stub |
| `volumeRemovable` | platinum | `drive` | A USB stick, cap off |
| `volumeFloppy` | platinum | `drive` | 3.5in, sliding shutter, write-protect tab |
| `volumeOptical` | glass | `drive` | Iridescent disc — the one place a rainbow is correct |
| `volumeDiskImage` | glass | `drive` | A drive rendered in glass: mounted, not real |
| `volumeNetwork` | platinum | `cloud` | A drive with a globe embossed |
| `volumeServer` | graphite | `drive` | A rack column, ventilation slots, pin-lights |
| `devicePrinter` | platinum | `document` | A sheet emerging from the slot |
| `deviceScanner` | platinum | `image` | Lit glass bed under a lid |
| `deviceDisplay` | platinum | `desktop` | |
| `deviceKeyboard` | platinum | `grid` | |
| `deviceMouse` | platinum | `drop` | One-button, as it should be |
| `deviceCamera` | graphite | `image` | |
| `devicePhone` | graphite | `desktop` | |
| `deviceMediaPlayer` | platinum | `music` | Click wheel |

**16 objects.**

### Network

`cloud` ships in Wave 1. `netGlobe` ↳`cloud` · `netAirport` ↳`cloud` · `netEthernet` ↳`drive` ·
`netBluetooth` ↳`cloud` · `netSharedComputer` ↳`desktop` · `netRemoteDesktop` ↳`desktop`

**6 objects.**

**Wave 2: 14 + 18 + 16 + 6 = 54 objects. Total object tier ≈ 104.**

---

## Glyph tier

### Shipping today (11, unchanged)

`chevronLeft` · `chevronRight` · `chevronDown` · `search` · `grid` · `list` · `gear` · `check` ·
`close` · `plus` · `minus`

### Later — needs the C target in scope

These are drawn now and live in the Sketch sheet, but adding them to `icons.json` grows
`LP_ICON_COUNT` and trips the count assertion in `linux/tests/test_icons.c`, so they land as one
batch when linux is in scope.

| Group | Marks |
|---|---|
| **Navigation** | `chevronUp` · `back` · `forward` · `enclosingFolder` · `reload` · `eject` |
| **Disclosure** | `triangleClosed` · `triangleOpen` — filled triangles, as Platinum drew them, not chevrons |
| **State** | `radioDot` · `mixed` · `lock` · `unlock` · `pin` · `tag` · `unread` |
| **Sort and view** | `sortAsc` · `sortDesc` · `viewColumn` · `viewGallery` · `sidebarToggle` · `filter` |
| **Alerts** | `warning` · `error` · `stop` · `question` — the Platinum alert set |
| **Transport** | `play` · `pause` · `stopPlayback` · `next` · `previous` · `volume` · `mute` |
| **Text ruler** | `bold` · `italic` · `underline` · `alignLeft` · `alignCenter` · `alignRight` · `alignJustify` · `listBullet` · `listNumber` |
| **Menu bar** | `wifi` · `battery` · `bluetooth` · `clock` · `user` · `inputSource` |
| **Chrome** | `ellipsis` · `expand` · `collapse` · `zoomIn` · `zoomOut` · `share` · `grip` |

**54 glyphs.**

---

## Badges

Composited onto an object rather than replacing it, so one object can carry state without a second
drawing. Bottom-left at ⅓ scale, with their own contact shadow so they read as sitting on top.

`badgeAlias` · `badgeLocked` · `badgeShared` · `badgeSyncing` · `badgeProgress` · `badgeCount` ·
`badgeAction` — **7.**

## Cursors

A genuinely separate tier: each has a hotspot, each must survive over arbitrary content, and each
must read at exactly one size. They do not go through the object recipe — recorded here so they are
not forgotten when the compositor needs them.

`cursorArrow` · `cursorIBeam` · `cursorCrosshair` · `cursorResizeNS` · `cursorResizeEW` ·
`cursorResizeNESW` · `cursorResizeNWSE` · `cursorOpenHand` · `cursorPointingHand` · `cursorWatch` ·
`cursorCopy` · `cursorAlias` · `cursorNotAllowed` · `cursorTextSelect` — **14.**

`cursorWatch` is the spinning wristwatch, not a beachball. The era is Platinum.

---

## Partner guidance

`appMusic`, `appCalendar` and `appCalculator` are drawn ahead of the rest on purpose. They are the
three exemplars a commercial partner is pointed at: the shapes an application icon in this system
should take.

What they demonstrate, and what a partner should copy:

- **An application is a tile; a system object is a thing.** Applications take the squircle and the
  glossy finish. Folders, documents and drives keep their own silhouette and stay `lit`. Reading a
  dock, you can tell an app from a file without reading a label.
- **Colour comes from the system's own hues.** Ruby, amber and verdant are the traffic-light beads,
  reused. A partner does not bring a palette; they pick one.
- **The mark is knocked out of the tile, not laid on it.** One shape, high contrast, no outline.
- **It still has to survive 16px.** All three collapse to their glyph below 20px, which is why every
  one of them declares a fallback.

They are icons and a specification, not running applications — nothing in `registry.ts` opens them.

## What this implies about MaryOS

Twenty-eight applications, five of which exist in some form. Nothing here commits to *building*
them — it commits to knowing what they are, so the Apple menu, the Applications folder and the
Spotlight index have a shape to grow into instead of being invented one at a time.

| Category | Applications | Today |
|---|---|---|
| **Shell** | Finder · Spotlight · Trash | Finder ✓ · Spotlight ✓ |
| **System** | Control Panels · Get Info · Activity Monitor · Disk Setup · Console · Keychain · Installer · Font Book · Grab | About ✓ |
| **Text and documents** | TextEdit · Notes · Script Editor | TextEdit ✓ |
| **Numbers and time** | Calculator · Calendar · Clock · Reminders | — |
| **People and comms** | Contacts · Mail · Messages | — |
| **Media** | Preview · Photos · Music · Video · Voice Memos | Gallery ✓ → Photos |
| **Creative** | Paint · Draw | — |
| **Internet** | Browser · File Transfer | — |
| **Development** | Terminal · Script Editor | Terminal ✓, as a Spotlight command rather than an app |
| **Diversions** | Chess · Puzzle | — |

Two notes on what that list is telling us:

**Terminal is not yet an app.** It is a synthesised Spotlight command in
[`spotlight.ts`](../web/src/desktop/spotlight.ts) with nothing behind it. Giving it an icon is
giving it a window.

**Gallery is two things wearing one name.** Today it is the design-system browser; the roster wants
it to be Photos. The component gallery is a developer tool and probably belongs under a different
name entirely.

## Counts

| Tier | Wave 1 | Wave 2 | Later | Total |
|---|---|---|---|---|
| Objects | 50 | 54 | — | 104 |
| Glyphs | 11 (existing) | — | 54 | 65 |
| Badges | — | — | 7 | 7 |
| Cursors | — | — | 14 | 14 |
| | | | | **190** |

Counts are the drawings on the canvas, not an estimate. `volume` belongs to Transport and is not
repeated under Menu bar; the transport square is `stopPlayback`, since `stop` is the alert octagon.
