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

VISIBLE uint32_t
ECS_NewSubpoolRange (ecs_registry_t *registry, uint32_t component)
{
	ecs_subpool_t *subpool = &registry->subpools[component];
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
			size_t      rsize = subpool->max_ranges * sizeof (ecs_range_t);
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

VISIBLE void
ECS_DelSubpoolRange (ecs_registry_t *registry, uint32_t component, uint32_t id)
{
	ecs_subpool_t *subpool = &registry->subpools[component];
	uint32_t    next = subpool->next | Ent_NextGen (Ent_Generation (id));
	uint32_t    ind = Ent_Index (id);
	uint32_t    count = subpool->num_ranges - subpool->available;
	subpool->rangeids[ind] = next;
	subpool->next = ind;
	subpool->available++;
	memmove (subpool->ranges + ind, subpool->ranges + ind + 1,
			 (count - 1 - ind) * sizeof (ecs_range_t));
	for (uint32_t i = 0; i < count; i++) {
		if (subpool->sorted[i] > ind) {
			subpool->sorted[i]--;
		}
	}
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
}
