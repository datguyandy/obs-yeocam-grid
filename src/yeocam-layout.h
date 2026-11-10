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

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YEOCAM_MAX_TILES 64

enum yeocam_fit {
	YEOCAM_FIT_FILL = 0, /* tile fills its cell, source is cropped */
	YEOCAM_FIT_FIT = 1,  /* whole source visible, tile letterboxed in its cell */
};

enum yeocam_mirror {
	YEOCAM_MIRROR_NONE = 0,
	YEOCAM_MIRROR_ALL = 1,
	YEOCAM_MIRROR_ALTERNATE = 2,
};

struct yeocam_layout_params {
	uint32_t count;
	uint32_t columns; /* 0 = derive from rows, or auto */
	uint32_t rows;    /* 0 = derive from columns, or auto */
	float gap;
	float out_w, out_h;
	float src_w, src_h;
	enum yeocam_fit fit;
	float focus_x, focus_y; /* 0..1: which part of the source is kept (fill) or where the tile sits (fit) */
	float zoom;             /* >= 1 */
	enum yeocam_mirror mirror;
};

struct yeocam_tile {
	/* destination rectangle in output pixels */
	float x, y, w, h;
	/* source rectangle in source pixels */
	uint32_t sx, sy, sw, sh;
	bool mirrored;
};

/* Computes up to YEOCAM_MAX_TILES tiles. Returns the number of tiles written. */
uint32_t yeocam_layout_compute(const struct yeocam_layout_params *p, struct yeocam_tile *tiles);

#ifdef __cplusplus
}
#endif
