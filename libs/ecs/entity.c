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

#include "compat.h"

static void swap_inds (uint32_t *a, uint32_t *b)
{
	uint32_t    t = *a;
	*a = *b;
	*b = t;
}

VISIBLE void *
Ent_AddComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry)
{
	ecs_pool_t *pool = &registry->comp_pools[comp];
	ecs_subpool_t *subpool = &registry->subpools[comp];
	component_t *c = &registry->components.a[comp];
	uint32_t    id = Ent_Index (ent);
	uint32_t    ind = pool->sparse[id];
	if (ind >= pool->count || pool->dense[ind] != ent) {
		if (pool->count == pool->max_count) {
			pool->max_count += COMP_GROW;
			pool->dense = realloc (pool->dense,
								   pool->max_count * sizeof (uint32_t));
			Component_ResizeArray (c, &pool->data, pool->max_count);
		}
		uint32_t    ind = pool->count++;
		pool->sparse[id] = ind;
		pool->dense[ind] = ent;
		uint32_t    rind = subpool->num_ranges - subpool->available;
		if (rind && c->rangeid) {
			uint32_t    rangeid = c->rangeid (registry, ent, comp);
			uint32_t    rangeind = subpool->sorted[Ent_Index (rangeid)];
			while (rind-- > rangeind) {
				if (subpool->ranges[rind] == ind) {
					subpool->ranges[rind]++;
					continue;
				}
				uint32_t    end = subpool->ranges[rind]++;
				Component_MoveElements (c, pool->data, ind, end, 1);
				swap_inds (&pool->sparse[Ent_Index (pool->dense[end])],
						   &pool->sparse[Ent_Index (pool->dense[ind])]);
				swap_inds (&pool->dense[ind], &pool->dense[end]);
				ind = end;
			}
		}
	}
	return Ent_GetComponent (ent, comp, registry);
}

static int
range_cmp (const void *_key, const void *_range, void *_subpool)
{
	const uint32_t *key = _key;
	const uint32_t *range = _range;
	ecs_subpool_t *subpool = _subpool;

	if (*key >= *range) {
		return 1;
	}
	if (range - subpool->ranges > 0) {
		return *key >= range[-1] ? 0 : -1;
	}
	return 0;
}

static uint32_t *
find_range (ecs_subpool_t *subpool, uint32_t ind)
{
	return bsearch_r (&ind, subpool->ranges,
					  subpool->num_ranges - subpool->available,
					  sizeof (uint32_t), range_cmp, subpool);
}

VISIBLE void
Ent_RemoveComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry)
{
	uint32_t    id = Ent_Index (ent);
	ecs_pool_t *pool = &registry->comp_pools[comp];
	ecs_subpool_t *subpool = &registry->subpools[comp];
	uint32_t    ind = pool->sparse[id];
	component_t *c = &registry->components.a[comp];
	if (ind < pool->count && pool->dense[ind] == ent) {
		uint32_t    last = pool->count - 1;
		Component_DestroyElements (c, pool->data, ind, 1);
		if (subpool->num_ranges - subpool->available) {
			uint32_t    range_count = subpool->num_ranges - subpool->available;
			uint32_t   *range = find_range (subpool, ind);
			while ((size_t) (range - subpool->ranges) < range_count) {
				uint32_t    end = --*range;
				range++;
				if (ind < end) {
					pool->sparse[Ent_Index (pool->dense[end])] = ind;
					pool->dense[ind] = pool->dense[end];
					Component_MoveElements (c, pool->data, ind, end, 1);
					ind = end;
				}
			}
		}
		if (last > ind) {
			pool->sparse[Ent_Index (pool->dense[last])] = ind;
			pool->dense[ind] = pool->dense[last];
			Component_MoveElements (c, pool->data, ind, last, 1);
		}
		pool->count--;
		pool->sparse[id] = nullent;
	}
}
