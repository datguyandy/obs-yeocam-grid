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
 * obs-websocket vendor requests (vendorName "yeocam").
 *
 * Every grid setting can already be changed with obs-websocket's built-in
 * SetInputSettings. These requests add the relative actions that are awkward
 * to express that way, for Stream Deck buttons and other controllers:
 *
 *   ListGrids                                -> { grids: [{ sourceName, targetName, count }] }
 *   AdjustCount  { sourceName?, delta }      -> { count }
 *   SetCount     { sourceName?, count }      -> { count }
 *   ToggleFit    { sourceName? }             -> { fit: "fill" | "fit" }
 *   SetAnchor    { sourceName?, anchor }     -> { anchor }   anchor: "top_left" ... "bottom_right", "custom"
 *   NextAnchor   { sourceName? }             -> { anchor }
 *   NextLayout   { sourceName? }             -> {}   step the layout sequence
 *   ResetLayout  { sourceName? }             -> {}   back to Tile count
 *   SetBeatSync  { sourceName?, enabled }    -> { beatSync }
 *   ToggleBeatSync { sourceName? }           -> { beatSync }
 *
 * ListGrids items also carry liveCount (tiles on screen), sequenceIndex (-1 when
 * resting) and beatSync.
 *
 * sourceName may be omitted when the scene collection has exactly one grid.
 */

#include "yeocam-grid.h"

#include <plugin-support.h>

#include "obs-websocket-api.h"

static const char *anchor_names[] = {
	"top_left", "top", "top_right", "left", "center", "right", "bottom_left", "bottom", "bottom_right", "custom",
};

static obs_websocket_vendor vendor = NULL;

struct find_ctx {
	obs_source_t *only;
	size_t count;
	obs_data_array_t *list;
};

static bool enum_grids(void *param, obs_source_t *source)
{
	struct find_ctx *ctx = param;
	if (!yeocam_grid_is_grid(source))
		return true;

	ctx->count++;
	if (ctx->count == 1)
		ctx->only = obs_source_get_ref(source);

	if (ctx->list) {
		obs_data_t *settings = obs_source_get_settings(source);
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "sourceName", obs_source_get_name(source));
		obs_data_set_string(item, "targetName", obs_data_get_string(settings, S_TARGET));
		obs_data_set_int(item, "count", obs_data_get_int(settings, S_COUNT));
		obs_data_set_int(item, "liveCount", yeocam_grid_live_count(source));
		obs_data_set_int(item, "sequenceIndex", yeocam_grid_sequence_index(source));
		obs_data_set_bool(item, "beatSync", obs_data_get_bool(settings, S_BEAT_SYNC));
		obs_data_array_push_back(ctx->list, item);
		obs_data_release(item);
		obs_data_release(settings);
	}
	return true;
}

static void fail(obs_data_t *response, const char *message)
{
	obs_data_set_bool(response, "success", false);
	obs_data_set_string(response, "error", message);
}

/* Returns a strong reference to the requested grid, or NULL after writing an error. */
static obs_source_t *resolve_grid(obs_data_t *request, obs_data_t *response)
{
	const char *name = obs_data_get_string(request, "sourceName");

	if (name && *name) {
		obs_source_t *source = obs_get_source_by_name(name);
		if (!source) {
			fail(response, "No source with that name.");
			return NULL;
		}
		if (!yeocam_grid_is_grid(source)) {
			obs_source_release(source);
			fail(response, "That source is not a Yeocam Grid.");
			return NULL;
		}
		return source;
	}

	struct find_ctx ctx = {0};
	obs_enum_sources(enum_grids, &ctx);
	if (ctx.count == 1)
		return ctx.only;

	obs_source_release(ctx.only);
	fail(response, ctx.count == 0 ? "There are no Yeocam Grid sources."
				      : "Several Yeocam Grid sources exist; pass sourceName.");
	return NULL;
}

static void ok(obs_data_t *response)
{
	obs_data_set_bool(response, "success", true);
}

static void req_list(obs_data_t *request, obs_data_t *response, void *priv)
{
	UNUSED_PARAMETER(request);
	UNUSED_PARAMETER(priv);

	struct find_ctx ctx = {0};
	ctx.list = obs_data_array_create();
	obs_enum_sources(enum_grids, &ctx);
	obs_source_release(ctx.only);

	obs_data_set_array(response, "grids", ctx.list);
	obs_data_array_release(ctx.list);
	ok(response);
}

static void req_adjust_count(obs_data_t *request, obs_data_t *response, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *grid = resolve_grid(request, response);
	if (!grid)
		return;
	long long delta = obs_data_has_user_value(request, "delta") ? obs_data_get_int(request, "delta") : 1;
	obs_data_set_int(response, "count", yeocam_grid_adjust_count(grid, delta));
	obs_source_release(grid);
	ok(response);
}

