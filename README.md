# Yeocam Grid for OBS Studio

**One camera, many tiles.** Yeocam Grid turns a single camera (or any video source) into a
grid of 1–64 copies: 2-cam, 3-cam, 2×2, a 3×3 wall, mirrored pairs. You change the layout
with a hotkey, a Stream Deck button or a chat event, without switching scenes.

```
 count 1          count 2               count 3                 count 9
┌──────────┐    ┌─────┐┌─────┐    ┌───┐┌───┐┌───┐      ┌──┐┌──┐┌──┐
│          │    │     ││     │    │   ││   ││   │      ├──┤├──┤├──┤
│   you    │ →  │ you ││ you │ →  │you││you││you│  →   ├──┤├──┤├──┤
│          │    │     ││     │    │   ││   ││   │      └──┘└──┘└──┘
└──────────┘    └─────┘└─────┘    └───┘└───┘└───┘
```

- **Light on your PC:** the camera is decoded once and rendered once per frame. Each extra tile
  is a single GPU draw, so 2 tiles and 64 tiles cost about the same.
- **One scene instead of five:** no more "2 Cam" / "3 Cam" scenes to keep in sync. Overlays,
  alerts and music stay put while the layout changes.
- **Remote control:** OBS hotkeys, obs-websocket (Streamer.bot, Stream Deck, scripts), plus
  ready-made layout presets and a BPM-synced "beat party" script.
- **Made for OBS 33:** installs to the new plugin location, so it won't show as "Legacy".

## Install

**Requires OBS Studio 33.0 or newer, Windows 10/11 x64.**

