/*
	hierarch.h

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

#ifndef __QF_scene_hierarch_h
#define __QF_scene_hierarch_h

#include "QF/darray.h"
#include "QF/qtypes.h"
#include "QF/scene/types.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

/** \defgroup entity Hierarchy management
	\ingroup utils
*/
///@{

#define null_transform (~0u)

typedef struct hierarchy_s {
	struct hierarchy_s *next;
	struct hierarchy_s **prev;
	struct scene_s *scene;
	xformset_t  transform;
	entityset_t entity;
	uint32set_t childCount;
	uint32set_t childIndex;
	uint32set_t parentIndex;
	stringset_t name;
	uint32set_t tag;
	byteset_t   modified;
	mat4fset_t  localMatrix;
	mat4fset_t  localInverse;
	mat4fset_t  worldMatrix;
	mat4fset_t  worldInverse;
	vec4fset_t  localRotation;
	vec4fset_t  localScale;
	vec4fset_t  worldRotation;
	vec4fset_t  worldScale;
} hierarchy_t;

hierarchy_t *Hierarchy_New (struct scene_s *scene, int createRoot);
hierarchy_t *Hierarchy_Copy (hierarchy_t *src);
void Hierarchy_Delete (hierarchy_t *hierarchy);

void Hierarchy_UpdateMatrices (hierarchy_t *hierarchy);
uint32_t Hierarchy_InsertHierarchy (hierarchy_t *dst, const hierarchy_t *src,
									uint32_t dstParent, uint32_t srcRoot);
void Hierarchy_RemoveHierarchy (hierarchy_t *hierarchy, uint32_t index);
///@}

#endif//__QF_scene_hierarch_h
