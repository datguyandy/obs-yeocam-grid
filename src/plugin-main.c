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

#include <obs-module.h>
#include <plugin-support.h>

#include "yeocam-grid.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

void yeocam_websocket_register(void);
void yeocam_websocket_unregister(void);

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

bool obs_module_load(void)
{
	obs_register_source(&yeocam_grid_info);
	obs_log(LOG_INFO, "plugin loaded (version %s)", PLUGIN_VERSION);
	return true;
}

/* obs-websocket loads as a module too, so vendor registration waits until
 * every module has loaded. */
void obs_module_post_load(void)
{
	yeocam_websocket_register();
}

void obs_module_unload(void)
{
	yeocam_websocket_unregister();
	obs_log(LOG_INFO, "plugin unloaded");
}
