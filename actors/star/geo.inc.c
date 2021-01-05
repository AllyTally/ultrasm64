#include "src/game/envfx_snow.h"

const GeoLayout star_geo[] = {
	GEO_NODE_START(),
	GEO_OPEN_NODE(),
		GEO_SHADOW(1, 178, 100),
		GEO_OPEN_NODE(),
			GEO_DISPLAY_LIST(LAYER_OPAQUE, star_obj_star_bmdout_mesh),
		GEO_CLOSE_NODE(),
		GEO_DISPLAY_LIST(LAYER_OPAQUE, star_material_revert_render_settings),
	GEO_CLOSE_NODE(),
	GEO_END(),
};