1. Close OBS.
2. Copy the `yeocam-grid` folder into `C:\ProgramData\obs-studio\plugins\`, so you have:

   ```
   C:\ProgramData\obs-studio\plugins\yeocam-grid\yeocam-grid.dll
   C:\ProgramData\obs-studio\plugins\yeocam-grid\data\...
   ```

3. Start OBS. **Yeocam Grid** now appears in **Sources → +**.

To uninstall, delete that folder. To build it yourself, see [Building](#building).

## Quick start

1. Hide your camera source in the scene (eye icon). Don't delete it.
2. **Sources → + → Yeocam Grid**, then pick your camera in **Source**.
3. Set **Tile count** and you're done. Everything else has sensible defaults.

To use it as your everyday camera and only go multi-cam on cue, set **Tile count 1** and
**Gap 0**: it then looks exactly like your normal camera. The [scene guide](docs/SCENE-GUIDE.md)
walks through this setup.

## Settings

| Setting | What it does | Default |
|---|---|---|
| Source | The camera or video source to tile | — |
| Tile count | 1–64 tiles | 4 |
| Columns / Rows | Fix the grid shape. 0 = automatic (near-square). Columns win if both are set | 0 / 0 |
| Gap | Space between tiles and around the edge, in pixels | 16 |
| Tile fit | **Fill (crop)** fills each tile, cropping the camera. **Fit (letterbox)** shows the whole frame | Fill |
| Crop anchor | Which part of the camera each tile keeps: 9 positions, or Custom (Focus X/Y) | Center |
| Zoom | 100–400%, crops tighter around the anchor | 100% |
| Mirror | None, all tiles, or alternate tiles | None |
| Background colour | Behind the tiles and gaps | Transparent |
| Width / Height | Size of the grid. 0 = canvas size | 0 / 0 |

Tip: in narrow layouts (3 in a row, strips), use **Crop anchor** or **Zoom** to keep your
face in each tile.

## Changing layouts live

| Method | Best for | How |
|---|---|---|
| **OBS hotkeys** | Keyboard, macro pads, Stream Deck hotkey buttons | Settings → Hotkeys → your grid: *Add tile*, *Remove tile*, *Toggle fill / fit*, *Next crop anchor* |
| **Layout presets** | A Stream Deck button per layout | `node examples/set-layout.mjs trio` (solo, duo, trio, quad, wall, strip, mirror-duo, party) |
| **Streamer.bot** | Gift / sub / raid automation | OBS Raw sub-action → `CallVendorRequest` or `SetInputSettings` ([recipe](docs/SCENE-GUIDE.md#3-streamerbot-gift--music--grid)) |
| **Beat party** | Layout changes in time with a song | `node examples/beat-party.mjs --bpm 128` |
| **Any obs-websocket client** | Your own tools | See [obs-websocket API](#obs-websocket-api) |

**Full walkthroughs are in the [scene guide](docs/SCENE-GUIDE.md):** one-scene setup, Stream
Deck, the gift → music → multi-cam flow, syncing to music, and the classic multi-scene
approach.

## obs-websocket API

OBS's built-in WebSocket server (Tools → WebSocket Server Settings) controls the grid in two
ways.

**1. Change any setting** with the standard `SetInputSettings` request. Keys: `source`,
`count`, `columns`, `rows`, `gap`, `fit` (0 fill, 1 fit), `anchor` (0–8 left-to-right,
top-to-bottom; 9 custom), `focus_x`, `focus_y` (0–100), `zoom` (100–400), `mirror`
(0 none, 1 all, 2 alternate), `background_color`, `width`, `height`.

```json
{ "inputName": "Cam grid", "inputSettings": { "count": 3, "columns": 3 }, "overlay": true }
```

**2. Vendor requests** for relative actions: `CallVendorRequest` with `vendorName: "yeocam"`.

| requestType | requestData | Returns |
|---|---|---|
| `ListGrids` | — | `grids: [{ sourceName, targetName, count }]` |
| `AdjustCount` | `sourceName?`, `delta` (default 1) | `count` |
| `SetCount` | `sourceName?`, `count` | `count` |
| `ToggleFit` | `sourceName?` | `fit` |
| `SetAnchor` | `sourceName?`, `anchor` (`top_left` … `bottom_right`, `custom`) | `anchor` |
| `NextAnchor` | `sourceName?` | `anchor` |

You can leave out `sourceName` when there's only one grid. Every response includes
`success`, and failures include an `error` message.

## Examples

All examples need Node.js 22+ and no other installs. They ask for your OBS WebSocket password,
or read it from the `OBS_WS_PASSWORD` environment variable.

| File | What it does |
|---|---|
| [`examples/set-layout.mjs`](examples/set-layout.mjs) | Applies a named preset from [`layouts.json`](examples/layouts.json) |
| [`examples/beat-party.mjs`](examples/beat-party.mjs) | Steps through tile counts on the beat, then resets (`--dry-run` to preview) |
| [`examples/obs-ws.mjs`](examples/obs-ws.mjs) | Tiny obs-websocket client to copy into your own tools |
| [`tests/ws-vendor-test.mjs`](tests/ws-vendor-test.mjs) | Checks every vendor request against a running OBS |

## Roadmap

Ideas, not built yet:

- **Built-in beat sync:** pick an OBS audio source (your music) and the grid steps through a
  sequence on detected beats. No script, no BPM to type in.
- **Layout sequences in the plugin:** define "1 → 2 → 4 → 9" in the properties, with a
  *Next layout* hotkey.
- **Animated changes:** tiles slide or scale into place instead of cutting.
- **Per-tile delay:** each tile a few frames behind the last, for an echo effect.
- **macOS and Linux builds:** the build presets exist but haven't been tested.

## Building

Windows: Visual Studio 2022 or newer (Desktop C++ workload), CMake 3.28+, Windows SDK
10.0.26100.

```bash
cmake --preset windows-x64
```

```bash
cmake --build --preset windows-x64 --config RelWithDebInfo
```

```bash
cmake --install build_x64 --config RelWithDebInfo --prefix release
```

The first configure downloads the OBS 33.0.1 sources and prebuilt dependencies and builds
libobs, which takes a few minutes. The plugin ends up in `release\yeocam-grid\`, laid out for
`C:\ProgramData\obs-studio\plugins\`. Pass `-DENABLE_BUILD_LEGACY=ON` to also produce an
old-layout copy for OBS 32 and earlier.

Layout-maths unit test (from a Visual Studio Developer prompt):

```bash
cl /nologo /I src tests\layout-test.c src\yeocam-layout.c && layout-test.exe
```

### Project layout

| Path | Contents |
|---|---|
| `src/yeocam-grid.c` | The source: settings, target tracking, hotkeys, rendering |
| `src/yeocam-layout.c` | Grid and crop maths (no graphics calls) |
| `src/yeocam-websocket.c` | obs-websocket vendor requests |
| `src/obs-websocket-api.h` | Vendored from obs-websocket (GPL-2.0) |
| `examples/`, `docs/` | Scripts and guides |

Built on [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate), with the
OBS 33 plugin layout from
[obsproject/obs-plugintemplate#172](https://github.com/obsproject/obs-plugintemplate/pull/172).

## Licence

GPL-2.0-or-later, the same as OBS Studio.
