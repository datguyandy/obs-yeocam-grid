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
 * Yeocam Grid source
 *
 * Renders one target video source into a texture once per frame, then draws
 * that texture N times as a grid of tiles. The camera is opened and decoded
 * once; each extra tile only costs one textured quad on the GPU.
 */

#include "yeocam-grid.h"
#include "yeocam-layout.h"

#include <plugin-support.h>
#include <graphics/vec4.h>
#include <util/threading.h>

#define T(s) obs_module_text(s)

#define RESOLVE_INTERVAL_SEC 1.0f

struct yeocam_cfg {
	uint32_t count;
	uint32_t columns;
	uint32_t rows;
	uint32_t width;  /* 0 = canvas width */
	uint32_t height; /* 0 = canvas height */
	float gap;
	enum yeocam_fit fit;
	float focus_x, focus_y;
	float zoom;
	enum yeocam_mirror mirror;
	uint32_t bg_color; /* 0xAABBGGRR as stored by OBS colour properties */
};

struct yeocam_grid {
	obs_source_t *self;

	pthread_mutex_t target_lock; /* serialises set_target() between UI and graphics threads */
	pthread_mutex_t lock;
	/* Guarded by lock. */
	obs_weak_source_t *target;
	char *target_name;
	struct yeocam_cfg cfg;

	/* Graphics thread only. */
	gs_texrender_t *texrender;
	enum gs_color_space tex_space;
	bool tex_valid; /* target already rendered this frame */
	bool rendering; /* recursion guard */
	struct yeocam_tile tiles[YEOCAM_MAX_TILES];

	float resolve_timer;

	obs_hotkey_id hk_add;
	obs_hotkey_id hk_remove;
	obs_hotkey_id hk_fit;
	obs_hotkey_id hk_anchor;
};

/* ------------------------------------------------------------------------- */
/* Target handling                                                           */

static obs_source_t *get_target(struct yeocam_grid *g)
{
	pthread_mutex_lock(&g->lock);
	obs_source_t *target = obs_weak_source_get_source(g->target);
	pthread_mutex_unlock(&g->lock);
	return target;
}

static void set_target_locked(struct yeocam_grid *g, const char *name);

static void set_target(struct yeocam_grid *g, const char *name)
{
	pthread_mutex_lock(&g->target_lock);
	set_target_locked(g, name);
	pthread_mutex_unlock(&g->target_lock);
}

static void set_target_locked(struct yeocam_grid *g, const char *name)
{
	obs_source_t *source = (name && *name) ? obs_get_source_by_name(name) : NULL;

	if (source == g->self || (source && yeocam_grid_is_grid(source))) {
		obs_log(LOG_WARNING, "[%s] refusing to tile '%s' (a grid cannot target itself or another grid)",
			obs_source_get_name(g->self), name);
		obs_source_release(source);
		source = NULL;
	}

	obs_source_t *old = get_target(g);
	if (old == source && old) {
		obs_source_release(old);
		obs_source_release(source);
		return;
	}

	/* Keep the old target's activation in sync, so cameras set to
	 * "deactivate when not showing" turn on and off with the grid. */
	if (old)
		obs_source_remove_active_child(g->self, old);

	if (source && !obs_source_add_active_child(g->self, source)) {
		obs_log(LOG_WARNING, "[%s] '%s' would create a render loop, ignoring it",
			obs_source_get_name(g->self), name);
		obs_source_release(source);
		source = NULL;
	}

	pthread_mutex_lock(&g->lock);
	obs_weak_source_release(g->target);
	g->target = source ? obs_source_get_weak_source(source) : NULL;
	if (!g->target_name || !name || strcmp(g->target_name, name) != 0) {
		bfree(g->target_name);
		g->target_name = bstrdup(name ? name : "");
	}
	pthread_mutex_unlock(&g->lock);

	obs_source_release(old);
	obs_source_release(source);
}

static void on_source_rename(void *data, calldata_t *cd)
{
	struct yeocam_grid *g = data;
	const char *prev = calldata_string(cd, "prev_name");
	const char *next = calldata_string(cd, "new_name");
	if (!prev || !next)
		return;

	bool matched = false;
	pthread_mutex_lock(&g->lock);
	if (g->target_name && strcmp(g->target_name, prev) == 0) {
		bfree(g->target_name);
		g->target_name = bstrdup(next);
		matched = true;
	}
	pthread_mutex_unlock(&g->lock);

	if (matched) {
		obs_data_t *settings = obs_source_get_settings(g->self);
		obs_data_set_string(settings, S_TARGET, next);
		obs_data_release(settings);
	}
}

