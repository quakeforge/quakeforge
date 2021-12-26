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

typedef struct hierarchyset_s DARRAY_TYPE (struct hierarchy_s *)
	hierarchyset_t;
typedef struct visibilityset_s DARRAY_TYPE (struct visibility_s *)
	visibilityset_t;

typedef struct scene_s {
	struct scene_resources_s *const resources;
	hierarchyset_t  roots;
	xformset_t  transforms;
	entityset_t entities;
	visibilityset_t visibility;
} scene_t;

scene_t *Scene_NewScene (void);
struct entity_s *Scene_CreateEntity (scene_t *scene);
void Scene_FreeAllEntities (scene_t *scene);


///@}

#endif//__QF_scene_scene_h