static void req_set_count(obs_data_t *request, obs_data_t *response, void *priv)
{
	UNUSED_PARAMETER(priv);
	if (!obs_data_has_user_value(request, "count")) {
		fail(response, "Missing count.");
		return;
	}
	obs_source_t *grid = resolve_grid(request, response);
	if (!grid)
		return;
	obs_data_set_int(response, "count", yeocam_grid_set_count(grid, obs_data_get_int(request, "count")));
	obs_source_release(grid);
	ok(response);
}

static void req_toggle_fit(obs_data_t *request, obs_data_t *response, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *grid = resolve_grid(request, response);
	if (!grid)
		return;
	long long fit = yeocam_grid_cycle_fit(grid);
	obs_data_set_string(response, "fit", fit == 1 ? "fit" : "fill");
	obs_source_release(grid);
	ok(response);
}

static void req_set_anchor(obs_data_t *request, obs_data_t *response, void *priv)
{
	UNUSED_PARAMETER(priv);
	const char *wanted = obs_data_get_string(request, "anchor");
	long long index = -1;
	for (long long i = 0; i < (long long)OBS_COUNTOF(anchor_names); i++) {
		if (wanted && strcmp(wanted, anchor_names[i]) == 0)
			index = i;
	}
	if (index < 0) {
		fail(response, "Unknown anchor. Use top_left, top, top_right, left, center, right, "
			       "bottom_left, bottom, bottom_right or custom.");
		return;
	}

	obs_source_t *grid = resolve_grid(request, response);
	if (!grid)
		return;
	yeocam_grid_set_anchor(grid, index);
	obs_data_set_string(response, "anchor", anchor_names[index]);
	obs_source_release(grid);
	ok(response);
}

static void req_next_anchor(obs_data_t *request, obs_data_t *response, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *grid = resolve_grid(request, response);
	if (!grid)
		return;
	long long index = yeocam_grid_cycle_anchor(grid);
	obs_data_set_string(response, "anchor", anchor_names[index]);
	obs_source_release(grid);
	ok(response);
}

static void req_next_layout(obs_data_t *request, obs_data_t *response, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *grid = resolve_grid(request, response);
	if (!grid)
		return;
	yeocam_grid_next_layout(grid);
	obs_source_release(grid);
	ok(response);
}

static void req_reset_layout(obs_data_t *request, obs_data_t *response, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *grid = resolve_grid(request, response);
	if (!grid)
		return;
	yeocam_grid_reset_layout(grid);
	obs_source_release(grid);
	ok(response);
}

static void req_set_beat_sync(obs_data_t *request, obs_data_t *response, void *priv)
{
	UNUSED_PARAMETER(priv);
	if (!obs_data_has_user_value(request, "enabled")) {
		fail(response, "Missing enabled.");
		return;
	}
	obs_source_t *grid = resolve_grid(request, response);
	if (!grid)
		return;
	obs_data_set_bool(response, "beatSync", yeocam_grid_set_beat_sync(grid, obs_data_get_bool(request, "enabled")));
	obs_source_release(grid);
	ok(response);
}

static void req_toggle_beat_sync(obs_data_t *request, obs_data_t *response, void *priv)
{
	UNUSED_PARAMETER(priv);
	obs_source_t *grid = resolve_grid(request, response);
	if (!grid)
		return;
	obs_data_set_bool(response, "beatSync", yeocam_grid_toggle_beat_sync(grid));
	obs_source_release(grid);
	ok(response);
}

static const struct {
	const char *name;
	obs_websocket_request_callback_function fn;
} requests[] = {
	{"ListGrids", req_list},
	{"AdjustCount", req_adjust_count},
	{"SetCount", req_set_count},
	{"ToggleFit", req_toggle_fit},
	{"SetAnchor", req_set_anchor},
	{"NextAnchor", req_next_anchor},
	{"NextLayout", req_next_layout},
	{"ResetLayout", req_reset_layout},
	{"SetBeatSync", req_set_beat_sync},
	{"ToggleBeatSync", req_toggle_beat_sync},
};

void yeocam_websocket_register(void)
{
	vendor = obs_websocket_register_vendor("yeocam");
	if (!vendor) {
		obs_log(LOG_INFO, "obs-websocket not available, vendor requests disabled");
		return;
	}

	for (size_t i = 0; i < OBS_COUNTOF(requests); i++)
		obs_websocket_vendor_register_request(vendor, requests[i].name, requests[i].fn, NULL);
	obs_log(LOG_INFO, "registered obs-websocket vendor 'yeocam'");
}

void yeocam_websocket_unregister(void)
{
	if (!vendor)
		return;
	for (size_t i = 0; i < OBS_COUNTOF(requests); i++)
		obs_websocket_vendor_unregister_request(vendor, requests[i].name);
	vendor = NULL;
}
