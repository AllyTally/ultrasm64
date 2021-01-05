#include "src/game/envfx_snow.h"

const GeoLayout transparent_star_geo[] = {
	GEO_NODE_START(),
	GEO_OPEN_NODE(),
		GEO_DISPLAY_LIST(LAYER_TRANSPARENT, transparent_star_blue_starout_mesh),
		GEO_DISPLAY_LIST(LAYER_TRANSPARENT, transparent_star_material_revert_render_settings),
	GEO_CLOSE_NODE(),
	GEO_END(),
};
