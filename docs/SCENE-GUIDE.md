# Scene guide

How to set Yeocam Grid up for a stream, from the simplest setup to fully automated
"gift → music → grid" moments.

- [The idea: one source, many layouts](#the-idea-one-source-many-layouts)
- [Recommended setup: one scene, change the count](#recommended-setup-one-scene-change-the-count)
- [Layout presets](#layout-presets)
- [The gift moment, fully automatic (beat sync)](#the-gift-moment-fully-automatic-beat-sync)
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

## The gift moment, fully automatic (beat sync)

The classic Yeocam moment: someone gifts, a song plays, and the cam goes 2-cam, 3-cam, 4-cam
in time with the music before going back to normal. With beat sync the grid does the
switching itself. All anything else has to do is play the song.

1. **Set up the grid as your camera** ([one-scene setup](#recommended-setup-one-scene-change-the-count)):
   Tile count `1`, Gap `0`.
2. **Add the song** to the scene as a **Media Source** (for example "Gift song"), with
   *Restart playback when source becomes active* on and *Loop* off. Hide it (eye icon).
   A hidden Media Source is silent, and showing it starts the song from the top.
3. **In the grid's properties:**
   - **Layout sequence → Sequence:** the layouts to step through, for example
     `2x1, 3x1, 2x2, 3x3`. That's 2 side by side, 3 in a row, 2×2, then 3×3, and back round.
   - **Beat sync:** tick it, set **Music source** to "Gift song".
   - **Beats per step:** `1` for a change on every kick, `2` or `4` for calmer changes.
   - **Back to resting layout after silence:** `2000` ms.
4. **Trigger the song** on a gift by showing "Gift song", from Streamer.bot (set source
   visibility), a Stream Deck source-visibility button, or an OBS show/hide hotkey. Hide it
   again afterwards so it's ready for the next gift.

What happens:

```
gift → song starts → kick: 2 cams → kick: 3 in a row → kick: 2×2 → kick: 3×3 → …
                     song ends → 2 s of silence → back to 1 cam
```

No scene switch, no timer to keep in sync with the song length. If the song is cut short,
the grid notices the silence and resets by itself.

**Using music you already play (Spotify, a DJ deck):** set **Music source** to Desktop
Audio or an Application Audio Capture of the player instead. Then toggle **Beat sync**
on and off with the *Toggle beat sync* hotkey (or the `ToggleBeatSync` websocket request), so
the cam only dances when you want it to.

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
| Next layout in sequence | steps through your **Sequence** (2x1 → 3x1 → 2x2 → …) |
| Back to resting layout | returns to Tile count |
| Toggle beat sync | turns beat sync on or off |
| Add tile | count + 1 |
| Remove tile | count − 1 |
| Toggle fill / fit | switches tile fit |
| Next crop anchor | cycles the 9 anchors |

Simple and needs nothing else. Any keyboard, macro pad or Stream Deck "Hotkey" button can
press these. *Next layout* plus *Back to resting layout* on two buttons gives you manual
control of the same sequence beat sync uses.

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

**With beat sync** ([above](#the-gift-moment-fully-automatic-beat-sync)), Streamer.bot only
needs to play the song: on the gift trigger, **set "Gift song" visible**, **delay** for the
song's length, then **set it hidden**. The grid handles the layouts and the reset.

**Without beat sync**, or for a fixed layout per gift size, drive the grid directly. In
Streamer.bot this is one action:

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
  `AdjustCount`, `SetCount`, `ToggleFit`, `SetAnchor`, `NextAnchor`, `NextLayout`,
  `ResetLayout`, `SetBeatSync`, `ToggleBeatSync`. See the README for parameters.

[`examples/obs-ws.mjs`](../examples/obs-ws.mjs) is a small, dependency-free Node client you
can copy.

## Syncing to music

**Use the built-in beat sync first** ([setup](#the-gift-moment-fully-automatic-beat-sync)).
It listens to the actual audio, so it follows any song and tempo, and resets when the music
stops. Tuning tips:

- **Missing kicks?** Raise **Sensitivity** (try 75). **Reacting to snares or vocals?**
  Lower it (try 45).
- **Too frantic?** Set **Beats per step** to 2 or 4, or raise the minimum time between beats.
- **Songs without a kick drum** (acoustic, ballads) won't trigger much. Use the script below
  or the *Next layout* hotkey for those.

For a fixed tempo instead, for example a song with no clear kick,
[`examples/beat-party.mjs`](../examples/beat-party.mjs) steps the grid through tile counts on
a BPM you give it:

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

The script's timing is fixed-tempo: it stays locked to the BPM for the whole song, but you
need to start it on the beat. Starting the song and the script from the same Streamer.bot
action is the easiest way to keep them lined up. Turn the grid's beat sync off while the
script runs, so the two don't fight.

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
