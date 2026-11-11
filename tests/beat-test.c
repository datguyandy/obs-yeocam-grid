/* Standalone checks for the beat detector using synthetic audio. Build with any C compiler:
 *   cl /nologo /I src tests\beat-test.c src\yeocam-beat.c
 *   cc -I src tests/beat-test.c src/yeocam-beat.c -lm
 */

#include "yeocam-beat.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define RATE 48000
#define BLOCK 1024 /* OBS hands audio over in 1024-frame blocks */
#define PI 3.14159265358979f

static int failures = 0;

#define CHECK(cond, ...)                                                       \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL %s:%d: %s  ", __FILE__, __LINE__, #cond); \
			printf(__VA_ARGS__);                                   \
			printf("\n");                                          \
			failures++;                                            \
		}                                                              \
	} while (0)

static uint32_t rng = 12345;
static float noise(void)
{
	rng = rng * 1664525u + 1013904223u;
	return ((float)(rng >> 8) / 16777216.0f) * 2.0f - 1.0f;
}

struct track {
	float bpm;
	float kick_gain;  /* 0 = no kick */
	float hat_gain;   /* hi-hat noise bursts on 8th notes */
	float pad_gain;   /* sustained chord */
	float noise_gain; /* constant broadband noise */
	float bass_gain;  /* sustained sub-bass line */
};

static float sample_at(const struct track *t, uint32_t n)
{
	const float time = (float)n / RATE;
	float x = 0.0f;

	if (t->kick_gain > 0.0f) {
		const float beat = 60.0f / t->bpm;
		const float since = fmodf(time, beat);
		/* 55 Hz sine with a pitch drop and fast decay: a typical kick */
		const float f = 55.0f + 60.0f * expf(-since * 40.0f);
		x += t->kick_gain * sinf(2.0f * PI * f * since) * expf(-since * 12.0f);
	}
	if (t->hat_gain > 0.0f) {
		const float eighth = 30.0f / t->bpm;
		const float since = fmodf(time, eighth);
		x += t->hat_gain * noise() * expf(-since * 80.0f);
	}
	if (t->pad_gain > 0.0f)
		x += t->pad_gain *
		     (sinf(2.0f * PI * 220.0f * time) + sinf(2.0f * PI * 277.2f * time) +
		      sinf(2.0f * PI * 329.6f * time)) /
		     3.0f;
	if (t->noise_gain > 0.0f)
		x += t->noise_gain * noise();
	if (t->bass_gain > 0.0f)
		x += t->bass_gain * sinf(2.0f * PI * 41.2f * time);
	return x;
}

static uint32_t run(const struct track *t, float seconds, int sensitivity, struct yeocam_beat *out)
{
	struct yeocam_beat b;
	yeocam_beat_init(&b, RATE);
	yeocam_beat_set_params(&b, sensitivity, 250);

	static float left[BLOCK], right[BLOCK];
	const float *ch[2] = {left, right};
	const uint32_t total = (uint32_t)(seconds * RATE);
	uint32_t beats = 0;

	for (uint32_t pos = 0; pos < total; pos += BLOCK) {
		uint32_t n = total - pos < BLOCK ? total - pos : BLOCK;
		for (uint32_t i = 0; i < n; i++)
			left[i] = right[i] = sample_at(t, pos + i);
		beats += yeocam_beat_process(&b, ch, 2, n);
	}

	if (out)
		*out = b;
	return beats;
}

int main(void)
{
	/* 120 BPM dance loop for 20 s: 40 kicks. The first second is spent learning the level. */
	struct track dance = {.bpm = 120, .kick_gain = 0.8f, .hat_gain = 0.15f, .pad_gain = 0.2f};
	uint32_t n = run(&dance, 20.0f, 60, NULL);
	CHECK(n >= 36 && n <= 40, "120 BPM dance: %u beats, expected 36-40", n);

	/* Faster track, 150 BPM: 50 kicks. */
	struct track fast = {.bpm = 150, .kick_gain = 0.7f, .hat_gain = 0.2f, .pad_gain = 0.25f};
	n = run(&fast, 20.0f, 60, NULL);
	CHECK(n >= 45 && n <= 50, "150 BPM: %u beats, expected 45-50", n);

	/* Quiet mix (kick 20 dB down) is still detected; detection is relative, not absolute. */
	struct track quiet = {.bpm = 120, .kick_gain = 0.08f, .hat_gain = 0.015f, .pad_gain = 0.02f};
	n = run(&quiet, 20.0f, 60, NULL);
	CHECK(n >= 36 && n <= 40, "quiet mix: %u beats, expected 36-40", n);

	/* Busy mix with constant noise: fewer, but still mostly on the kicks. */
	struct track busy = {.bpm = 120, .kick_gain = 0.6f, .hat_gain = 0.3f, .pad_gain = 0.4f, .noise_gain = 0.1f};
	n = run(&busy, 20.0f, 60, NULL);
	CHECK(n >= 30 && n <= 40, "busy mix: %u beats, expected 30-40", n);

	/* Kick over a constant sub-bass line: smaller jumps, still mostly caught at default sensitivity. */
	struct track bassline = {.bpm = 124, .kick_gain = 0.7f, .hat_gain = 0.15f, .pad_gain = 0.2f, .bass_gain = 0.25f};
	n = run(&bassline, 20.0f, 60, NULL);
	CHECK(n >= 33 && n <= 42, "kick over bassline: %u beats, expected 33-42", n);

	/* A bassline alone is not a beat. */
	struct track bass_only = {.bass_gain = 0.4f, .pad_gain = 0.2f};
	n = run(&bass_only, 20.0f, 60, NULL);
	CHECK(n <= 2, "bassline only: %u beats, expected 0-2", n);

	/* No kick: a pad and hi-hats alone must not trigger. */
	struct track pad = {.bpm = 120, .hat_gain = 0.3f, .pad_gain = 0.5f};
	n = run(&pad, 20.0f, 60, NULL);
	CHECK(n <= 2, "pad and hats only: %u beats, expected 0-2", n);

	/* Steady noise must not trigger. */
	struct track hiss = {.noise_gain = 0.3f};
	n = run(&hiss, 20.0f, 60, NULL);
	CHECK(n <= 2, "steady noise: %u beats, expected 0-2", n);

	/* Lowest sensitivity still catches a clear kick; minimum interval caps the rate. */
	n = run(&dance, 20.0f, 0, NULL);
	CHECK(n >= 30 && n <= 40, "sensitivity 0: %u beats, expected 30-40", n);
	n = run(&dance, 20.0f, 100, NULL);
	CHECK(n <= 80, "sensitivity 100 respects the 250 ms minimum: %u beats", n);

	/* Silence: no beats, and the silence timer runs. */
	struct track silence = {0};
	struct yeocam_beat b;
	n = run(&silence, 5.0f, 60, &b);
	CHECK(n == 0, "silence: %u beats", n);
	CHECK(yeocam_beat_seconds_since_sound(&b) > 4.0, "silence timer: %.2f s", yeocam_beat_seconds_since_sound(&b));

	/* Music: the silence timer stays near zero. */
	run(&dance, 5.0f, 60, &b);
	CHECK(yeocam_beat_seconds_since_sound(&b) < 0.1, "music timer: %.2f s", yeocam_beat_seconds_since_sound(&b));

	printf(failures ? "%d failure(s)\n" : "all beat checks passed\n", failures);
	return failures ? 1 : 0;
}
