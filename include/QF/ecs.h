/*
	ecs.h

	Entity Component System

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

#ifndef __QF_ecs_h
#define __QF_ecs_h

#include <string.h>
#include <stdlib.h>

#include "QF/darray.h"
#include "QF/qtypes.h"
#include "QF/progs.h"//FIXME for PR_RESMAP

#include "QF/ecs/component.h"
#include "QF/ecs/hierarchy.h"

/** \defgroup ecs Entity Component System
	\ingroup utils
*/
///@{

#define ENT_GROW 1024
#define COMP_GROW 128
#define RANGE_GROW 32
#define ENT_IDBITS 20
#define nullent (~0u)

typedef struct ecs_pool_s {
	uint32_t   *sparse;		// indexed by entity id, holds index to dense/data
	uint32_t   *dense;		// holds entity id
	void       *data;		// component data
	uint32_t    count;		// number of components/dense entity ids
	uint32_t    max_count;	// current capacity for components/entity ids
} ecs_pool_t;

typedef struct DARRAY_TYPE(component_t) componentset_t;

typedef struct ecs_range_s {
	uint32_t    start;
	uint32_t    end;
} ecs_range_t;

typedef struct ecs_subpool_s {
	uint32_t   *rangeids;
	uint32_t   *sorted;
	uint32_t   *ranges;
	uint32_t    next;
	uint32_t    available;
	uint32_t    num_ranges;
	uint32_t    max_ranges;
} ecs_subpool_t;

typedef struct ecs_registry_s {
	const char *name;
	ecs_pool_t *comp_pools;
	uint32_t   *entities;
	ecs_subpool_t *subpools;
	uint32_t    next;
	uint32_t    available;
	uint32_t    num_entities;
	uint32_t    max_entities;
	componentset_t components;
	PR_RESMAP (hierarchy_t) hierarchies;//FIXME find a better way
	int         locked;
} ecs_registry_t;

/** Tie an ECS system to a registry.

	Keeps the registry using the system and the system's component base
	together.
*/
typedef struct ecs_system_s {
	ecs_registry_t *reg;
	uint32_t    base;
} ecs_system_t;

#include "QF/ecs/entity.h"

#define ECSINLINE GNU89INLINE inline

ecs_registry_t *ECS_NewRegistry (const char *name);
void ECS_DelRegistry (ecs_registry_t *registry);
uint32_t ECS_RegisterComponents (ecs_registry_t *registry,
								 const component_t *components,
								 uint32_t count);
void ECS_CreateComponentPools (ecs_registry_t *registry);


#ifndef __compar_d_fn_t_defined
#define __compar_d_fn_t_defined
typedef int (*__compar_d_fn_t)(const void *, const void *, void *);
#endif
void ECS_SortComponentPool (ecs_registry_t *registry, uint32_t component,
							__compar_d_fn_t cmp, void *arg);
void ECS_SortComponentPoolRange (ecs_registry_t *registry, uint32_t component,
								 ecs_range_t range,
								 __compar_d_fn_t cmp, void *arg);

uint32_t ECS_NewEntity (ecs_registry_t *registry);
void ECS_DelEntity (ecs_registry_t *registry, uint32_t ent);
void ECS_RemoveEntities (ecs_registry_t *registry, uint32_t component);
void ECS_PrintEntity (ecs_registry_t *registry, uint32_t ent);
void ECS_PrintRegistry (ecs_registry_t *registry);

uint32_t ECS_NewSubpoolRange (ecs_registry_t *registry, uint32_t component);
void ECS_DelSubpoolRange (ecs_registry_t *registry, uint32_t component,
						  uint32_t id);
ECSINLINE ecs_range_t ECS_GetSubpoolRange (ecs_registry_t *registry,
										   uint32_t component, uint32_t id);
void ECS_MoveSubpoolLast (ecs_registry_t *registry, uint32_t component,
						  uint32_t id);

#undef ECSINLINE
#ifndef IMPLEMENT_ECS_Funcs
#define ECSINLINE GNU89INLINE inline
#else
#define ECSINLINE VISIBLE
#endif

ECSINLINE
ecs_range_t
ECS_GetSubpoolRange (ecs_registry_t *registry, uint32_t component, uint32_t id)
{
	ecs_subpool_t *subpool = &registry->subpools[component];
	uint32_t    ind = subpool->sorted[Ent_Index (id)];
	ecs_range_t range = {
		.start = ind ? subpool->ranges[ind - 1] : 0,
		.end = subpool->ranges[ind],
	};
	return range;
}

#undef ECSINLINE

///@}

#endif//__QF_ecs_h
