# Yeocam Grid for OBS Studio

Tile one camera (or any video source) into a grid of 1–64 copies.

The camera is opened and decoded **once**. Each frame the plugin renders it into a
GPU texture a single time, then draws that texture once per tile, so 2 tiles and
40 tiles cost about the same.

Built from [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)
with the OBS 33 plugin layout from
[obsproject/obs-plugintemplate#172](https://github.com/obsproject/obs-plugintemplate/pull/172),
targeting **OBS Studio 33.0.1**.

## Using it

1. Add your camera to a scene as usual (for example "Facecam"), then hide it with the eye icon.
2. Add **Sources → + → Yeocam Grid** and pick the camera in **Source**.
3. Set the tile count, columns or rows, gap, fit and crop anchor.

The hidden camera keeps running because the grid marks it as active.

| Setting | What it does |
|---|---|
| Tile count | 1–64 tiles |
| Columns / Rows | 0 = automatic (near-square). Columns win if both are set |
| Gap | Space between tiles and around the edge, in pixels |
| Tile fit | **Fill (crop)** is the default. **Fit (letterbox)** shows the whole frame |
| Crop anchor | Which part of the camera each tile keeps: 9 presets or Custom (Focus X/Y) |
| Zoom | 100–400%, crops tighter around the anchor |
| Mirror | None, all tiles, or alternate tiles |
| Background colour | Behind the tiles; transparent by default |
| Width / Height | Output size; 0 = canvas size |

## Remote control and Stream Deck

**Hotkeys** (Settings → Hotkeys, listed under the grid source's name): Add tile,
Remove tile, Toggle fill / fit, Next crop anchor. A Stream Deck "Hotkey" button can
press these.

**obs-websocket**, built into OBS:

- Any setting: the standard `SetInputSettings` request, using the keys
  `source`, `count`, `columns`, `rows`, `gap`, `fit` (0 fill, 1 fit), `anchor` (0–8, 9 custom),
  `focus_x`, `focus_y` (0–100), `zoom` (100–400), `mirror` (0–2), `background_color`, `width`, `height`.
- Vendor requests via `CallVendorRequest` with `vendorName: "yeocam"`:

| requestType | requestData | Returns |
|---|---|---|
| `ListGrids` | – | `grids: [{ sourceName, targetName, count }]` |
| `AdjustCount` | `sourceName?`, `delta` (default 1) | `count` |
| `SetCount` | `sourceName?`, `count` | `count` |
| `ToggleFit` | `sourceName?` | `fit` |
| `SetAnchor` | `sourceName?`, `anchor` (`top_left` … `bottom_right`, `custom`) | `anchor` |
| `NextAnchor` | `sourceName?` | `anchor` |

`sourceName` can be left out when the scene collection has exactly one grid.

Example:

```json
{
  "op": 6,
  "d": {
    "requestType": "CallVendorRequest",
    "requestId": "1",
    "requestData": {
      "vendorName": "yeocam",
      "requestType": "AdjustCount",
      "requestData": { "delta": 1 }
    }
  }
}
```

## Building (Windows)

Needs Visual Studio 2022 or newer (Desktop C++ workload) and CMake 3.28+.

```bash
cmake --preset windows-x64
```

```bash
cmake --build --preset windows-x64 --config RelWithDebInfo
```

The first configure downloads the OBS 33.0.1 sources and prebuilt dependencies
and builds libobs, which takes a few minutes.

macOS (`--preset macos`, Xcode, macOS 13+) and Ubuntu (`--preset ubuntu-x86_64`)
presets come from the template but have not been tested yet.

## Installing (OBS 33+ layout)

```bash
cmake --install build_x64 --config RelWithDebInfo --prefix release
```

Copy `release\yeocam-grid` to:

```
C:\ProgramData\obs-studio\plugins\yeocam-grid\yeocam-grid.dll
C:\ProgramData\obs-studio\plugins\yeocam-grid\data\...
```

This is the new layout OBS 33 expects. The old `Program Files\obs-studio\obs-plugins\64bit`
location is "Legacy" and stops loading in OBS 34. Pass `-DENABLE_BUILD_LEGACY=ON`
at configure time to also produce a legacy-layout copy for OBS 32 and earlier.

## Code map

- `src/yeocam-grid.c`: the source: settings, target tracking, hotkeys, rendering
- `src/yeocam-layout.c`: grid and crop maths (no graphics calls)
- `src/yeocam-websocket.c`: obs-websocket vendor requests
- `src/obs-websocket-api.h`: vendored from obs-websocket (GPL-2.0)

## Licence

GPL-2.0-or-later, the same as OBS Studio.
