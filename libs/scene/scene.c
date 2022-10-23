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

#include "QF/scene/component.h"
#include "QF/scene/entity.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

#include "scn_internal.h"

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

static const component_t scene_components[] = {
	[scene_href] = {
		.size = sizeof (hierref_t),
		.create = 0,//create_href,
		.name = "href",
	},
	[scene_animation] = {
		.size = sizeof (animation_t),
		.create = 0,//create_animation,
		.name = "animation",
	},
	[scene_visibility] = {
		.size = sizeof (visibility_t),
		.create = 0,//create_visibility,
		.name = "visibility",
	},
	[scene_renderer] = {
		.size = sizeof (renderer_t),
		.create = 0,//create_renderer,
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
	ECS_RegisterComponents (scene->reg, scene_components, scene_num_components);

	scene_resources_t *res = calloc (1, sizeof (scene_resources_t));
	*(scene_resources_t **)&scene->resources = res;

	scene->worldmodel = &empty_world;

	return scene;
}

void
Scene_DeleteScene (scene_t *scene)
{
	ECS_DelRegistry (scene->reg);

	free (scene->resources);
	free (scene);
}

entity_t
Scene_CreateEntity (scene_t *scene)
{
	transform_t trans = Transform_New (scene, nulltransform);
	uint32_t    id = trans.id;

	Ent_AddComponent (id, scene_href, scene->reg);
	Ent_AddComponent (id, scene_animation, scene->reg);
	Ent_AddComponent (id, scene_visibility, scene->reg);
	Ent_AddComponent (id, scene_renderer, scene->reg);
	Ent_AddComponent (id, scene_active, scene->reg);
	Ent_AddComponent (id, scene_old_origin, scene->reg);

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
}
