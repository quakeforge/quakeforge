/*
	entity.c

	ECS entity management

	Copyright (C) 2022 Bill Currke

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

#include "QF/heapsort.h"
#include "QF/sys.h"

#define IMPLEMENT_ECS_ENTITY_Funcs
#include "QF/ecs.h"

VISIBLE void *
Ent_AddComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry)
{
	ecs_pool_t *pool = &registry->comp_pools[comp];
	uint32_t    id = Ent_Index (ent);
	uint32_t    ind = pool->sparse[id];
	if (ind >= pool->count || pool->dense[ind] != ent) {
		if (pool->count == pool->max_count) {
			pool->max_count += COMP_GROW;
			pool->dense = realloc (pool->dense,
								   pool->max_count * sizeof (uint32_t));
			Component_ResizeArray (&registry->components[comp], &pool->data,
								   pool->max_count);
		}
		uint32_t    ind = pool->count++;
		pool->sparse[id] = ind;
		pool->dense[ind] = ent;
	}
	return Ent_GetComponent (ent, comp, registry);
}

VISIBLE void
Ent_RemoveComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry)
{
	uint32_t    id = Ent_Index (ent);
	ecs_pool_t *pool = &registry->comp_pools[comp];
	uint32_t    ind = pool->sparse[id];
	if (ind < pool->count && pool->dense[ind] == ent) {
		uint32_t    last = pool->count - 1;
		Component_DestroyElements (&registry->components[comp], pool->data,
								   ind, 1);
		if (last > ind) {
			pool->sparse[Ent_Index (pool->dense[last])] = ind;
			pool->dense[ind] = pool->dense[last];
			Component_MoveElements (&registry->components[comp], pool->data,
									ind, last, 1);
		}
		pool->count--;
		pool->sparse[id] = nullent;
	}
}
