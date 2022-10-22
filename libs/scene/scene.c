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
#include "QF/progs.h"	// for PR_RESMAP
#include "QF/sys.h"
#include "QF/model.h"

#include "QF/scene/entity.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

#include "scn_internal.h"

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
	scene_t    *scene;
	scene_resources_t *res;

	scene = calloc (1, sizeof (scene_t));
	res = calloc (1, sizeof (scene_resources_t));
	*(scene_resources_t **)&scene->resources = res;

	scene->worldmodel = &empty_world;

	return scene;
}

void
Scene_DeleteScene (scene_t *scene)
{
	Scene_FreeAllEntities (scene);

	scene_resources_t *res = scene->resources;
	for (unsigned i = 0; i < res->entities._size; i++) {
		free (res->entities._map[i]);
	}
	free (res->entities._map);

	free (scene->resources);
	free (scene);
}

entity_t *
Scene_CreateEntity (scene_t *scene)
{
	scene_resources_t *res = scene->resources;

	entity_t   *ent = PR_RESNEW (res->entities);
	ent->transform = Transform_New (scene, 0);
	ent->id = PR_RESINDEX (res->entities, ent);

	hierarchy_t *h = ent->transform->ref.hierarchy;
	h->entity[ent->transform->ref.index] = ent;

	QuatSet (1, 1, 1, 1, ent->renderer.colormod);

	return ent;
}

entity_t *
Scene_GetEntity (scene_t *scene, int id)
{
	scene_resources_t *res = scene->resources;
	return PR_RESGET (res->entities, id);
}

static void
destroy_entity (scene_t *scene, entity_t *ent)
{
	scene_resources_t *res = scene->resources;
	// ent->transform will be trampled by the loop below
	transform_t *transform = ent->transform;

	// Transform_Delete takes care of all hierarchy stuff (transforms
	// themselves, name strings, hierarchy table)
	hierarchy_t *h = transform->ref.hierarchy;
	for (size_t i = 0; i < h->num_objects; i++) {
		entity_t   *e = h->entity[0];
		e->transform = 0;
		PR_RESFREE (res->entities, ent);
	}
	Transform_Delete (scene, transform);
}

void
Scene_DestroyEntity (scene_t *scene, entity_t *ent)
{
	scene_resources_t *res = scene->resources;

	if (PR_RESGET (res->entities, ent->id) != ent) {
		Sys_Error ("Scene_DestroyEntity: entity not owned by scene");
	}
	// pull the transform out of the hierarchy to make it easier to destory
	// all the child entities
	Transform_SetParent (scene, ent->transform, 0);
	destroy_entity (scene, ent);
}

void
Scene_FreeAllEntities (scene_t *scene)
{
	while (scene->hierarchies) {
		hierarchy_t *h = scene->hierarchies;
		// deleting the root entity deletes all child entities
		entity_t   *ent = h->entity[0];
		destroy_entity (scene, ent);
	}
	scene_resources_t *res = scene->resources;
	PR_RESRESET (res->entities);
}

transform_t *
Scene_GetTransform (scene_t *scene, int id)
{
	scene_resources_t *res = scene->resources;
	return (transform_t *) PR_RESGET (res->transforms, id);
}
