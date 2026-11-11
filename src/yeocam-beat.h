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

/*
 * Lightweight beat (kick) detector.
 *
 * Low-passes the audio to the bass range, measures energy in ~43 ms windows,
 * and reports a beat when a window's energy jumps well above the ~170 ms just
 * before it (an onset). Costs a few multiply-adds per sample; no FFT, no
 * allocation.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YEOCAM_BEAT_HISTORY 4

struct yeocam_beat {
	float sample_rate;

	/* Two cascaded one-pole low-pass filters (~150 Hz). */
	float lp_alpha;
	float lp1, lp2;

	uint32_t win_size;
	uint32_t win_frames;
	float win_bass;  /* sum of bass^2 in the current window */
	float win_full;  /* sum of full-band^2 in the current window */

	float history[YEOCAM_BEAT_HISTORY];
	uint32_t history_pos;
	uint32_t history_count;
	float prev_energy;

	double time;            /* seconds of audio processed */
	double last_beat_time;  /* seconds */
	double last_sound_time; /* seconds */

	/* Parameters */
	float threshold;    /* energy must exceed threshold * average */
	float min_interval; /* seconds between beats */
};

void yeocam_beat_init(struct yeocam_beat *b, uint32_t sample_rate);

/* sensitivity: 0 (only strong kicks) .. 100 (very twitchy). min_interval_ms: shortest gap between beats. */
void yeocam_beat_set_params(struct yeocam_beat *b, int sensitivity, int min_interval_ms);

/* Feeds planar float audio. Returns how many beats were detected in this block. */
uint32_t yeocam_beat_process(struct yeocam_beat *b, const float *const *channels, uint32_t num_channels,
			     uint32_t frames);

/* Feeds silence of the given length (for muted sources). */
void yeocam_beat_skip(struct yeocam_beat *b, uint32_t frames);

/* Seconds since the audio was last above the silence floor. */
double yeocam_beat_seconds_since_sound(const struct yeocam_beat *b);

#ifdef __cplusplus
}
#endif
