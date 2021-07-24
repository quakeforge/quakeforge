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

#include "QF/scene/entity.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

typedef struct scene_resources_s {
	PR_RESMAP (entity_t) entities;
} scene_resources_t;

scene_t *
Scene_NewScene (void)
{
	scene_t    *scene;
	scene_resources_t *res;

	scene = calloc (1, sizeof (scene_t));
	res = calloc (1, sizeof (scene_resources_t));
	*(scene_resources_t **)&scene->resources = res;

	DARRAY_INIT (&scene->roots, 16);
	DARRAY_INIT (&scene->transforms, 16);
	DARRAY_INIT (&scene->entities, 16);
	DARRAY_INIT (&scene->visibility, 16);

	return scene;
}

entity_t *
Scene_CreateEntity (scene_t *scene)
{
	scene_resources_t *res = scene->resources;

	entity_t   *ent = PR_RESNEW_NC (res->entities);
	ent->transform = 0;
	DARRAY_APPEND (&scene->entities, ent);
	return ent;
}

void
Scene_FreeAllEntities (scene_t *scene)
{
	scene_resources_t *res = scene->resources;
	for (size_t i = 0; i < scene->entities.size; i++) {
		entity_t   *ent = scene->entities.a[i];
		if (ent->transform) {
			Transform_Delete (ent->transform);
			ent->transform = 0;
		}
	}
	PR_RESRESET (res->entities);
}