/* ------------------------------------------------------------------------- */
/* Settings                                                                  */

static void anchor_to_focus(long long anchor, float *fx, float *fy)
{
	*fx = (float)(anchor % 3) * 0.5f;
	*fy = (float)(anchor / 3) * 0.5f;
}

static void read_cfg(struct yeocam_cfg *c, obs_data_t *s)
{
	c->count = (uint32_t)obs_data_get_int(s, S_COUNT);
	c->columns = (uint32_t)obs_data_get_int(s, S_COLUMNS);
	c->rows = (uint32_t)obs_data_get_int(s, S_ROWS);
	c->width = (uint32_t)obs_data_get_int(s, S_WIDTH);
	c->height = (uint32_t)obs_data_get_int(s, S_HEIGHT);
	c->gap = (float)obs_data_get_int(s, S_GAP);
	c->fit = obs_data_get_int(s, S_FIT) == YEOCAM_FIT_FIT ? YEOCAM_FIT_FIT : YEOCAM_FIT_FILL;
	c->zoom = (float)obs_data_get_int(s, S_ZOOM) / 100.0f;
	c->mirror = (enum yeocam_mirror)obs_data_get_int(s, S_MIRROR);
	c->bg_color = (uint32_t)obs_data_get_int(s, S_BG_COLOR);

	long long anchor = obs_data_get_int(s, S_ANCHOR);
	if (anchor >= 0 && anchor < YEOCAM_ANCHOR_CUSTOM) {
		anchor_to_focus(anchor, &c->focus_x, &c->focus_y);
	} else {
		c->focus_x = (float)obs_data_get_double(s, S_FOCUS_X) / 100.0f;
		c->focus_y = (float)obs_data_get_double(s, S_FOCUS_Y) / 100.0f;
	}

	if (c->count < 1)
		c->count = 1;
	if (c->count > YEOCAM_MAX_TILES)
		c->count = YEOCAM_MAX_TILES;
}

static void yeocam_update(void *data, obs_data_t *settings)
{
	struct yeocam_grid *g = data;

	struct yeocam_cfg cfg;
	read_cfg(&cfg, settings);

	pthread_mutex_lock(&g->lock);
	g->cfg = cfg;
	pthread_mutex_unlock(&g->lock);

	set_target(g, obs_data_get_string(settings, S_TARGET));
}

static void yeocam_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, S_TARGET, "");
	obs_data_set_default_int(settings, S_COUNT, 4);
	obs_data_set_default_int(settings, S_COLUMNS, 0);
	obs_data_set_default_int(settings, S_ROWS, 0);
	obs_data_set_default_int(settings, S_GAP, 16);
	obs_data_set_default_int(settings, S_WIDTH, 0);
	obs_data_set_default_int(settings, S_HEIGHT, 0);
	obs_data_set_default_int(settings, S_FIT, YEOCAM_FIT_FILL);
	obs_data_set_default_int(settings, S_ANCHOR, YEOCAM_ANCHOR_CENTER);
	obs_data_set_default_double(settings, S_FOCUS_X, 50.0);
	obs_data_set_default_double(settings, S_FOCUS_Y, 50.0);
	obs_data_set_default_int(settings, S_ZOOM, 100);
	obs_data_set_default_int(settings, S_MIRROR, YEOCAM_MIRROR_NONE);
	obs_data_set_default_int(settings, S_BG_COLOR, 0);
}

static bool add_video_source(void *param, obs_source_t *source)
{
	obs_property_t *list = param;
	if ((obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) == 0)
		return true;
	if (yeocam_grid_is_grid(source))
		return true;
	const char *name = obs_source_get_name(source);
	obs_property_list_add_string(list, name, name);
	return true;
}

static bool anchor_modified(obs_properties_t *props, obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	const bool custom = obs_data_get_int(settings, S_ANCHOR) == YEOCAM_ANCHOR_CUSTOM;
	obs_property_set_visible(obs_properties_get(props, S_FOCUS_X), custom);
	obs_property_set_visible(obs_properties_get(props, S_FOCUS_Y), custom);
	return true;
}

