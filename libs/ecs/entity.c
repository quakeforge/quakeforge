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

void
ecs_expand_sparse (ecs_pool_t *pool, uint32_t id)
{
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
}

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

uint32_t
ecs_set_component (uint32_t ent, ecs_pool_t *pool, const component_t *c,
				   void *data)
{
	uint32_t    id = Ent_Index (ent);
	ecs_expand_sparse (pool, id);
	uint32_t    ind = ecs_expand_pool (pool, 1, c);
	pool->sparse[id] = ind;
	pool->dense[ind] = ent;
	auto dst = Component_Address (c, pool->data, ind);
	Component_CopyElements (c, dst, 0, data, 0, 1);
	return ind;
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
	byte        tmp[c->size];
	bool        write = false;
	uint32_t    rind = subpool->num_ranges - subpool->available;
	uint32_t    rangeind = rind;

	if (rangeid != nullent) {
		// moving to another range in the subpool
		rangeind = subpool->sorted[Ent_Index (rangeid)];
	}

	if (rind && ind < subpool->ranges[rind - 1]) {
		rind = find_range (subpool, ind) - subpool->ranges;
	}
	while (rind < rangeind) {
		if (subpool->ranges[rind] == ind + 1) {
			// the range ends just past the index, so just shrink
			// the range to expclude the index (will automatically
			// grow the next range as its start is implict)
			subpool->ranges[rind]--;
			rind++;
			continue;
		}
		if (!write) {
			write = true;
			Component_CopyElements (c, tmp, 0, pool->data, ind, 1);
		}
		uint32_t    end = --subpool->ranges[rind];
		rind++;
		Component_MoveElements (c, pool->data, ind, end, 1);
		swap_inds (&pool->sparse[Ent_Index (pool->dense[end])],
				   &pool->sparse[Ent_Index (pool->dense[ind])]);
		swap_inds (&pool->dense[ind], &pool->dense[end]);
		ind = end;
	}
	while (rind-- > rangeind) {
		if (subpool->ranges[rind] == ind) {
			// the range ends at the index, so just grow the range
			// to include the index (will automatically shrink the
			// next range as its start is implict)
			subpool->ranges[rind]++;
			continue;
		}
		if (!write) {
			write = true;
			Component_CopyElements (c, tmp, 0, pool->data, ind, 1);
		}
		uint32_t    end = subpool->ranges[rind]++;
		Component_MoveElements (c, pool->data, ind, end, 1);
		swap_inds (&pool->sparse[Ent_Index (pool->dense[end])],
				   &pool->sparse[Ent_Index (pool->dense[ind])]);
		swap_inds (&pool->dense[ind], &pool->dense[end]);
		ind = end;
	}
	if (write) {
		Component_CopyElements (c, pool->data, ind, tmp, 0, 1);
	}
	return ind;
}

static bool
ent_test_group (ecs_registry_t *reg, uint32_t gid, uint32_t ent)
{
	auto g = &reg->groups;
	auto range = ecs_get_subpool_range (&g->groups, gid);
	ecs_grpcomp_t *gc = Component_Address (&ecs_group_components,
										   g->group_components.data,
										   range.start);
	for (uint32_t i = range.end - range.start; i-- > 0; gc++) {
		if (!Ent_HasComponent (ent, gc->component, reg)) {
			return false;
		}
	}
	return true;
}

static void
ent_try_add_group (ecs_registry_t *reg, uint32_t gid, uint32_t ent)
{
	if (!ent_test_group (reg, gid, ent)) {
		return;
	}
	auto g = &reg->groups;
	auto type = ecs_get_group_type (gid, reg);
	if (type == ecs_nonown) {
		// the group doesn't own any of the components, so record the entity
		// in the group's list of entities so it can be iterated by group
		auto nid = *(uint32_t *) ecs_get_component (gid, &g->nonown_ranges,
													&ecs_group_nonown_range);
		auto pool = &g->nonown_entities;
		auto subpool = &g->nonown_groups;
		auto c = &ecs_group_nonown_entity;
		uint32_t ind = ecs_expand_pool (pool, 1, c);
		auto dst = Component_Address (c, pool->data, ind);
		Component_CopyElements (c, dst, 0, &ent, 0, 1);
		ecs_move_component (pool, subpool, nid, ind, c);
	} else {
		auto range = ecs_get_subpool_range (&g->groups, gid);
		ecs_grpcomp_t *gc = Component_Address (&ecs_group_components,
											   g->group_components.data,
											   range.start);
		uint32_t count = range.end - range.start;
		for (uint32_t i = 0; i < count; i++) {
			if (gc[i].rangeid == nullent) {
				// group doesn't own the component, so can't move it
				continue;
			}
			uint32_t comp = gc[i].component;
			auto c = &reg->components.a[comp];
			auto pool = &reg->comp_pools[comp];
			auto subpool = &reg->subpools[comp];
			uint32_t ind = pool->sparse[Ent_Index (ent)];
			ecs_move_component (pool, subpool, gc[i].rangeid, ind, c);
		}
	}
}

