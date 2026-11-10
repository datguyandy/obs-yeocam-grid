# Scene guide

How to set Yeocam Grid up for a stream, from the simplest setup to fully automated
"gift → music → grid" moments.

- [The idea: one source, many layouts](#the-idea-one-source-many-layouts)
- [Recommended setup: one scene, change the count](#recommended-setup-one-scene-change-the-count)
- [Layout presets](#layout-presets)
- [Ways to switch layouts](#ways-to-switch-layouts)
  - [OBS hotkeys](#1-obs-hotkeys)
  - [Stream Deck](#2-stream-deck)
  - [Streamer.bot: gift → music → grid](#3-streamerbot-gift--music--grid)
  - [Your own scripts](#4-your-own-scripts)
- [Syncing to music](#syncing-to-music)
- [The classic way: separate 2-cam / 3-cam scenes](#the-classic-way-separate-2-cam--3-cam-scenes)
- [Tips and gotchas](#tips-and-gotchas)

## The idea: one source, many layouts

The usual way to do the 2-cam / 3-cam effect is a separate scene per layout, each with the
camera added several times and positioned by hand. That means:

- several scenes to keep in sync (overlays, alerts, chat box in each one),
- a scene switch, and often a transition flash, every time the layout changes,
- more work for OBS if each copy has its own filters.

With Yeocam Grid, the layout is just a number on one source. The camera is decoded once,
filters on it run once, and going from 1 to 2 to 9 tiles is a settings change, not a scene
change. Everything else in the scene (overlays, alerts, music) stays exactly where it is.

## Recommended setup: one scene, change the count

1. **Keep your camera source, but hide it.** In the scene where your camera lives, click the
   eye icon next to it. The grid keeps the camera running while the grid is visible.
2. **Add the grid.** Sources → **+** → **Yeocam Grid** → name it (for example "Cam grid") →
   pick your camera in **Source**.
3. **Make it look like your normal camera at count 1:**
   - Tile count `1`, Gap `0`, Tile fit `Fill (crop)`, Crop anchor `Center`.
   - Width and Height `0` (full canvas) for a full-screen cam. For a corner cam, set the
     grid's Width/Height to the size the cam should be (for example `640` × `360`) and place
     it like any other source.
4. **Use the same grid in every scene.** Right-click the grid → **Copy**, then in each other
   scene right-click → **Paste (Reference)**. All scenes now share one grid, so a layout change
   shows up wherever you are.
5. **Pick how you'll trigger layouts** (next sections).

At count 1 the grid looks exactly like the plain camera. When something happens, bump the
count, then drop it back to 1.

> Gap is applied around the outside too. Keep it `0` for the "normal cam" look, and use a
> preset that sets a gap when you go multi-cam (the presets below do this).

## Layout presets

These are ready-made settings. Use them from hotkey-free tools
(`examples/set-layout.mjs`), Streamer.bot, or any obs-websocket client with
`SetInputSettings`. They live in [`examples/layouts.json`](../examples/layouts.json); edit or
add your own.

| Preset | Looks like | Settings |
|---|---|---|
| `solo` | Normal camera | `count 1, gap 0` |
| `duo` | 2 side by side | `count 2, columns 2, gap 16` |
| `trio` | 3 in a row | `count 3, columns 3, gap 16` |
| `quad` | 2 × 2 | `count 4, columns 2, gap 16` |
| `wall` | 3 × 3 | `count 9, columns 3, gap 8` |
| `strip` | 5 tall slices in one row | `count 5, rows 1, gap 8` |
| `mirror-duo` | 2 facing each other | `count 2, columns 2, gap 0, mirror alternate` |
| `party` | 4 × 4, alternate mirrored | `count 16, columns 4, gap 4, mirror alternate` |

Because tiles default to **Fill**, narrow layouts like `trio` and `strip` crop the sides of
the camera. Set **Crop anchor** so your face stays in frame (for example `Left` if you sit
on the left of your camera's view), or use **Zoom** to tighten on your face.

## Ways to switch layouts

### 1. OBS hotkeys

**Settings → Hotkeys**, find your grid's name, and bind:

| Hotkey | Does |
|---|---|
| Add tile | count + 1 |
| Remove tile | count − 1 |
| Toggle fill / fit | switches tile fit |
| Next crop anchor | cycles the 9 anchors |

Simple and needs nothing else. Any keyboard, macro pad or Stream Deck "Hotkey" button can
press these.

### 2. Stream Deck

Elgato's built-in OBS actions cover scenes and sources, not plugin-specific requests, so use
one of these:

- **Hotkey buttons:** bind OBS hotkeys (above) to key combos, then use Stream Deck's
  **System → Hotkey** action to press them. Good for +1 / −1 buttons.
- **One button per layout:** Stream Deck's **System → Open** action can run a command such as

  ```bash
  node C:\path\to\YeocamOBSPlugin\examples\set-layout.mjs trio
  ```

  Set your OBS WebSocket password once as a user environment variable named
  `OBS_WS_PASSWORD` so the script doesn't need to ask. It's stored in plain text in your user
  environment, so only do this on your own PC.
- **Through Streamer.bot:** Streamer.bot's Stream Deck integration can run any Streamer.bot
  action, including the ones in the next section. This is the most flexible option.

### 3. Streamer.bot: gift → music → grid

The common moment: someone gifts, music plays, the cam goes multi-cam, then back to normal.
In Streamer.bot this is one action:

**Trigger:** your platform's gift / gift-bomb / sub event.

**Sub-actions:**

1. **OBS → OBS Raw**, which sends any obs-websocket request: a request type plus JSON
   request data. Request type `CallVendorRequest`, data:

   ```json
   {
     "vendorName": "yeocam",
     "requestType": "SetCount",
     "requestData": { "count": 3 }
   }
   ```

   or switch to a full preset with request type `SetInputSettings`:

   ```json
   {
     "inputName": "Cam grid",
     "inputSettings": { "count": 9, "columns": 3, "gap": 8, "mirror": 2 },
     "overlay": true
   }
   ```

2. **Start the music:** for example restart a Media Source that holds your gift song, or use
   Streamer.bot's **Play Sound**.
3. **Delay** for the length of the clip, for example `15000` ms.
4. **OBS Raw** again, request type `SetInputSettings`, to go back to `solo`:

   ```json
   {
     "inputName": "Cam grid",
     "inputSettings": { "count": 1, "gap": 0, "mirror": 0 },
     "overlay": true
   }
   ```

Scale it with the gift size: use Streamer.bot's gift-count variable to pick the count, so a
5-gift bomb gives 5 tiles and a 10-gift bomb gives 9 or 16.

For layouts that change **in time with the song**, replace steps 1–4 with a
**Core → Run a Program** sub-action that starts the beat script (next section) and plays the
song at the same time.

### 4. Your own scripts

Every grid setting is an ordinary source setting, so any obs-websocket client can drive it:

- **Core request:** `SetInputSettings` with any of the keys `count`, `columns`, `rows`, `gap`,
  `fit`, `anchor`, `focus_x`, `focus_y`, `zoom`, `mirror`, `background_color`, `width`,
  `height`. Use `"overlay": true` so you only change what you pass.
- **Vendor requests** (`CallVendorRequest`, `vendorName: "yeocam"`): `ListGrids`,
  `AdjustCount`, `SetCount`, `ToggleFit`, `SetAnchor`, `NextAnchor`. See the README for
  parameters.

[`examples/obs-ws.mjs`](../examples/obs-ws.mjs) is a small, dependency-free Node client you
can copy.

## Syncing to music

[`examples/beat-party.mjs`](../examples/beat-party.mjs) steps the grid through tile counts on
the beat:

```bash
node examples/beat-party.mjs --bpm 128
```

```bash
node examples/beat-party.mjs --bpm 140 --every 2 --sequence 1,2,4,9,16 --loops 4 --flip
```

- `--bpm` is the song's tempo; `--every 4` changes once per bar, `--every 1` on every beat.
- `--sequence` is the list of tile counts to step through; `--loops` how many times.
- `--flip` toggles mirroring on each step for extra movement.
- `--delay <ms>` waits before the first change, to land on the drop.
- When it finishes (or you press Ctrl+C) it goes back to `--end` (default 1).
- `--dry-run` prints the timing without touching OBS.

Timing is fixed-tempo, not beat detection: it stays locked to the BPM for the whole song, but
you need to start it on the beat. Starting the song and the script from the same Streamer.bot
action is the easiest way to keep them lined up. Built-in audio-reactive beat detection is
on the roadmap (see the README).

## The classic way: separate 2-cam / 3-cam scenes

If you'd rather keep separate scenes (for example different overlays per layout), you still
gain from the plugin:

1. Make a scene per layout ("2 Cam", "3 Cam", "Wall").
2. In each, add a **new** Yeocam Grid (not a reference, since settings are per source) pointing
   at the same camera, with that scene's count and gap.
3. Switch scenes as you do today: Streamer.bot, Stream Deck scene buttons, or hotkeys.

The camera is still opened and decoded once, however many grids use it.

## Tips and gotchas

- **Don't delete the original camera source.** Hide it. The grid needs it to exist; if it's
  removed and re-added with the same name, the grid finds it again within a second.
- **Renaming the camera is fine.** The grid follows the rename.
- **Filters:** put colour correction, LUTs, background removal etc. on the **camera**, not the
  grid. They then run once and every tile gets them. Filters on the grid itself (for example
  a scroll or shake) apply to the whole grid.
- **Changes are instant cuts.** There's no animated transition between counts yet. For a
  softer change, put a short Fade or Luma Wipe stinger over it, or trigger on the beat so the
  cut feels intentional.
- **Performance:** the camera is rendered once per frame and each tile is one textured
  quad, so 2 tiles and 64 tiles cost about the same.
- **Websocket security:** keep the obs-websocket password on. Streamer.bot and the example
  scripts both support it.
