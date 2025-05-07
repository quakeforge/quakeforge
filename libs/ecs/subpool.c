/*
	subpool.c

	ECS subpool management

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

#include "QF/sys.h"

#include "QF/ecs.h"

uint32_t
ecs_new_subpool_range (ecs_subpool_t *subpool)
{
	uint32_t    id;
	uint32_t    num_ranges = subpool->num_ranges - subpool->available;
	if (subpool->available) {
		subpool->available--;
		uint32_t    next = subpool->next;
		id = next | Ent_Generation (subpool->rangeids[next]);
		subpool->next = Ent_Index (subpool->rangeids[next]);
		subpool->rangeids[next] = id;
		subpool->sorted[next] = num_ranges;
	} else {
		if (subpool->num_ranges == Ent_Index (nullent)) {
			Sys_Error ("ECS_NewEntity: out of rangeids");
		}
		if (subpool->num_ranges == subpool->max_ranges) {
			subpool->max_ranges += RANGE_GROW;
			size_t      idsize = subpool->max_ranges * sizeof (uint32_t);
			size_t      rsize = subpool->max_ranges * sizeof (uint32_t);
			subpool->rangeids = realloc (subpool->rangeids, idsize);
			subpool->sorted = realloc (subpool->sorted, idsize);
			subpool->ranges = realloc (subpool->ranges, rsize);
		}
		id = subpool->num_ranges++;
		// id starts out with generation 0
		subpool->rangeids[id] = id;
		subpool->sorted[id] = num_ranges;
	}
	uint32_t    end = 0;
	if (num_ranges) {
		end = subpool->ranges[num_ranges - 1];
	}
	subpool->ranges[subpool->sorted[Ent_Index (id)]] = end;
	return id;
}

VISIBLE uint32_t
ECS_NewSubpoolRange (ecs_registry_t *registry, uint32_t component)
{
	ecs_subpool_t *subpool = &registry->subpools[component];
	return ecs_new_subpool_range (subpool);
}

VISIBLE void
ECS_DelSubpoolRange (ecs_registry_t *registry, uint32_t component, uint32_t id)
{
	ecs_subpool_t *subpool = &registry->subpools[component];
	uint32_t    next = subpool->next | Ent_NextGen (Ent_Generation (id));
	uint32_t    ind = Ent_Index (id);
	//uint32_t    count = subpool->num_ranges - subpool->available;
	uint32_t    range = subpool->sorted[ind];
	uint32_t    start = range ? subpool->ranges[range - 1] : 0;
	uint32_t    end = subpool->ranges[range];
	uint32_t    delta = end - start;
	subpool->rangeids[ind] = next;
	subpool->next = ind;
	subpool->available++;
	for (uint32_t i = range; i < subpool->num_ranges - 1; i++) {
		subpool->ranges[i] = subpool->ranges[i + 1] - delta;
	}
	for (uint32_t i = 0; i < subpool->num_ranges; i++) {
		if (subpool->sorted[i] > range) {
			subpool->sorted[i]--;
		}
	}

	ecs_pool_t *pool = &registry->comp_pools[component];
	for (uint32_t i = start; i < end; i++) {
		pool->sparse[Ent_Index (pool->dense[i])] = nullent;
	}
	for (uint32_t i = end; i < pool->count; i++) {
		pool->sparse[Ent_Index (pool->dense[i])] -= delta;
	}
	component_t dc = { .size = sizeof (uint32_t) };
	Component_MoveElements (&dc, pool->dense, start, end, pool->count - end);
	component_t *c = &registry->components.a[component];
	Component_MoveElements (c, pool->data, start, end, pool->count - end);
	pool->count -= delta;
}

VISIBLE void
ECS_MoveSubpoolLast (ecs_registry_t *registry, uint32_t component, uint32_t id)
{
	ecs_pool_t *pool = &registry->comp_pools[component];
	ecs_subpool_t *subpool = &registry->subpools[component];
	uint32_t    ind = Ent_Index (id);
	component_t *c = &registry->components.a[component];
	uint32_t    range = subpool->sorted[ind];
	uint32_t    num_ranges = subpool->num_ranges - subpool->available;
	uint32_t    last_range = num_ranges - 1;

	uint32_t    start = range ? subpool->ranges[range - 1] : 0;
	uint32_t    end = subpool->ranges[range];
	uint32_t    last = subpool->ranges[last_range];
	uint32_t    count = end - start;
	uint32_t    srcIndex = start;
	uint32_t    dstIndex = last - count;
	for (uint32_t i = range; i < last_range; i++) {
		subpool->ranges[i] = subpool->ranges[i + 1] - count;
	}
	for (uint32_t i = 0; i < num_ranges; i++) {
		if (subpool->sorted[i] > range) {
			subpool->sorted[i]--;
		}
	}
	subpool->sorted[ind] = last_range;
	Component_RotateElements (c, pool->data, dstIndex, srcIndex, count);

	if (dstIndex == srcIndex) {
		// didn't move
	} else if (dstIndex < srcIndex) {
		// not expected to happen
		for (uint32_t i = dstIndex; i < srcIndex; i++) {
			pool->sparse[pool->dense[i]] += count;
		}
		for (uint32_t i = srcIndex; i < srcIndex + count; i++) {
			pool->sparse[pool->dense[i]] -= srcIndex - dstIndex;
		}
	} else if (dstIndex < srcIndex + count) {
		for (uint32_t i = srcIndex; i < srcIndex + count; i++) {
			pool->sparse[pool->dense[i]] += dstIndex - srcIndex;
		}
		for (uint32_t i = srcIndex + count; i < dstIndex + count; i++) {
			pool->sparse[pool->dense[i]] -= count;
		}
	} else {
		for (uint32_t i = srcIndex; i < srcIndex + count; i++) {
			pool->sparse[pool->dense[i]] += dstIndex - srcIndex;
		}
		for (uint32_t i = srcIndex + count; i < dstIndex + count; i++) {
			pool->sparse[pool->dense[i]] -= count;
		}
	}

	component_t dc = { .size = sizeof (uint32_t) };
	Component_RotateElements (&dc, pool->dense, dstIndex, srcIndex, count);
}

const component_t ecs_group_components = {
	.size = sizeof (ecs_grpcomp_t),
	.name = "components in group",
};

const component_t ecs_component_groups = {
	.size = sizeof (uint32_t),
	.name = "groups component is in",
};

uint32_t
ECS_DefineGroup (ecs_registry_t *reg, uint32_t *components,
				 uint32_t num_components)
{
	uint32_t gid = ecs_new_subpool_range (&reg->groups.groups);
	for (uint32_t i = 0; i < num_components; i++) {
		if (reg->components.a[components[i]].rangeid) {
			Sys_Error ("adding component with subpools to group");
		}
		uint32_t ind = ecs_expand_pool (&reg->groups.group_components,
										1, &ecs_group_components);
		reg->groups.group_components.dense[ind] = 0;//FIXME see below
		ind = ecs_move_component (&reg->groups.group_components,
								  &reg->groups.groups, gid, ind,
								  &ecs_group_components);
		ecs_grpcomp_t grpcomp = {
			.component = components[i],
			.rangeid = ECS_NewSubpoolRange (reg, components[i]),
		};
		Component_CopyElements (&ecs_group_components,
								reg->groups.group_components.data, ind,
								&grpcomp, 0, 1);
	}
	for (uint32_t i = 0; i < num_components; i++) {
		uint32_t ind = ecs_expand_pool (&reg->groups.component_groups,
										1, &ecs_component_groups);
		reg->groups.component_groups.dense[ind] = 0;//FIXME see commit message
		ind = ecs_move_component (&reg->groups.component_groups,
								  &reg->groups.components, components[i], ind,
								  &ecs_component_groups);
		Component_CopyElements (&ecs_component_groups,
								reg->groups.component_groups.data, ind,
								&gid, 0, 1);
	}

	return gid;
}
