/* Standalone checks for the grid/crop maths. Build with any C compiler:
 *   cl /nologo /I src tests\layout-test.c src\yeocam-layout.c
 *   cc -I src tests/layout-test.c src/yeocam-layout.c -lm
 */

#include "yeocam-layout.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;

#define CHECK(cond)                                                              \
	do {                                                                     \
		if (!(cond)) {                                                   \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
			failures++;                                              \
		}                                                                \
	} while (0)

static int near(float a, float b)
{
	return fabsf(a - b) < 0.51f;
}

static struct yeocam_layout_params base(void)
{
	struct yeocam_layout_params p = {0};
	p.count = 6;
	p.gap = 0.0f;
	p.out_w = 1920.0f;
	p.out_h = 1080.0f;
	p.src_w = 1920.0f;
	p.src_h = 1080.0f;
	p.fit = YEOCAM_FIT_FILL;
	p.focus_x = 0.5f;
	p.focus_y = 0.5f;
	p.zoom = 1.0f;
	return p;
}

int main(void)
{
	struct yeocam_tile t[YEOCAM_MAX_TILES];

	/* Auto layout: 6 tiles -> 3 columns x 2 rows filling the canvas. */
	struct yeocam_layout_params p = base();
	CHECK(yeocam_layout_compute(&p, t) == 6);
	CHECK(near(t[0].w, 640.0f) && near(t[0].h, 540.0f));
	CHECK(near(t[5].x, 1280.0f) && near(t[5].y, 540.0f));
	/* Fill crops the 16:9 source to the 640:540 tile, centred. */
	CHECK(t[0].sh == 1080 && t[0].sw == 1280);
	CHECK(t[0].sx == 320 && t[0].sy == 0);

	/* Left anchor keeps the left side of the camera. */
	p.focus_x = 0.0f;
	yeocam_layout_compute(&p, t);
	CHECK(t[0].sx == 0);
	p.focus_x = 1.0f;
	yeocam_layout_compute(&p, t);
	CHECK(t[0].sx == 640);

	/* Zoom 200% halves the sampled region around the focus point. */
	p = base();
	p.zoom = 2.0f;
	yeocam_layout_compute(&p, t);
	CHECK(t[0].sw == 640 && t[0].sh == 540);
	CHECK(t[0].sx == 640 && t[0].sy == 270);

	/* Fit keeps the whole frame and letterboxes inside the cell. */
	p = base();
	p.fit = YEOCAM_FIT_FIT;
	yeocam_layout_compute(&p, t);
	CHECK(t[0].sw == 1920 && t[0].sh == 1080);
	CHECK(near(t[0].w, 640.0f) && near(t[0].h, 360.0f));
	CHECK(near(t[0].y, 90.0f));

	/* Incomplete last row is centred: 5 tiles, 3 columns. */
	p = base();
	p.count = 5;
	p.columns = 3;
	yeocam_layout_compute(&p, t);
	CHECK(near(t[3].x, 320.0f) && near(t[4].x, 960.0f));

	/* Fixed rows: 6 tiles in 1 row. */
	p = base();
	p.rows = 1;
	yeocam_layout_compute(&p, t);
	CHECK(near(t[0].w, 320.0f) && near(t[0].h, 1080.0f));

	/* Gap shrinks cells and offsets the first tile. */
	p = base();
	p.count = 4;
	p.gap = 20.0f;
	yeocam_layout_compute(&p, t);
	CHECK(near(t[0].x, 20.0f) && near(t[0].w, 930.0f));

	/* Alternate mirroring. */
	p = base();
	p.mirror = YEOCAM_MIRROR_ALTERNATE;
	yeocam_layout_compute(&p, t);
	CHECK(!t[0].mirrored && t[1].mirrored && !t[2].mirrored);

	/* Degenerate inputs produce no tiles instead of garbage. */
	p = base();
	p.src_w = 0.0f;
	CHECK(yeocam_layout_compute(&p, t) == 0);
	p = base();
	p.gap = 2000.0f;
	CHECK(yeocam_layout_compute(&p, t) == 0);
	p = base();
	p.count = 500;
	CHECK(yeocam_layout_compute(&p, t) == YEOCAM_MAX_TILES);

	printf(failures ? "%d failure(s)\n" : "all layout checks passed\n", failures);
	return failures ? 1 : 0;
}
