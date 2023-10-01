/*
	scene.h

	Entity management

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/02/26

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

#ifndef __QF_scene_scene_h
#define __QF_scene_scene_h

#include "QF/darray.h"

#include "QF/scene/types.h"

/** \defgroup scene Scene management
	\ingroup utils
*/
///@{

enum scene_components {
	scene_href,			//hierarchical transform
	scene_animation,
	scene_visibility,
	scene_renderer,
	scene_active,
	scene_old_origin,	//XXX FIXME XXX should not be here
	scene_colormap,

	scene_dynlight,

	scene_light,
	scene_efrags,
	scene_lightstyle,
	scene_lightleaf,
	scene_lightid,

	//FIXME these should probably be private to the sw renderer (and in a
	//group, which needs to be implemented), but need to sort out a good
	//scheme for semi-dynamic components
	scene_sw_matrix,	// world transform matrix
	scene_sw_frame,		// animation frame
	scene_sw_brush,		// brush model data pointer

	scene_comp_count
};

typedef struct scene_s {
	struct ecs_registry_s *reg;

	struct model_s *worldmodel;
	int         num_models;
	struct model_s **models;
	struct mleaf_s *viewleaf;
	struct lightingdata_s *lights;
} scene_t;

typedef struct scene_system_s {
	struct ecs_system_s *system;
	const struct component_s *components;
	uint32_t    component_count;
} scene_system_t;

scene_t *Scene_NewScene (scene_system_t *extra_systems);
void Scene_DeleteScene (scene_t *scene);
struct entity_s Scene_CreateEntity (scene_t *scene);
void Scene_DestroyEntity (scene_t *scene, struct entity_s entity);
void Scene_FreeAllEntities (scene_t *scene);


///@}

#endif//__QF_scene_scene_h