VISIBLE void *
Ent_AddComponent (uint32_t ent, uint32_t comp, ecs_registry_t *reg)
{
	ecs_pool_t *pool = &reg->comp_pools[comp];
	ecs_subpool_t *subpool = &reg->subpools[comp];
	component_t *c = &reg->components.a[comp];
	uint32_t    id = Ent_Index (ent);
	ecs_expand_sparse (pool, id);
	uint32_t    ind = pool->sparse[id];
	if (ind >= pool->count || pool->dense[ind] != ent) {
		uint32_t    ind = ecs_expand_pool (pool, 1, c);
		pool->sparse[id] = ind;
		pool->dense[ind] = ent;
		uint32_t    rangeid = nullent;
		if (set_is_member (&reg->groups.grouped_components, comp)) {
			auto g = &reg->groups;
			auto range = ecs_get_subpool_range (&g->components, comp);
			uint32_t *gid = Component_Address (&ecs_component_groups,
											   g->component_groups.data,
											   range.start);
			// go through the groups in reverse order so partially or fully
			// owning groups are done from smallest to largest and non-owning
			// groups are done first
			for (uint32_t i = range.end - range.start; i-- > 0; ) {
				ent_try_add_group (reg, gid[i], ent);
			}
		} else {
			uint32_t    rind = subpool->num_ranges - subpool->available;
			if (rind && c->rangeid) {
				rangeid = c->rangeid (reg, ent, comp);
			}
		}
		if (rangeid != nullent) {
			ecs_move_component (pool, subpool, rangeid, ind, c);
		}
	}
	return Ent_GetComponent (ent, comp, reg);
}

static void
ent_remove_group (ecs_registry_t *reg, uint32_t gid, uint32_t ent, uint32_t ind,
				  set_t *done_components)
{
	if (!ent_test_group (reg, gid, ent)) {
		return;
	}
	uint32_t id = Ent_Index (ent);
	auto g = &reg->groups;
	// move other owned components out of group
	auto range = ecs_get_subpool_range (&g->groups, gid);
	ecs_grpcomp_t *gc = Component_Address (&ecs_group_components,
										   g->group_components.data,
										   range.start);
	uint32_t count = range.end - range.start;
	for (; count-- > 0; gc++) {
		if (gc->rangeid != nullent) {
			// the component is owned by this group
			if (set_is_member (done_components, gc->component)) {
				continue;
			}
			set_add (done_components, gc->component);
			auto p = &reg->comp_pools[gc->component];
			auto sp = &reg->subpools[gc->component];
			auto c = &reg->components.a[gc->component];
			if (ind != p->sparse[id]) {
				Sys_Error ("misaligned components in owning group");
			}
			ecs_move_component (p, sp, nullent, ind, c);
		}
	}
}

static void
ent_remove_raw_component (uint32_t ind, const component_t *c,
						  ecs_pool_t *pool, ecs_subpool_t *subpool)
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
}

static void
ent_remove_nonown_group (ecs_registry_t *reg, uint32_t gid, uint32_t ent)
{
	if (!ent_test_group (reg, gid, ent)) {
		return;
	}
	auto g = &reg->groups;
	auto pool = &g->nonown_entities;
	auto subpool = &g->nonown_groups;
	auto nid = *(uint32_t *) ecs_get_component (gid, &g->nonown_ranges,
												&ecs_group_nonown_range);
	auto ent_range = ecs_get_subpool_range (subpool, nid);
	auto c = &ecs_group_nonown_entity;
	uint32_t ent_count = ent_range.end - ent_range.start;
	uint32_t *ent_list = Component_Address (c, pool->data, ent_range.start);
	for (uint32_t i = 0; i < ent_count; i++) {
		if (ent_list[i] == ent) {
			uint32_t ind = ent_range.start + i;
			ent_remove_raw_component (ind, c, pool, subpool);
			return;
		}
	}
	Sys_Error ("entity not in non-owning group");
}

