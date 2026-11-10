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

#include "yeocam-layout.h"

#include <math.h>

static float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static uint32_t to_px(float v, uint32_t max)
{
	if (v <= 0.0f)
		return 0;
	uint32_t r = (uint32_t)(v + 0.5f);
	return r > max ? max : r;
}

static void grid_shape(const struct yeocam_layout_params *p, uint32_t n, uint32_t *cols, uint32_t *rows)
{
	uint32_t c;

	if (p->columns > 0)
		c = p->columns;
	else if (p->rows > 0)
		c = (n + p->rows - 1) / p->rows;
	else
		c = (uint32_t)ceilf(sqrtf((float)n));

	if (c > n)
		c = n;
	if (c < 1)
		c = 1;

	*cols = c;
	*rows = (n + c - 1) / c;
}

uint32_t yeocam_layout_compute(const struct yeocam_layout_params *p, struct yeocam_tile *tiles)
{
	uint32_t n = p->count;
	if (n == 0 || p->src_w < 1.0f || p->src_h < 1.0f || p->out_w < 1.0f || p->out_h < 1.0f)
		return 0;
	if (n > YEOCAM_MAX_TILES)
		n = YEOCAM_MAX_TILES;

	uint32_t cols, rows;
	grid_shape(p, n, &cols, &rows);

	const float gap = p->gap < 0.0f ? 0.0f : p->gap;
	const float cell_w = (p->out_w - gap * (float)(cols + 1)) / (float)cols;
	const float cell_h = (p->out_h - gap * (float)(rows + 1)) / (float)rows;
	if (cell_w < 1.0f || cell_h < 1.0f)
		return 0;

	const float fx = clampf(p->focus_x, 0.0f, 1.0f);
	const float fy = clampf(p->focus_y, 0.0f, 1.0f);
	const float zoom = p->zoom < 1.0f ? 1.0f : p->zoom;
	const float src_aspect = p->src_w / p->src_h;
	const uint32_t src_w = (uint32_t)p->src_w;
	const uint32_t src_h = (uint32_t)p->src_h;

	/* Tile size and the source region it samples are the same for every tile. */
	float tile_w = cell_w, tile_h = cell_h;
	float reg_w, reg_h;

	if (p->fit == YEOCAM_FIT_FIT) {
		if (cell_w / cell_h > src_aspect)
			tile_w = cell_h * src_aspect;
		else
			tile_h = cell_w / src_aspect;
		reg_w = p->src_w;
		reg_h = p->src_h;
	} else {
		const float tile_aspect = cell_w / cell_h;
		if (src_aspect > tile_aspect) {
			reg_h = p->src_h;
			reg_w = p->src_h * tile_aspect;
		} else {
			reg_w = p->src_w;
			reg_h = p->src_w / tile_aspect;
		}
	}

	reg_w /= zoom;
	reg_h /= zoom;

	struct yeocam_tile proto = {0};
	proto.w = tile_w;
	proto.h = tile_h;
	proto.sw = to_px(reg_w, src_w);
	proto.sh = to_px(reg_h, src_h);
	if (proto.sw == 0 || proto.sh == 0)
		return 0;
	proto.sx = to_px((p->src_w - (float)proto.sw) * fx, src_w - proto.sw);
	proto.sy = to_px((p->src_h - (float)proto.sh) * fy, src_h - proto.sh);

	/* In fit mode the focus point also aligns the letterboxed tile inside its cell. */
	const float inset_x = p->fit == YEOCAM_FIT_FIT ? (cell_w - tile_w) * fx : 0.0f;
	const float inset_y = p->fit == YEOCAM_FIT_FIT ? (cell_h - tile_h) * fy : 0.0f;

	for (uint32_t i = 0; i < n; i++) {
		const uint32_t r = i / cols;
		const uint32_t k = i % cols;
		const uint32_t in_row = (n - r * cols) < cols ? (n - r * cols) : cols;
		/* Centre an incomplete last row. */
		const float row_offset = (float)(cols - in_row) * (cell_w + gap) * 0.5f;

		struct yeocam_tile *t = &tiles[i];
		*t = proto;
		t->x = gap + row_offset + (float)k * (cell_w + gap) + inset_x;
		t->y = gap + (float)r * (cell_h + gap) + inset_y;
		t->mirrored = p->mirror == YEOCAM_MIRROR_ALL || (p->mirror == YEOCAM_MIRROR_ALTERNATE && (i % 2) == 1);
	}

	return n;
}
