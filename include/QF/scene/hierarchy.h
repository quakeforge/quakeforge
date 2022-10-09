/*
	hierarchy.h

	Hierarchy management

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

#ifndef __QF_scene_hierarchy_h
#define __QF_scene_hierarchy_h

#include "QF/darray.h"
#include "QF/qtypes.h"
#include "QF/scene/types.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

/** \defgroup entity Hierarchy management
	\ingroup utils
*/
///@{

/** Descriptors for components attached to every entity in the hierarchy.
*/
typedef struct hierarchy_type_s {
	uint32_t    num_components;
	const struct component_s *components;
} hierarchy_type_t;

#define null_transform (~0u)

typedef struct hierref_s {
	struct hierarchy_s *hierarchy;
	uint32_t    index;	///< index in hierarchy
	int32_t     id;		///< scene id
} hierref_t;

typedef struct hierarchy_s {
	struct hierarchy_s *next;
	struct hierarchy_s **prev;
	struct scene_s *scene;
	uint32_t    num_objects;
	uint32_t    max_objects;
	struct transform_s **transform;	//FIXME use hierref_t
	struct entity_s **entity;		//FIXME should not exist
	uint32_t   *childCount;
	uint32_t   *childIndex;
	uint32_t   *parentIndex;
	const hierarchy_type_t *type;
	void      **components;
} hierarchy_t;

hierarchy_t *Hierarchy_New (struct scene_s *scene,
							const hierarchy_type_t *type, int createRoot);
void Hierarchy_Reserve (hierarchy_t *hierarchy, uint32_t count);
hierarchy_t *Hierarchy_Copy (struct scene_s *scene, const hierarchy_t *src);
void Hierarchy_Delete (hierarchy_t *hierarchy);

uint32_t Hierarchy_InsertHierarchy (hierarchy_t *dst, const hierarchy_t *src,
									uint32_t dstParent, uint32_t srcRoot);
void Hierarchy_RemoveHierarchy (hierarchy_t *hierarchy, uint32_t index);
///@}

#endif//__QF_scene_hierarchy_h