static void
ent_remove_component (uint32_t ent, ecs_registry_t *reg, uint32_t comp)
{
	auto pool = &reg->comp_pools[comp];
	auto subpool = &reg->subpools[comp];
	auto g = &reg->groups;
	uint32_t    id = Ent_Index (ent);
	uint32_t    ind = pool->sparse[id];
	uint32_t    range_count = subpool->num_ranges - subpool->available;
	if (set_is_member (&reg->groups.grouped_components, comp)) {
		if (!range_count) {
			Sys_Error ("no ranges on grouped component");
		}
		set_t done_components = SET_STATIC_INIT (reg->components.size, alloca);
		set_empty (&done_components);
		// prevent this component from getting moved so it can be handled
		// by the non-grouped component code
		set_add (&done_components, comp);
		// get the list of all groups using this component
		auto range = ecs_get_subpool_range (&g->components, comp);
		uint32_t *cg = Component_Address (&ecs_component_groups,
										 g->component_groups.data,
										 range.start);
		if (ind < subpool->ranges[range_count - 1]) {
			// the component/entity is in an owning group
			uint32_t rind = find_range (subpool, ind) - subpool->ranges;
			// component groups are sorted such that fully owning groups
			// come first and are sorted by size, same as the ranges in
			// the pools, thus the range index also indexes the group
			uint32_t gid = cg[rind];
			ent_remove_group (reg, gid, ent, ind, &done_components);
		}
		uint32_t count = range.end - range.start;
		if (count < range_count) {
			Sys_Error ("extra ranges on grouped component");
		}
		// skip over owning groups
		cg += range_count;
		count -= range_count;
		// go through all partially and non-owning groups
		for (; count-- > 0; cg++) {
			uint32_t gid = *cg;
			auto type = ecs_get_group_type (gid, reg);
			if (type == ecs_nonown) {
				ent_remove_nonown_group (reg, gid, ent);
			} else {
				ent_remove_group (reg, gid, ent, ind, &done_components);
			}
		}
	}
	component_t *c = &reg->components.a[comp];
	ent_remove_raw_component (ind, c, pool, subpool);
	pool->sparse[id] = nullent;
}

VISIBLE void
Ent_RemoveComponent (uint32_t ent, uint32_t comp, ecs_registry_t *reg)
{
	if (reg->locked) {
		// the registry is being deleted and mass entity and component
		// deletions are going on
		return;
	}
	uint32_t    id = Ent_Index (ent);
	ecs_pool_t *pool = &reg->comp_pools[comp];
	if (id >= pool->max_entind) {
		return;
	}
	uint32_t    ind = pool->sparse[id];
	component_t *c = &reg->components.a[comp];
	if (ind < pool->count && pool->dense[ind] == ent) {
		// copy the component out of the pool so that it can be overwritten
		// now, thus removing it from the pool, before actually destroying
		// the component so that any recursive removals of the same component
		// don't mess up the indices, and also don't try to remove the
		// component from the same entity
		byte        tmp_comp[c->size];
		Component_CopyElements (c, tmp_comp, 0, pool->data, ind, 1);

		ent_remove_component (ent, reg, comp);

		// the component has been fully removed from the pool so it is now
		// safe to destroy it
		Component_DestroyElements (c, tmp_comp, 0, 1, &ent, reg);
	}
}

void Ent_AddGroup (uint32_t ent, uint32_t group, ecs_registry_t *reg)
{
	auto g = &reg->groups;
	auto gtype = ecs_get_group_type (group, reg);
	if (gtype == ecs_nonown) {
		// the group doesn't own any of the components, so record the entity
		// in the group's list of entities so it can be iterated by group
		auto nid = *(uint32_t *) ecs_get_component (group, &g->nonown_ranges,
													&ecs_group_nonown_range);
		auto pool = &g->nonown_entities;
		auto subpool = &g->nonown_groups;
		auto c = &ecs_group_nonown_entity;
		uint32_t ind = ecs_expand_pool (pool, 1, c);
		auto dst = Component_Address (c, pool->data, ind);
		Component_CopyElements (c, dst, 0, &ent, 0, 1);
		ecs_move_component (pool, subpool, nid, ind, c);
		return;
	}
	auto range = ecs_get_subpool_range (&g->groups, group);
	ecs_grpcomp_t *gc = Component_Address (&ecs_group_components,
										   g->group_components.data,
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
			ecs_expand_sparse (pool, id);
			ind = ecs_expand_pool (pool, 1, c);
			pool->sparse[id] = ind;
			pool->dense[ind] = ent;
			// FIXME: optionally supply data?
			Component_CreateElements (c, pool->data, ind, 1, &ent, reg);
		}
		if (gc[i].rangeid != nullent) {
			ecs_move_component (pool, subpool, gc[i].rangeid, ind, c);
		}
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
		uint32_t ind;
		if (!Ent_HasComponent (ent, comp, reg)) {
			Sys_Error ("component %d (%s) not on entity", comp, c->name);
		} else {
			ind = pool->sparse[Ent_Index (ent)];
		}
		byte        tmp_comp[c->size];
		Component_CopyElements (c, tmp_comp, 0, pool->data, ind, 1);

		ent_remove_component (ent, reg, comp);
		ind = ecs_expand_pool (pool, 1, c);
		Component_CopyElements (c, pool->data, ind, tmp_comp, 0, 1);
	}
}
