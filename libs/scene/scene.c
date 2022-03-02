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

#include "QF/progs.h"	// for PR_RESMAP
#include "QF/sys.h"

#include "QF/scene/entity.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

#include "scn_internal.h"

scene_t *
Scene_NewScene (void)
{
	scene_t    *scene;
	scene_resources_t *res;

	scene = calloc (1, sizeof (scene_t));
	res = calloc (1, sizeof (scene_resources_t));
	*(scene_resources_t **)&scene->resources = res;

	DARRAY_INIT (&scene->roots, 16);

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

	DARRAY_CLEAR (&scene->roots);

	free (scene->resources);
	free (scene);
}

entity_t *
Scene_CreateEntity (scene_t *scene)
{
	scene_resources_t *res = scene->resources;

	entity_t   *ent = PR_RESNEW_NC (res->entities);
	ent->transform = Transform_New (scene, 0);
	ent->id = PR_RESINDEX (res->entities, ent);

	hierarchy_t *h = ent->transform->hierarchy;
	h->entity.a[ent->transform->index] = ent;

	return ent;
}

entity_t *
Scene_GetEntity (scene_t *scene, int id)
{
	scene_resources_t *res = scene->resources;
	return PR_RESGET (res->entities, id);
}

void
scene_add_root (scene_t *scene, transform_t *transform)
{
	if (!Transform_GetParent (transform)) {
		DARRAY_APPEND (&scene->roots, transform);
	}
}

void
scene_del_root (scene_t *scene, transform_t *transform)
{
	if (!Transform_GetParent (transform)) {
		for (size_t i = 0; i < scene->roots.size; i++) {
			if (scene->roots.a[i] == transform) {
				DARRAY_REMOVE_AT (&scene->roots, i);
				break;
			}
		}
	}
}

static void
destroy_entity (scene_t *scene, entity_t *ent)
{
	scene_resources_t *res = scene->resources;
	// ent->transform will be trampled by the loop below
	transform_t *transform = ent->transform;

	// Transform_Delete takes care of all hierarchy stuff (transforms
	// themselves, name strings, hierarchy table)
	hierarchy_t *h = transform->hierarchy;
	for (size_t i = 0; i < h->entity.size; i++) {
		entity_t   *e = h->entity.a[0];
		e->transform = 0;
		PR_RESFREE (res->entities, ent);
	}
	Transform_Delete (transform);
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
	Transform_SetParent (ent->transform, 0);
	destroy_entity (scene, ent);
}

void
Scene_FreeAllEntities (scene_t *scene)
{
	for (size_t i = 0; i < scene->roots.size; i++) {
		hierarchy_t *h = scene->roots.a[i]->hierarchy;
		// deleting the root entity deletes all child entities
		entity_t   *ent = h->entity.a[0];
		destroy_entity (scene, ent);
	}
	scene_resources_t *res = scene->resources;
	PR_RESRESET (res->entities);
}

transform_t *
Scene_GetTransform (scene_t *scene, int id)
{
	scene_resources_t *res = scene->resources;
	return PR_RESGET (res->transforms, id);
}
