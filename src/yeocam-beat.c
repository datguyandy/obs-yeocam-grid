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

#include "yeocam-beat.h"

#include <math.h>
#include <string.h>

#define LOWPASS_HZ 150.0f
#define WINDOW_SEC (1.0f / 23.0f)    /* ~43 ms: long enough that noise doesn't look like kicks */
#define ONSET_WINDOWS 4              /* compare against the ~170 ms just before */
#define MIN_HISTORY ONSET_WINDOWS
#define SOUND_FLOOR_POWER 1.0e-5f    /* ~-50 dBFS mean power: below this is "silence" */
#define BEAT_FLOOR_POWER 2.0e-6f     /* bass window must be at least this loud to count */

void yeocam_beat_init(struct yeocam_beat *b, uint32_t sample_rate)
{
	memset(b, 0, sizeof(*b));
	b->sample_rate = sample_rate > 0 ? (float)sample_rate : 48000.0f;
	b->lp_alpha = 1.0f - expf(-2.0f * 3.14159265f * LOWPASS_HZ / b->sample_rate);
	b->win_size = (uint32_t)(b->sample_rate * WINDOW_SEC);
	if (b->win_size < 64)
		b->win_size = 64;
	b->last_beat_time = -1000.0;
	b->last_sound_time = -1000.0;
	yeocam_beat_set_params(b, 60, 250);
}

void yeocam_beat_set_params(struct yeocam_beat *b, int sensitivity, int min_interval_ms)
{
	if (sensitivity < 0)
		sensitivity = 0;
	if (sensitivity > 100)
		sensitivity = 100;
	if (min_interval_ms < 50)
		min_interval_ms = 50;

	/* Required jump over the preceding ~170 ms: 0 -> 5x, 60 -> 2.9x, 100 -> 1.5x */
	b->threshold = 5.0f - (float)sensitivity * 0.035f;
	b->min_interval = (float)min_interval_ms / 1000.0f;
}

static bool end_window(struct yeocam_beat *b)
{
	const float n = (float)b->win_frames;
	const float energy = b->win_bass / n;
	const float full = b->win_full / n;
	bool beat = false;

	if (full > SOUND_FLOOR_POWER)
		b->last_sound_time = b->time;

	if (b->history_count >= MIN_HISTORY) {
		/* Onset: bass energy jumps well above the trough just before it. */
		float sum = 0.0f;
		for (uint32_t i = 0; i < ONSET_WINDOWS; i++)
			sum += b->history[i];
		const float before = sum / (float)ONSET_WINDOWS;

		if (energy > BEAT_FLOOR_POWER && energy > before * b->threshold && energy > b->prev_energy &&
		    b->time - b->last_beat_time >= b->min_interval) {
			beat = true;
			b->last_beat_time = b->time;
		}
	}

	b->history[b->history_pos] = energy;
	b->history_pos = (b->history_pos + 1) % ONSET_WINDOWS;
	if (b->history_count < ONSET_WINDOWS)
		b->history_count++;

	b->prev_energy = energy;
	b->win_bass = 0.0f;
	b->win_full = 0.0f;
	b->win_frames = 0;
	return beat;
}

uint32_t yeocam_beat_process(struct yeocam_beat *b, const float *const *channels, uint32_t num_channels,
			     uint32_t frames)
{
	if (num_channels == 0 || !channels || !channels[0])
		return 0;
	if (num_channels > 2)
		num_channels = 2; /* front left/right carry the music */
	if (num_channels == 2 && !channels[1])
		num_channels = 1;

	const float a = b->lp_alpha;
	const float scale = 1.0f / (float)num_channels;
	const double dt = 1.0 / b->sample_rate;
	uint32_t beats = 0;

	for (uint32_t i = 0; i < frames; i++) {
		float x = channels[0][i];
		if (num_channels == 2)
			x += channels[1][i];
		x *= scale;

		b->lp1 += a * (x - b->lp1);
		b->lp2 += a * (b->lp1 - b->lp2);

		b->win_bass += b->lp2 * b->lp2;
		b->win_full += x * x;
		b->time += dt;

		if (++b->win_frames >= b->win_size && end_window(b))
			beats++;
	}

	return beats;
}

void yeocam_beat_skip(struct yeocam_beat *b, uint32_t frames)
{
	for (uint32_t i = 0; i < frames; i++) {
		b->lp1 += b->lp_alpha * (0.0f - b->lp1);
		b->lp2 += b->lp_alpha * (b->lp1 - b->lp2);
		b->win_bass += b->lp2 * b->lp2;
		b->time += 1.0 / b->sample_rate;
		if (++b->win_frames >= b->win_size)
			end_window(b);
	}
}

double yeocam_beat_seconds_since_sound(const struct yeocam_beat *b)
{
	return b->time - b->last_sound_time;
}