static obs_properties_t *yeocam_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	p = obs_properties_add_list(props, S_TARGET, T("Source"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T("Source.None"), "");
	obs_enum_sources(add_video_source, p);
	obs_property_set_long_description(p, T("Source.Description"));

	obs_properties_add_int_slider(props, S_COUNT, T("Count"), 1, YEOCAM_MAX_TILES, 1);

	p = obs_properties_add_int(props, S_COLUMNS, T("Columns"), 0, 16, 1);
	obs_property_set_long_description(p, T("Columns.Description"));
	p = obs_properties_add_int(props, S_ROWS, T("Rows"), 0, 16, 1);
	obs_property_set_long_description(p, T("Rows.Description"));

	p = obs_properties_add_int_slider(props, S_GAP, T("Gap"), 0, 400, 2);
	obs_property_int_set_suffix(p, " px");

	p = obs_properties_add_list(props, S_FIT, T("Fit"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T("Fit.Fill"), YEOCAM_FIT_FILL);
	obs_property_list_add_int(p, T("Fit.Fit"), YEOCAM_FIT_FIT);

	p = obs_properties_add_list(props, S_ANCHOR, T("Anchor"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T("Anchor.TopLeft"), 0);
	obs_property_list_add_int(p, T("Anchor.Top"), 1);
	obs_property_list_add_int(p, T("Anchor.TopRight"), 2);
	obs_property_list_add_int(p, T("Anchor.Left"), 3);
	obs_property_list_add_int(p, T("Anchor.Center"), 4);
	obs_property_list_add_int(p, T("Anchor.Right"), 5);
	obs_property_list_add_int(p, T("Anchor.BottomLeft"), 6);
	obs_property_list_add_int(p, T("Anchor.Bottom"), 7);
	obs_property_list_add_int(p, T("Anchor.BottomRight"), 8);
	obs_property_list_add_int(p, T("Anchor.Custom"), YEOCAM_ANCHOR_CUSTOM);
	obs_property_set_long_description(p, T("Anchor.Description"));
	obs_property_set_modified_callback(p, anchor_modified);

	p = obs_properties_add_float_slider(props, S_FOCUS_X, T("FocusX"), 0.0, 100.0, 1.0);
	obs_property_float_set_suffix(p, "%");
	p = obs_properties_add_float_slider(props, S_FOCUS_Y, T("FocusY"), 0.0, 100.0, 1.0);
	obs_property_float_set_suffix(p, "%");

	p = obs_properties_add_int_slider(props, S_ZOOM, T("Zoom"), 100, 400, 5);
	obs_property_int_set_suffix(p, "%");

	p = obs_properties_add_list(props, S_MIRROR, T("Mirror"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T("Mirror.None"), YEOCAM_MIRROR_NONE);
	obs_property_list_add_int(p, T("Mirror.All"), YEOCAM_MIRROR_ALL);
	obs_property_list_add_int(p, T("Mirror.Alternate"), YEOCAM_MIRROR_ALTERNATE);

	obs_properties_add_color_alpha(props, S_BG_COLOR, T("BackgroundColor"));

	p = obs_properties_add_int(props, S_WIDTH, T("Width"), 0, 8192, 1);
	obs_property_set_long_description(p, T("Size.Description"));
	p = obs_properties_add_int(props, S_HEIGHT, T("Height"), 0, 8192, 1);
	obs_property_set_long_description(p, T("Size.Description"));

	return props;
}

/* ------------------------------------------------------------------------- */
/* Remote actions (hotkeys, obs-websocket)                                   */

bool yeocam_grid_is_grid(obs_source_t *source)
{
	const char *id = obs_source_get_unversioned_id(source);
	return id && strcmp(id, YEOCAM_GRID_ID) == 0;
}

static long long apply_int(obs_source_t *grid, const char *key, long long value)
{
	obs_data_t *delta = obs_data_create();
	obs_data_set_int(delta, key, value);
	obs_source_update(grid, delta);
	obs_data_release(delta);
	return value;
}

static long long get_int(obs_source_t *grid, const char *key)
{
	obs_data_t *settings = obs_source_get_settings(grid);
	long long v = obs_data_get_int(settings, key);
	obs_data_release(settings);
	return v;
}

long long yeocam_grid_set_count(obs_source_t *grid, long long count)
{
	if (count < 1)
		count = 1;
	if (count > YEOCAM_MAX_TILES)
		count = YEOCAM_MAX_TILES;
	return apply_int(grid, S_COUNT, count);
}

long long yeocam_grid_adjust_count(obs_source_t *grid, long long delta)
{
	return yeocam_grid_set_count(grid, get_int(grid, S_COUNT) + delta);
}

long long yeocam_grid_cycle_fit(obs_source_t *grid)
{
	long long fit = get_int(grid, S_FIT) == YEOCAM_FIT_FILL ? YEOCAM_FIT_FIT : YEOCAM_FIT_FILL;
	return apply_int(grid, S_FIT, fit);
}

long long yeocam_grid_set_anchor(obs_source_t *grid, long long anchor)
{
	if (anchor < 0 || anchor > YEOCAM_ANCHOR_CUSTOM)
		anchor = YEOCAM_ANCHOR_CENTER;
	return apply_int(grid, S_ANCHOR, anchor);
}

long long yeocam_grid_cycle_anchor(obs_source_t *grid)
{
	long long anchor = get_int(grid, S_ANCHOR);
	anchor = (anchor >= YEOCAM_ANCHOR_CUSTOM - 1 || anchor < 0) ? 0 : anchor + 1;
	return yeocam_grid_set_anchor(grid, anchor);
}

static void hk_add(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (pressed)
		yeocam_grid_adjust_count(((struct yeocam_grid *)data)->self, 1);
}

static void hk_remove(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (pressed)
		yeocam_grid_adjust_count(((struct yeocam_grid *)data)->self, -1);
}

static void hk_fit(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (pressed)
		yeocam_grid_cycle_fit(((struct yeocam_grid *)data)->self);
}

static void hk_anchor(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (pressed)
		yeocam_grid_cycle_anchor(((struct yeocam_grid *)data)->self);
}

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                 */

static const char *yeocam_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return T("YeocamGrid");
}

static void *yeocam_create(obs_data_t *settings, obs_source_t *source)
{
	struct yeocam_grid *g = bzalloc(sizeof(*g));
	g->self = source;
	g->tex_space = GS_CS_SRGB;
	pthread_mutex_init(&g->lock, NULL);
	pthread_mutex_init(&g->target_lock, NULL);

	signal_handler_connect(obs_get_signal_handler(), "source_rename", on_source_rename, g);

	g->hk_add = obs_hotkey_register_source(source, "YeocamGrid.AddTile", T("Hotkey.AddTile"), hk_add, g);
	g->hk_remove = obs_hotkey_register_source(source, "YeocamGrid.RemoveTile", T("Hotkey.RemoveTile"), hk_remove, g);
	g->hk_fit = obs_hotkey_register_source(source, "YeocamGrid.ToggleFit", T("Hotkey.ToggleFit"), hk_fit, g);
	g->hk_anchor =
		obs_hotkey_register_source(source, "YeocamGrid.NextAnchor", T("Hotkey.NextAnchor"), hk_anchor, g);

	yeocam_update(g, settings);
	return g;
}

static void yeocam_destroy(void *data)
{
	struct yeocam_grid *g = data;

	signal_handler_disconnect(obs_get_signal_handler(), "source_rename", on_source_rename, g);

	obs_source_t *target = get_target(g);
	if (target) {
		obs_source_remove_active_child(g->self, target);
		obs_source_release(target);
	}

	obs_enter_graphics();
	gs_texrender_destroy(g->texrender);
	obs_leave_graphics();

	obs_weak_source_release(g->target);
	bfree(g->target_name);
	pthread_mutex_destroy(&g->lock);
	pthread_mutex_destroy(&g->target_lock);
	bfree(g);
}

static void canvas_size(uint32_t *w, uint32_t *h)
{
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		*w = ovi.base_width;
		*h = ovi.base_height;
	} else {
		*w = 1920;
		*h = 1080;
	}
}

static void output_size(struct yeocam_grid *g, uint32_t *w, uint32_t *h)
{
	pthread_mutex_lock(&g->lock);
	uint32_t cw = g->cfg.width, ch = g->cfg.height;
	pthread_mutex_unlock(&g->lock);

	if (cw == 0 || ch == 0) {
		uint32_t bw, bh;
		canvas_size(&bw, &bh);
		if (cw == 0)
			cw = bw;
		if (ch == 0)
			ch = bh;
	}
	*w = cw;
	*h = ch;
}

static uint32_t yeocam_get_width(void *data)
{
	uint32_t w, h;
	output_size(data, &w, &h);
	return w;
}

static uint32_t yeocam_get_height(void *data)
{
	uint32_t w, h;
	output_size(data, &w, &h);
	return h;
}

static void yeocam_tick(void *data, float seconds)
{
	struct yeocam_grid *g = data;

	/* Render the target at most once per frame, however many views draw us. */
	g->tex_valid = false;

	/* Retry resolving a target that did not exist yet (load order) or was
	 * removed and re-added under the same name. */
	g->resolve_timer += seconds;
	if (g->resolve_timer < RESOLVE_INTERVAL_SEC)
		return;
	g->resolve_timer = 0.0f;

	obs_source_t *target = get_target(g);
	if (target) {
		obs_source_release(target);
		return;
	}

	pthread_mutex_lock(&g->lock);
	char *name = g->target_name && *g->target_name ? bstrdup(g->target_name) : NULL;
	pthread_mutex_unlock(&g->lock);

	if (name) {
		set_target(g, name);
		bfree(name);
	}
}

static void yeocam_enum_active(void *data, obs_source_enum_proc_t enum_callback, void *param)
{
	struct yeocam_grid *g = data;
	obs_source_t *target = get_target(g);
	if (target) {
		enum_callback(g->self, target, param);
		obs_source_release(target);
	}
}

/* ------------------------------------------------------------------------- */
/* Rendering                                                                 */

static const enum gs_color_space preferred_spaces[] = {
	GS_CS_SRGB,
	GS_CS_SRGB_16F,
	GS_CS_709_EXTENDED,
};

static enum gs_color_space yeocam_get_color_space(void *data, size_t count,
						  const enum gs_color_space *preferred)
{
	UNUSED_PARAMETER(count);
	UNUSED_PARAMETER(preferred);

	struct yeocam_grid *g = data;
	enum gs_color_space space = GS_CS_SRGB;
	obs_source_t *target = get_target(g);
	if (target) {
		space = obs_source_get_color_space(target, OBS_COUNTOF(preferred_spaces), preferred_spaces);
		obs_source_release(target);
	}
	return space;
}

/* Picks the default.effect technique that converts from the texture's colour
 * space to the current render target's, matching libobs' own conversions. */
static const char *draw_technique(enum gs_color_space current, enum gs_color_space source, float *multiplier)
{
	*multiplier = 1.0f;

	switch (source) {
	case GS_CS_SRGB:
	case GS_CS_SRGB_16F:
		if (current == GS_CS_709_SCRGB) {
			*multiplier = obs_get_video_sdr_white_level() / 80.0f;
			return "DrawMultiply";
		}
		break;
	case GS_CS_709_EXTENDED:
		if (current == GS_CS_SRGB || current == GS_CS_SRGB_16F)
			return "DrawTonemap";
		if (current == GS_CS_709_SCRGB) {
			*multiplier = obs_get_video_sdr_white_level() / 80.0f;
			return "DrawMultiply";
		}
		break;
	case GS_CS_709_SCRGB:
		if (current == GS_CS_SRGB || current == GS_CS_SRGB_16F) {
			*multiplier = 80.0f / obs_get_video_sdr_white_level();
			return "DrawMultiplyTonemap";
		}
		if (current == GS_CS_709_EXTENDED) {
			*multiplier = 80.0f / obs_get_video_sdr_white_level();
			return "DrawMultiply";
		}
		break;
	}

	return "Draw";
}

static void render_target_texture(struct yeocam_grid *g, obs_source_t *target, uint32_t sw, uint32_t sh)
{
	const enum gs_color_space space =
		obs_source_get_color_space(target, OBS_COUNTOF(preferred_spaces), preferred_spaces);
	const enum gs_color_format format = gs_get_format_from_space(space);

	if (!g->texrender || gs_texrender_get_format(g->texrender) != format) {
		gs_texrender_destroy(g->texrender);
		g->texrender = gs_texrender_create(format, GS_ZS_NONE);
	}

	gs_texrender_reset(g->texrender);
	if (gs_texrender_begin_with_color_space(g->texrender, sw, sh, space)) {
		struct vec4 clear_color;
		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, (float)sw, 0.0f, (float)sh, -100.0f, 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		g->rendering = true;
		obs_source_video_render(target);
		g->rendering = false;

		gs_blend_state_pop();
		gs_texrender_end(g->texrender);
	}

	g->tex_space = space;
	g->tex_valid = true;
}

static void draw_background(uint32_t color, uint32_t w, uint32_t h)
{
	if ((color >> 24) == 0)
		return;

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *param = gs_effect_get_param_by_name(solid, "color");

	struct vec4 c;
	vec4_from_rgba_srgb(&c, color);
	gs_effect_set_vec4(param, &c);

	while (gs_effect_loop(solid, "Solid"))
		gs_draw_sprite(NULL, 0, w, h);
}

static void yeocam_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct yeocam_grid *g = data;

	if (g->rendering)
		return;

	obs_source_t *target = get_target(g);
	if (!target)
		return;

	const uint32_t sw = obs_source_get_width(target);
	const uint32_t sh = obs_source_get_height(target);
	if (sw == 0 || sh == 0) {
		obs_source_release(target);
		return;
	}

	if (!g->tex_valid)
		render_target_texture(g, target, sw, sh);
	obs_source_release(target);

	gs_texture_t *tex = gs_texrender_get_texture(g->texrender);
	if (!tex)
		return;

	struct yeocam_cfg cfg;
	pthread_mutex_lock(&g->lock);
	cfg = g->cfg;
	pthread_mutex_unlock(&g->lock);

	uint32_t ow, oh;
	output_size(g, &ow, &oh);

	const struct yeocam_layout_params lp = {
		.count = cfg.count,
		.columns = cfg.columns,
		.rows = cfg.rows,
		.gap = cfg.gap,
		.out_w = (float)ow,
		.out_h = (float)oh,
		.src_w = (float)gs_texture_get_width(tex),
		.src_h = (float)gs_texture_get_height(tex),
		.fit = cfg.fit,
		.focus_x = cfg.focus_x,
		.focus_y = cfg.focus_y,
		.zoom = cfg.zoom,
		.mirror = cfg.mirror,
	};
	const uint32_t n = yeocam_layout_compute(&lp, g->tiles);

	const bool previous_srgb = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	draw_background(cfg.bg_color, ow, oh);

	float multiplier;
	const char *tech = draw_technique(gs_get_color_space(), g->tex_space, &multiplier);

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_effect_set_texture_srgb(gs_effect_get_param_by_name(effect, "image"), tex);
	gs_effect_set_float(gs_effect_get_param_by_name(effect, "multiplier"), multiplier);

	while (gs_effect_loop(effect, tech)) {
		for (uint32_t i = 0; i < n; i++) {
			const struct yeocam_tile *t = &g->tiles[i];
			gs_matrix_push();
			gs_matrix_translate3f(t->x, t->y, 0.0f);
			gs_matrix_scale3f(t->w / (float)t->sw, t->h / (float)t->sh, 1.0f);
			gs_draw_sprite_subregion(tex, t->mirrored ? GS_FLIP_U : 0, t->sx, t->sy, t->sw, t->sh);
			gs_matrix_pop();
		}
	}

	gs_enable_framebuffer_srgb(previous_srgb);
}

struct obs_source_info yeocam_grid_info = {
	.id = YEOCAM_GRID_ID,
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_SRGB,
	.icon_type = OBS_ICON_TYPE_CAMERA,
	.get_name = yeocam_get_name,
	.create = yeocam_create,
	.destroy = yeocam_destroy,
	.update = yeocam_update,
	.get_defaults = yeocam_defaults,
	.get_properties = yeocam_properties,
	.get_width = yeocam_get_width,
	.get_height = yeocam_get_height,
	.video_tick = yeocam_tick,
	.video_render = yeocam_render,
	.enum_active_sources = yeocam_enum_active,
	.video_get_color_space = yeocam_get_color_space,
};
