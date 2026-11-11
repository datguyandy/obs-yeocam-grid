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
- **Beat sync:** point it at your music and it steps through layouts (2 → 3 in a row → 2×2 →
  3×3 …) on every kick, then goes back to your normal cam when the music stops. No scripts.
- **Remote control:** OBS hotkeys, obs-websocket (Streamer.bot, Stream Deck, scripts), plus
  ready-made layout presets.
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

### Layout sequence

| Setting | What it does | Default |
|---|---|---|
| Sequence | Layouts that *Next layout* and beat sync step through. A number is a tile count; `CxR` is columns × rows, so `3x1` is three in a row | `2x1, 3x1, 2x2, 3x3` |
| Flip mirroring on every other step | Adds a mirror flip to each change for extra movement | Off |

**Tile count** is the *resting* layout: where the grid sits when no sequence step is active.
Set it to 1 to look like your normal camera between moments.

### Beat sync

Tick **Beat sync** and pick a **Music source**: a Media Source with your song, Desktop Audio,
or an Application Audio Capture of Spotify. The grid then moves one step through the
sequence on each kick drum.

| Setting | What it does | Default |
|---|---|---|
| Music source | Audio to listen to for beats | — |
| Sensitivity | Higher catches softer kicks; lower only reacts to strong ones | 60 |
| Beats per step | 1 = change every beat, 4 = once per bar (4/4) | 1 |
| Minimum time between beats | Stops busy drums from flickering | 250 ms |
| Back to resting layout after silence | When the music has been quiet this long, go back to Tile count. 0 = stay on the last step | 2000 ms |

The detector listens to the bass (below ~150 Hz) for sudden jumps, which is where kicks live.
It's cheap: a few multiply-adds per audio sample, no FFT. It works best on music with a clear
kick (pop, hip-hop, EDM, K-pop). Songs without drums won't trigger much; use
**Next layout** or the beat-party script for those.

## Changing layouts live

| Method | Best for | How |
|---|---|---|
| **Beat sync** | Hands-free: layouts follow the music | Grid properties → Beat sync → pick the music source |
| **OBS hotkeys** | Keyboard, macro pads, Stream Deck hotkey buttons | Settings → Hotkeys → your grid: *Next layout in sequence*, *Back to resting layout*, *Toggle beat sync*, *Add tile*, *Remove tile*, *Toggle fill / fit*, *Next crop anchor* |
| **Layout presets** | A Stream Deck button per layout | `node examples/set-layout.mjs trio` (solo, duo, trio, quad, wall, strip, mirror-duo, party) |
| **Streamer.bot** | Gift / sub / raid automation | OBS Raw sub-action → `CallVendorRequest` or `SetInputSettings` ([recipe](docs/SCENE-GUIDE.md#3-streamerbot-gift--music--grid)) |
| **Beat party script** | Fixed-tempo sync when there's no kick to detect | `node examples/beat-party.mjs --bpm 128` |
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
(0 none, 1 all, 2 alternate), `background_color`, `width`, `height`, `sequence` (text),
`sequence_flip`, `beat_sync`, `audio_source`, `beat_sensitivity` (0–100), `beats_per_step`,
`beat_min_interval` (ms), `silence_reset` (ms).

```json
{ "inputName": "Cam grid", "inputSettings": { "count": 3, "columns": 3 }, "overlay": true }
```

**2. Vendor requests** for relative actions: `CallVendorRequest` with `vendorName: "yeocam"`.

| requestType | requestData | Returns |
|---|---|---|
| `ListGrids` | — | `grids: [{ sourceName, targetName, count, liveCount, sequenceIndex, beatSync }]` |
| `AdjustCount` | `sourceName?`, `delta` (default 1) | `count` |
| `SetCount` | `sourceName?`, `count` | `count` |
| `ToggleFit` | `sourceName?` | `fit` |
| `SetAnchor` | `sourceName?`, `anchor` (`top_left` … `bottom_right`, `custom`) | `anchor` |
| `NextAnchor` | `sourceName?` | `anchor` |
| `NextLayout` | `sourceName?` | — (steps the sequence) |
| `ResetLayout` | `sourceName?` | — (back to Tile count) |
| `SetBeatSync` | `sourceName?`, `enabled` | `beatSync` |
| `ToggleBeatSync` | `sourceName?` | `beatSync` |

`liveCount` is the number of tiles on screen right now, and `sequenceIndex` is the current
step (`-1` when resting on Tile count).

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

- **Animated changes:** tiles slide or scale into place instead of cutting.
- **Beat effects:** a short zoom "punch" or flash on each kick, as well as layout steps.
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

Unit tests for the layout maths and the beat detector (from a Visual Studio Developer prompt):

```bash
cl /nologo /I src tests\layout-test.c src\yeocam-layout.c && layout-test.exe
```

```bash
cl /nologo /O2 /I src tests\beat-test.c src\yeocam-beat.c && beat-test.exe
```

`tests/beat-test.c` feeds synthetic music through the detector: kicks at 120 and 150 BPM,
quiet and busy mixes, a kick over a sub-bass line, plus pads, hi-hats, noise and silence
that must not trigger. `tests/make-beat-track.mjs` writes a 120 BPM WAV with a silent tail
for checking beat sync inside OBS.

### Project layout

| Path | Contents |
|---|---|
| `src/yeocam-grid.c` | The source: settings, target tracking, hotkeys, rendering |
| `src/yeocam-layout.c` | Grid and crop maths (no graphics calls) |
| `src/yeocam-beat.c` | Kick-drum beat detector (no OBS calls) |
| `src/yeocam-websocket.c` | obs-websocket vendor requests |
| `src/obs-websocket-api.h` | Vendored from obs-websocket (GPL-2.0) |
| `examples/`, `docs/` | Scripts and guides |

Built on [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate), with the
OBS 33 plugin layout from
[obsproject/obs-plugintemplate#172](https://github.com/obsproject/obs-plugintemplate/pull/172).

## Licence

GPL-2.0-or-later, the same as OBS Studio.
