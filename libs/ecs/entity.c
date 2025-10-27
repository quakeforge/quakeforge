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

uint32_t
ecs_expand_pool (ecs_pool_t *pool, uint32_t count, const component_t *comp)
{
	if (pool->count + count > pool->max_count) {
		pool->max_count += COMP_GROW;
		pool->dense = realloc (pool->dense,
							   pool->max_count * sizeof (uint32_t));
		Component_ResizeArray (comp, &pool->data, pool->max_count);
	}
	uint32_t    ind = pool->count;
	pool->count += count;
	return ind;
}

static void swap_inds (uint32_t *a, uint32_t *b)
{
	uint32_t    t = *a;
	*a = *b;
	*b = t;
}

uint32_t
ecs_move_component (ecs_pool_t *pool, ecs_subpool_t *subpool, uint32_t rangeid,
					uint32_t ind, const component_t *c)
{
	uint32_t    rind = subpool->num_ranges - subpool->available;
	uint32_t    rangeind = subpool->sorted[Ent_Index (rangeid)];
	while (rind-- > rangeind) {
		if (subpool->ranges[rind] == ind) {
			// the range ends at the index, so just grow the range
			// to include the index (will automatically shrink the
			// next range as its start is implict)
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
	return ind;
}

VISIBLE void *
Ent_AddComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry)
{
	ecs_pool_t *pool = &registry->comp_pools[comp];
	ecs_subpool_t *subpool = &registry->subpools[comp];
	component_t *c = &registry->components.a[comp];
	uint32_t    id = Ent_Index (ent);
	if (id >= pool->max_entind) {
		// no - 1 because want id + 1
		uint32_t new_max = (id + ENT_GROW) & ~(ENT_GROW - 1);
		uint32_t   *sparse = pool->sparse;
		sparse = realloc (sparse, sizeof (uint32_t[new_max]));
		memset (sparse + pool->max_entind, nullent,
				sizeof (uint32_t[new_max - pool->max_entind]));
		pool->sparse = sparse;
		pool->max_entind = new_max;
	}
	uint32_t    ind = pool->sparse[id];
	if (ind >= pool->count || pool->dense[ind] != ent) {
		uint32_t    ind = ecs_expand_pool (pool, 1, c);
		pool->sparse[id] = ind;
		pool->dense[ind] = ent;
		uint32_t    rind = subpool->num_ranges - subpool->available;
		if (rind && c->rangeid) {
			uint32_t    rangeid = c->rangeid (registry, ent, comp);
			ecs_move_component (pool, subpool, rangeid, ind, c);
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

	// range is the first index for the next subpool range
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

static void
ent_remove_component (ecs_pool_t *pool, ecs_subpool_t *subpool,
					  uint32_t id, uint32_t ind, const component_t *c)
{
	uint32_t    range_count = subpool->num_ranges - subpool->available;
	// if ind >= the last range, then it is outside the subpools
	if (range_count && ind < subpool->ranges[range_count - 1]) {
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
	uint32_t    last = pool->count - 1;
	if (last > ind) {
		pool->sparse[Ent_Index (pool->dense[last])] = ind;
		pool->dense[ind] = pool->dense[last];
		Component_MoveElements (c, pool->data, ind, last, 1);
	}
	pool->count--;
	pool->sparse[id] = nullent;
}

VISIBLE void
Ent_RemoveComponent (uint32_t ent, uint32_t comp, ecs_registry_t *registry)
{
	if (registry->locked) {
		// the registry is being deleted and mass entity and component
		// deletions are going on
		return;
	}
	uint32_t    id = Ent_Index (ent);
	ecs_pool_t *pool = &registry->comp_pools[comp];
	ecs_subpool_t *subpool = &registry->subpools[comp];
	uint32_t    ind = pool->sparse[id];
	component_t *c = &registry->components.a[comp];
	if (ind < pool->count && pool->dense[ind] == ent) {
		// copy the component out of the pool so that it can be overwritten
		// now, thus removing it from the pool, before actually destroying
		// the component so that any recursive removals of the same component
		// don't mess up the indices, and also don't try to remove the
		// component from the same entity
		byte        tmp_comp[c->size];
		Component_CopyElements (c, tmp_comp, 0, pool->data, ind, 1);

		ent_remove_component (pool, subpool, id, ind, c);

		// the component has been fully removed from the pool so it is now
		// safe to destroy it
		Component_DestroyElements (c, tmp_comp, 0, 1, registry);
	}
}

void Ent_AddGroup (uint32_t ent, uint32_t group, ecs_registry_t *reg)
{
	auto range = ecs_get_subpool_range (&reg->groups.groups, group);
	ecs_grpcomp_t *gc = Component_Address (&ecs_group_components,
										   reg->groups.group_components.data,
										   range.start);
	uint32_t count = range.end - range.start;
	for (uint32_t i = 0; i < count; i++) {
		uint32_t comp = gc[i].component;
		auto c = &reg->components.a[comp];
		auto pool = &reg->comp_pools[comp];
		auto subpool = &reg->subpools[comp];
		uint32_t ind;
		if (Ent_HasComponent (ent, comp, reg)) {
			ind = pool->sparse[Ent_Index (ent)];
			uint32_t range_count = subpool->num_ranges - subpool->available;
			if (ind < subpool->ranges[range_count - 1]) {
				Sys_Error ("component %d (%s) in overlapping group", comp,
						   c->name);
			}
		} else {
			uint32_t id = Ent_Index (ent);
			ind = ecs_expand_pool (pool, 1, c);
			pool->sparse[id] = ind;
			pool->dense[ind] = ent;
			// FIXME: optionally supply data?
			Component_CreateElements (c, pool->data, ind, 1);
		}
		ecs_move_component (pool, subpool, gc[i].rangeid, ind, c);
	}
}

void
Ent_RemoveGroup (uint32_t ent, uint32_t group, ecs_registry_t *reg)
{
	if (ent != nullent) Sys_Error ("not implemented");
	auto range = ecs_get_subpool_range (&reg->groups.groups, group);
	ecs_grpcomp_t *gc = Component_Address (&ecs_group_components,
										   reg->groups.group_components.data,
										   range.start);
	uint32_t count = range.end - range.start;
	for (uint32_t i = 0; i < count; i++) {
		uint32_t comp = gc[i].component;
		auto c = &reg->components.a[comp];
		auto pool = &reg->comp_pools[comp];
		auto subpool = &reg->subpools[comp];
		uint32_t id = Ent_Index (ent);
		uint32_t ind;
		if (!Ent_HasComponent (ent, comp, reg)) {
			Sys_Error ("component %d (%s) not on entity", comp, c->name);
		} else {
			ind = pool->sparse[Ent_Index (ent)];
		}
		byte        tmp_comp[c->size];
		Component_CopyElements (c, tmp_comp, 0, pool->data, ind, 1);

		ent_remove_component (pool, subpool, id, ind, c);
		ind = ecs_expand_pool (pool, 1, c);
		Component_CopyElements (c, pool->data, ind, tmp_comp, 0, 1);
	}
}
