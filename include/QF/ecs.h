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
	uint32_t    max_entind;	// maximum possible entity id in sparse
} ecs_pool_t;

typedef struct DARRAY_TYPE(component_t) componentset_t;

typedef struct ecs_range_s {
	uint32_t    start;		// first item in range
	uint32_t    end;		// one past last item in range
} ecs_range_t;

typedef struct ecs_subpool_s {
	uint32_t   *rangeids;
	uint32_t   *sorted;		// indexed by range id, holdes index to ranges
	uint32_t   *ranges;		// holds one past end of range. implicit beginning
	uint32_t    next;
	uint32_t    available;
	uint32_t    num_ranges;
	uint32_t    max_ranges;
} ecs_subpool_t;

/// components that are available in every registry
enum {
	ecs_name,				///< const char *
	ecs_hierarchy,			///< hierarchy_t

	ecs_comp_count
};

typedef enum {
	ecs_fullown,
	ecs_partown,
	ecs_nonown,
} ecs_grpown_t;

typedef struct ecs_idpool_s {
	uint32_t   *ids;
	uint32_t    next;
	uint32_t    available;
	uint32_t    num_ids;
	uint32_t    max_ids;
} ecs_idpool_t;

typedef struct ecs_grpcomp_s {
	uint32_t    component;
	uint32_t    rangeid;
} ecs_grpcomp_t;

typedef struct ecs_groups_s {
	ecs_subpool_t groups;
	ecs_subpool_t components;
	ecs_pool_t  group_components;
	ecs_pool_t  component_groups;

	ecs_subpool_t nonown_groups;
	ecs_pool_t  nonown_entities;
} ecs_groups_t;

typedef struct ecs_registry_s {
	const char *name;
	int         locked;
	uint32_t    component_groups_sparse_hack;
	ecs_idpool_t entities;
	ecs_pool_t *comp_pools;
	ecs_subpool_t *subpools;
	componentset_t components;
	ecs_groups_t groups;
} ecs_registry_t;

/** Tie an ECS system to a registry.

	Keeps the registry using the system and the system's component base
	together.
*/
typedef struct ecs_system_s {
	ecs_registry_t *reg;
	uint32_t    base;
} ecs_system_t;

extern const component_t ecs_group_components;
extern const component_t ecs_component_groups;

#define ECSINLINE GNU89INLINE inline

uint32_t ECS_NewId (ecs_idpool_t *idpool);
bool ECS_DelId (ecs_idpool_t *idpool, uint32_t id);
ECSINLINE int ECS_IdValid (ecs_idpool_t *idpool, uint32_t id);
ECSINLINE uint32_t Ent_Index (uint32_t id);
ECSINLINE uint32_t Ent_Generation (uint32_t id);
ECSINLINE uint32_t Ent_NextGen (uint32_t id);

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
ECSINLINE ecs_range_t ecs_get_subpool_range (ecs_subpool_t *subpool,
											 uint32_t id);
ECSINLINE ecs_range_t ECS_GetSubpoolRange (ecs_registry_t *registry,
										   uint32_t component, uint32_t id);
void ECS_MoveSubpoolLast (ecs_registry_t *registry, uint32_t component,
						  uint32_t id);

uint32_t ECS_DefineGroup (ecs_registry_t *reg, uint32_t *components,
						  uint32_t num_components, ecs_grpown_t ownership);

ECSINLINE int ECS_EntValid (uint32_t id, ecs_registry_t *reg);
ECSINLINE int Ent_HasComponent (uint32_t ent, uint32_t comp,
								ecs_registry_t *reg);
ECSINLINE void *Ent_GetComponent (uint32_t ent, uint32_t comp,
								  ecs_registry_t *reg);
ECSINLINE void *Ent_SafeGetComponent (uint32_t ent, uint32_t comp,
									  ecs_registry_t *reg);
ECSINLINE void *Ent_SetComponent (uint32_t ent, uint32_t comp,
								  ecs_registry_t *registry, const void *data);

