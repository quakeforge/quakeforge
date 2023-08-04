/*
	scene.c

	General scene handling

	Copyright (C) 2021 Bill Currke

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/model.h"

#include "QF/plugin/vid_render.h"

#include "QF/scene/entity.h"
#include "QF/scene/light.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

static void
create_active (void *_active)
{
	byte       *active = _active;
	*active = 1;
}

static void
create_old_origin (void *_old_origin)
{
	vec4f_t    *old_origin = _old_origin;
	*old_origin = (vec4f_t) {0, 0, 0, 1};
}

static void
create_colormap (void *_colormap)
{
	colormap_t *colormap = _colormap;
	*colormap = (colormap_t) {1, 6};
}

static void
destroy_visibility (void *_visibility)
{
	visibility_t *visibility = _visibility;
	if (visibility->efrag) {
		R_ClearEfragChain (visibility->efrag);
	}
}

static void
destroy_renderer (void *_renderer)
{
	renderer_t *renderer = _renderer;
	if (renderer->skin) {
		mod_funcs->Skin_Free (renderer->skin);
	}
}

static void
destroy_efrags (void *_efrags)
{
	efrag_t **efrags = _efrags;
	R_ClearEfragChain (*efrags);
}

static void
sw_identity_matrix (void *_mat)
{
	mat4f_t    *mat = _mat;
	mat4fidentity (*mat);
}

static void
sw_frame_0 (void *_frame)
{
	byte       *frame = _frame;
	*frame = 0;
}

static void
sw_null_brush (void *_brush)
{
	struct mod_brush_s **brush = _brush;
	*brush = 0;
}

static const component_t scene_components[scene_comp_count] = {
	[scene_href] = {
		.size = sizeof (hierref_t),
		.create = 0,//create_href,
		.name = "href",
		.destroy = Hierref_DestroyComponent,
	},
	[scene_animation] = {
		.size = sizeof (animation_t),
		.create = 0,//create_animation,
		.name = "animation",
	},
	[scene_visibility] = {
		.size = sizeof (visibility_t),
		.create = 0,//create_visibility,
		.destroy = destroy_visibility,
		.name = "visibility",
	},
	[scene_renderer] = {
		.size = sizeof (renderer_t),
		.create = 0,//create_renderer,
		.destroy = destroy_renderer,
		.name = "renderer",
	},
	[scene_active] = {
		.size = sizeof (byte),
		.create = create_active,
		.name = "active",
	},
	[scene_old_origin] = {
		.size = sizeof (vec4f_t),
		.create = create_old_origin,
		.name = "old_origin",
	},
	[scene_colormap] = {
		.size = sizeof (colormap_t),
		.create = create_colormap,
		.name = "colormap",
	},

	[scene_dynlight] = {
		.size = sizeof (dlight_t),
		.name = "dyn_light",
	},

	[scene_light] = {
		.size = sizeof (light_t),
		.name = "light",
	},
	[scene_efrags] = {
		.size = sizeof (efrag_t *),
		.destroy = destroy_efrags,
		.name = "efrags",
	},
	[scene_lightstyle] = {
		.size = sizeof (uint32_t),
		.name = "lightstyle",
	},
	[scene_lightleaf] = {
		.size = sizeof (uint32_t),
		.name = "lightleaf",
	},
	[scene_lightid] = {
		.size = sizeof (uint32_t),
		.name = "lightid",
	},

	[scene_sw_matrix] = {
		.size = sizeof (mat4f_t),
		.create = sw_identity_matrix,
		.name = "sw world transform",
	},
	[scene_sw_frame] = {
		.size = sizeof (byte),
		.create = sw_frame_0,
		.name = "sw brush model animation frame",
	},
	[scene_sw_brush] = {
		.size = sizeof (struct mod_brush_s *),
		.create = sw_null_brush,
		.name = "sw brush model data pointer",
	},
};

static byte empty_visdata[] = { 0x01 };

static mleaf_t empty_leafs[] = {
	[1] = {
		.contents = CONTENTS_EMPTY,
		.mins = {-INFINITY, -INFINITY, -INFINITY},
		.maxs = { INFINITY,  INFINITY,  INFINITY},
		.compressed_vis = empty_visdata,
	},
};

static mnode_t empty_nodes[] = {
	[0] = {
		.plane = { 0, 0, 0, -1 },
		.type = 3,
		.children = { ~0, ~1 },
		.minmaxs = {-INFINITY, -INFINITY, -INFINITY,
					INFINITY,  INFINITY,  INFINITY},
	},
};

static int empty_node_parents[] = {
	[0] = -1,
};

static int empty_leaf_parents[] = {
	[0] = 0,
	[1] = 0,
};

static int empty_leaf_flags[] = {
	[1] = SURF_DRAWSKY,
};

static char empty_entities[] = { 0 };

static model_t empty_world = {
	.type = mod_brush,
	.radius = INFINITY,
	.mins = {-INFINITY, -INFINITY, -INFINITY},
	.maxs = { INFINITY,  INFINITY,  INFINITY},
	.brush = {
		.modleafs = 2,
		.visleafs = 1,
		.numnodes = 1,
		.nodes = empty_nodes,
		.leafs = empty_leafs,
		.entities = empty_entities,
		.visdata = empty_visdata,
		.node_parents = empty_node_parents,
		.leaf_parents = empty_leaf_parents,
		.leaf_flags = empty_leaf_flags,
	},
};

scene_t *
Scene_NewScene (void)
{
	scene_t    *scene = calloc (1, sizeof (scene_t));

	scene->reg = ECS_NewRegistry ();
	ECS_RegisterComponents (scene->reg, scene_components, scene_comp_count);
	ECS_CreateComponentPools (scene->reg);

	scene->worldmodel = &empty_world;

	return scene;
}

void
Scene_DeleteScene (scene_t *scene)
{
	ECS_DelRegistry (scene->reg);

	free (scene);
}

entity_t
Scene_CreateEntity (scene_t *scene)
{
	// Transform_New creates an entity and adds a scene_href component to the
	// entity
	transform_t trans = Transform_New (scene->reg, nulltransform);
	uint32_t    id = trans.id;

	Ent_SetComponent (id, scene_animation, scene->reg, 0);
	Ent_SetComponent (id, scene_renderer, scene->reg, 0);
	Ent_SetComponent (id, scene_active, scene->reg, 0);
	Ent_SetComponent (id, scene_old_origin, scene->reg, 0);

	renderer_t *renderer = Ent_GetComponent (id, scene_renderer, scene->reg);
	QuatSet (1, 1, 1, 1, renderer->colormod);

	return (entity_t) { .reg = scene->reg, .id = id };
}

void
Scene_DestroyEntity (scene_t *scene, entity_t ent)
{
	ECS_DelEntity (scene->reg, ent.id);
}

void
Scene_FreeAllEntities (scene_t *scene)
{
	auto reg = scene->reg;
	for (uint32_t i = 0; i < reg->num_entities; i++) {
		uint32_t    ent = reg->entities[i];
		uint32_t    ind = Ent_Index (ent);
		if (ind == i) {
			ECS_DelEntity (reg, ent);
		}
	}
}
