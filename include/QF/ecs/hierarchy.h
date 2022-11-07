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

#ifndef __QF_ecs_hierarchy_h
#define __QF_ecs_hierarchy_h

#include "QF/qtypes.h"

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

typedef struct hierref_s {
	struct hierarchy_s *hierarchy;
	uint32_t    index;	///< index in hierarchy
} hierref_t;

typedef struct hierarchy_s {
	struct ecs_registry_s *reg;
	uint32_t    num_objects;
	uint32_t    max_objects;
	uint32_t   *ent;
	uint32_t   *childCount;
	uint32_t   *childIndex;
	uint32_t   *parentIndex;
	const hierarchy_type_t *type;
	void      **components;
} hierarchy_t;

hierarchy_t *Hierarchy_New (struct ecs_registry_s *reg,
							const hierarchy_type_t *type, int createRoot);
void Hierarchy_Reserve (hierarchy_t *hierarchy, uint32_t count);
hierarchy_t *Hierarchy_Copy (struct ecs_registry_s *reg,
							 const hierarchy_t *src);
void Hierarchy_Delete (hierarchy_t *hierarchy);

uint32_t Hierarchy_InsertHierarchy (hierarchy_t *dst, const hierarchy_t *src,
									uint32_t dstParent, uint32_t srcRoot);
void Hierarchy_RemoveHierarchy (hierarchy_t *hierarchy, uint32_t index,
								int delEntities);

hierref_t Hierarchy_SetParent (hierarchy_t *dst, uint32_t dstParent,
							   hierarchy_t *src, uint32_t srcIndex);
///@}

#endif//__QF_ecs_hierarchy_h