#undef ECSINLINE
#ifndef IMPLEMENT_ECS_Funcs
#define ECSINLINE GNU89INLINE inline
#else
#define ECSINLINE VISIBLE
#endif

ECSINLINE int
ECS_IdValid (ecs_idpool_t *idpool, uint32_t id)
{
	uint32_t    ind = Ent_Index (id);
	return ind < idpool->num_ids && idpool->ids[ind] == id;
}

ECSINLINE uint32_t
Ent_Index (uint32_t id)
{
	return id & ((1 << ENT_IDBITS) - 1);
}

ECSINLINE uint32_t
Ent_Generation (uint32_t id)
{
	return id & ~((1 << ENT_IDBITS) - 1);
}

ECSINLINE uint32_t
Ent_NextGen(uint32_t id)
{
	return id + (1 << ENT_IDBITS);
}

ECSINLINE ecs_range_t
ecs_get_subpool_range (ecs_subpool_t *subpool, uint32_t id)
{
	uint32_t    ind = subpool->sorted[Ent_Index (id)];
	ecs_range_t range = {
		.start = ind ? subpool->ranges[ind - 1] : 0,
		.end = subpool->ranges[ind],
	};
	return range;
}

ECSINLINE
ecs_range_t
ECS_GetSubpoolRange (ecs_registry_t *registry, uint32_t component, uint32_t id)
{
	ecs_subpool_t *subpool = &registry->subpools[component];
	return ecs_get_subpool_range (subpool, id);
}

ECSINLINE int
ECS_EntValid (uint32_t id, ecs_registry_t *reg)
{
	return ECS_IdValid (&reg->entities, id);
}

ECSINLINE int
Ent_HasComponent (uint32_t ent, uint32_t comp, ecs_registry_t *reg)
{
	ecs_pool_t *pool = &reg->comp_pools[comp];
	uint32_t    entind = Ent_Index (ent);
	if (entind >= pool->max_entind) {
		return false;
	}
	uint32_t    ind = pool->sparse[entind];
	return ind < pool->count && pool->dense[ind] == ent;
}

ECSINLINE void *
Ent_GetComponent (uint32_t ent, uint32_t comp, ecs_registry_t *reg)
{
	const component_t *component = &reg->components.a[comp];
	uint32_t    ind = reg->comp_pools[comp].sparse[Ent_Index (ent)];
	byte       *data = reg->comp_pools[comp].data;
	return Component_Address (component, data, ind);
}

ECSINLINE void *
Ent_SafeGetComponent (uint32_t ent, uint32_t comp, ecs_registry_t *reg)
{
	if (!ECS_EntValid (ent, reg) || !Ent_HasComponent (ent, comp, reg)) {
		return 0;
	}
	return Ent_GetComponent (ent, comp, reg);
}

uint32_t ecs_expand_pool (ecs_pool_t *pool, uint32_t count,
						  const component_t *comp);
uint32_t ecs_move_component (ecs_pool_t *pool, ecs_subpool_t *subpool,
							 uint32_t rangeid, uint32_t ind,
							 const component_t *c);
uint32_t ecs_new_subpool_range (ecs_subpool_t *subpool);

void *Ent_AddComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry);
void Ent_RemoveComponent (uint32_t ent, uint32_t comp,
						  ecs_registry_t *registry);

// NOTE: adds components as needed. Existing components must not be actively
// in another group (use Ent_RemoveGroup first if changing groups)
void Ent_AddGroup (uint32_t ent, uint32_t group, ecs_registry_t *reg);
// NOTE: does not remove the components in the group
void Ent_RemoveGroup (uint32_t ent, uint32_t group, ecs_registry_t *reg);

ECSINLINE void *
Ent_SetComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry,
				  const void *data)
{
	void       *dst = Ent_AddComponent (ent, comp, registry);
	if (data) {
		return Component_CopyElements (&registry->components.a[comp],
									   dst, 0, data, 0, 1);
	} else {
		return Component_CreateElements (&registry->components.a[comp],
										 dst, 0, 1, registry);
	}
}

#undef ECSINLINE

///@}

#endif//__QF_ecs_h
