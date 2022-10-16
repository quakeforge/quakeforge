/*
	component.h

	Component management

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/10/07

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

#ifndef __QF_scene_component_h
#define __QF_scene_component_h

#include <string.h>
#include <stdlib.h>

#include "QF/qtypes.h"

/** \defgroup component Entity Component System
	\ingroup utils
*/
///@{

#define ENT_GROW 1024
#define COMP_GROW 128
#define ENT_IDBITS 20

typedef struct ecs_pool_s {
	uint32_t   *sparse;		// indexed by entity id, holds index to dense/data
	uint32_t   *dense;		// holds entity id
	void       *data;		// component data
	uint32_t    count;		// number of components/dense entity ids
	uint32_t    max_count;	// current capacity for components/entity ids
} ecs_pool_t;

typedef struct component_s {
	size_t      size;
	void      (*create) (void *);
	void      (*destroy) (void *);
	const char *name;
} component_t;

typedef struct ecs_registry_s {
	uint32_t   *entities;
	uint32_t    next;
	uint32_t    available;
	uint32_t    num_entities;
	uint32_t    max_entities;
	const component_t *components;
	ecs_pool_t *comp_pools;
	uint32_t    num_components;
} ecs_registry_t;

#define COMPINLINE GNU89INLINE inline

COMPINLINE void Component_ResizeArray (const component_t *component,
									   void **array, uint32_t count);
COMPINLINE void Component_MoveElements (const component_t *component,
										void *array, uint32_t dstIndex,
										uint32_t srcIndex, uint32_t count);
COMPINLINE void Component_CopyElements (const component_t *component,
										void *dstArray, uint32_t dstIndex,
										const void *srcArray, uint32_t srcIndex,
										uint32_t count);
COMPINLINE void Component_CreateElements (const component_t *component,
										  void *array,
										  uint32_t index, uint32_t count);
COMPINLINE void Component_DestroyElements (const component_t *component,
										   void *array,
										   uint32_t index, uint32_t count);
COMPINLINE uint32_t Ent_Index (uint32_t id);
COMPINLINE uint32_t Ent_Generation (uint32_t id);
COMPINLINE uint32_t Ent_NextGen(uint32_t id);

#undef COMPINLINE
#ifndef IMPLEMENT_COMPONENT_Funcs
#define COMPINLINE GNU89INLINE inline
#else
#define COMPINLINE VISIBLE
#endif

COMPINLINE void
Component_ResizeArray (const component_t *component,
					   void **array, uint32_t count)
{
	*array = realloc (*array, count * component->size);
}

COMPINLINE void
Component_MoveElements (const component_t *component,
						void *array, uint32_t dstIndex, uint32_t srcIndex,
						uint32_t count)
{
	__auto_type dst = (byte *) array + dstIndex * component->size;
	__auto_type src = (byte *) array + srcIndex * component->size;
	memmove (dst, src, count * component->size);
}

COMPINLINE void
Component_CopyElements (const component_t *component,
						void *dstArray, uint32_t dstIndex,
						const void *srcArray, uint32_t srcIndex,
						uint32_t count)
{
	__auto_type dst = (byte *) dstArray + dstIndex * component->size;
	__auto_type src = (byte *) srcArray + srcIndex * component->size;
	memcpy (dst, src, count * component->size);
}

COMPINLINE void
Component_CreateElements (const component_t *component, void *array,
						  uint32_t index, uint32_t count)
{
	if (component->create) {
		for (uint32_t i = index; count-- > 0; i++) {
			__auto_type dst = (byte *) array + i * component->size;
			component->create (dst);
		}
	} else {
		__auto_type dst = (byte *) array + index * component->size;
		memset (dst, 0, count * component->size);
	}
}

COMPINLINE void
Component_DestroyElements (const component_t *component, void *array,
						   uint32_t index, uint32_t count)
{
	if (component->destroy) {
		for (uint32_t i = index; count-- > 0; i++) {
			__auto_type dst = (byte *) array + i * component->size;
			component->destroy (dst);
		}
	}
}

COMPINLINE uint32_t
Ent_Index (uint32_t id)
{
	return id & ((1 << ENT_IDBITS) - 1);
}

COMPINLINE uint32_t
Ent_Generation (uint32_t id)
{
	return id & ~((1 << ENT_IDBITS) - 1);
}

COMPINLINE uint32_t
Ent_NextGen(uint32_t id)
{
	return id + (1 << ENT_IDBITS);
}

ecs_registry_t *ECS_NewRegistry (void);
void ECS_DelRegistry (ecs_registry_t *registry);
void ECS_RegisterComponents (ecs_registry_t *registry,
							 const component_t *components, uint32_t count);

uint32_t ECS_NewEntity (ecs_registry_t *registry);
void ECS_DelEntity (ecs_registry_t *registry, uint32_t ent);

void Ent_AddComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry);
void Ent_RemoveComponent (uint32_t ent, uint32_t comp,
						  ecs_registry_t *registry);

///@}

#endif//__QF_scene_component_h
