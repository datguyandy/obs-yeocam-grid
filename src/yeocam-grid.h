/*
Yeocam Grid
Copyright (C) 2026 Yeocam Grid contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

#include <obs-module.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YEOCAM_GRID_ID "yeocam_grid_source"

/* Settings keys. These are also the keys obs-websocket's SetInputSettings uses. */
#define S_TARGET "source"
#define S_COUNT "count"
#define S_COLUMNS "columns"
#define S_ROWS "rows"
#define S_GAP "gap"
#define S_WIDTH "width"
#define S_HEIGHT "height"
#define S_FIT "fit"
#define S_ANCHOR "anchor"
#define S_FOCUS_X "focus_x"
#define S_FOCUS_Y "focus_y"
#define S_ZOOM "zoom"
#define S_MIRROR "mirror"
#define S_BG_COLOR "background_color"

/* Layout sequence and beat sync */
#define S_SEQUENCE "sequence"
#define S_SEQ_FLIP "sequence_flip"
#define S_BEAT_SYNC "beat_sync"
#define S_AUDIO "audio_source"
#define S_SENSITIVITY "beat_sensitivity"
#define S_BEATS_PER_STEP "beats_per_step"
#define S_MIN_INTERVAL "beat_min_interval"
#define S_SILENCE_RESET "silence_reset"

#define YEOCAM_MAX_SEQUENCE 16

/* Anchor values 0..8 are a 3x3 grid read left to right, top to bottom. */
#define YEOCAM_ANCHOR_CENTER 4
#define YEOCAM_ANCHOR_CUSTOM 9

extern struct obs_source_info yeocam_grid_info;

bool yeocam_grid_is_grid(obs_source_t *source);
long long yeocam_grid_adjust_count(obs_source_t *grid, long long delta);
long long yeocam_grid_set_count(obs_source_t *grid, long long count);
long long yeocam_grid_cycle_fit(obs_source_t *grid);
long long yeocam_grid_set_anchor(obs_source_t *grid, long long anchor);
long long yeocam_grid_cycle_anchor(obs_source_t *grid);

/* Sequence control. Thread-safe; takes effect on the next frame. */
void yeocam_grid_next_layout(obs_source_t *grid);
void yeocam_grid_reset_layout(obs_source_t *grid);
bool yeocam_grid_set_beat_sync(obs_source_t *grid, bool enabled);
bool yeocam_grid_toggle_beat_sync(obs_source_t *grid);
bool yeocam_grid_beat_sync_enabled(obs_source_t *grid);

/* Tile count currently on screen (the sequence step, or Tile count when resting). */
long long yeocam_grid_live_count(obs_source_t *grid);
/* Sequence position: -1 when resting on Tile count. */
long long yeocam_grid_sequence_index(obs_source_t *grid);

#ifdef __cplusplus
}
#endif
