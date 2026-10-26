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

#include "QF/ecs.h"
#include "QF/heapsort.h"
#include "QF/set.h"
#include "QF/sys.h"

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
			subpool->dense = realloc (subpool->dense, rsize);
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
	subpool->dense[subpool->sorted[Ent_Index (id)]] = id;
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
		subpool->dense[i] = subpool->dense[i + 1];
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
	uint32_t move_count = pool->count - end;
	if (move_count) {
		component_t dc = { .size = sizeof (uint32_t) };
		Component_MoveElements (&dc, pool->dense, start, end, move_count);
		component_t *c = &registry->components.a[component];
		Component_MoveElements (c, pool->data, start, end, move_count);
	}
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
	// remove the specified range from the list of ranges, moving all later
	// ranges down one slot
	for (uint32_t i = range; i < last_range; i++) {
		subpool->ranges[i] = subpool->ranges[i + 1] - count;
		subpool->dense[i] = subpool->dense[i + 1];
	}
	// ranges[last_range] doesn't change
	subpool->dense[last_range] = id;
	// update the indices of all range ids that point past the moved range
	for (uint32_t i = 0; i < num_ranges; i++) {
		if (subpool->sorted[i] > range) {
			subpool->sorted[i]--;
		}
	}
	// point the specified range at the last range
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

const component_t ecs_group_type = {
	.size = sizeof (ecs_grpown_t),
	.name = "Group type",
};

const component_t ecs_group_nonown_range = {
	.size = sizeof (uint32_t),
	.name = "Group nonown range",
};

const component_t ecs_group_nonown_entity = {
	.size = sizeof (uint32_t),
	.name = "Group nonown entity",
};

const component_t ecs_component_groups = {
	.size = sizeof (uint32_t),
	.name = "groups component is in",
};

static set_flags_t
ecs_test_groups (uint32_t *components, uint32_t num_components,
				 ecs_grpcomp_t *grpcomps, uint32_t num_grpcomps,
				 set_t *owned, ecs_registry_t *reg)
{
	set_t a = SET_STATIC_INIT (reg->components.size, alloca);
	set_t b = SET_STATIC_INIT (reg->components.size, alloca);
	set_empty (&a);
	set_empty (&b);
	for (uint32_t i = 0; i < num_components; i++) {
		set_add (&a, components[i]);
	}
	for (uint32_t i = 0; i < num_grpcomps; i++) {
		set_add (&b, grpcomps[i].component);
		set_remove (owned, grpcomps[i].component);
	}
	return set_test (&a, &b);
}

static ecs_grpown_t
ecs_calc_group_type (ecs_registry_t *reg, uint32_t *components,
					 uint32_t num_components, set_t *owned_components)
{
	auto grps = &reg->groups.groups;
	uint32_t num_groups = grps->num_ranges - grps->available;
	set_t groups = SET_STATIC_INIT (num_groups, alloca);
	set_empty (&groups);

	// build the set of all groups on the components in the list
	for (uint32_t i = 0; i < num_components; i++) {
		auto range = ecs_get_subpool_range (&reg->groups.components,
											components[i]);
		auto cgrp = &reg->groups.component_groups;
		uint32_t *g = Component_Address (&ecs_component_groups, cgrp->data,
										 range.start);
		uint32_t count = range.end - range.start;
		while (count-- > 0) {
			set_add (&groups, *g++);
		}
		set_add (owned_components, components[i]);
	}
	// Test the proposed group against each of the groups already associated
	// with the components in the proposed group
	// FIXME incomplete: need to figure out the conditions and finish the
	// implementation
	bool dup = false;
	bool full = true;
	for (auto g = set_first (&groups); g; g = set_next (g)) {
		auto range = ecs_get_subpool_range (&reg->groups.groups, g->element);
		auto gcmp = &reg->groups.group_components;
		ecs_grpcomp_t *gc = Component_Address (&ecs_group_components,
											   gcmp->data, range.start);
		uint32_t count = range.end - range.start;
		auto test = ecs_test_groups (components, num_components,
									 gc, count, owned_components, reg);
		dup |= !(test & set_differ);
		full &= !(test == (set_differ | set_intersect));
	}
	if (dup) {
		Sys_Error ("group duplicates another group");
	}
	// can't checked owned_components for full ownership because nested
	// groups will remove some or all components
	return full ? ecs_fullown
				: set_count (owned_components) ? ecs_partown
												: ecs_nonown;
}

uint32_t
ECS_DefineGroup (ecs_registry_t *reg, uint32_t *components,
				 uint32_t num_components, ecs_grpown_t ownership)
{
	set_t owned_components = SET_STATIC_INIT (num_components, alloca);
	set_empty (&owned_components);
	auto type = ecs_calc_group_type (reg, components, num_components,
									 &owned_components);
	uint32_t gid = ecs_new_subpool_range (&reg->groups.groups);
	ecs_set_component (gid, &reg->groups.group_type, &ecs_group_type, &type);
	// Create subpool ranges for the group in each of the components' pools
	// records { component_id, range_id }, maintaining the order of the
	// components in the provided list
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
		// create subpool range for the group in the component's pool
		uint32_t rangeid = nullent;
		// fully-owning groups will have had components removed from
		// owned_components if they're nested
		if (type == ecs_fullown
			|| set_is_member (&owned_components, components[i])) {
			rangeid = ECS_NewSubpoolRange (reg, components[i]);
		}
		ecs_grpcomp_t grpcomp = {
			.component = components[i],
			.rangeid = rangeid,
		};
		Component_CopyElements (&ecs_group_components,
								reg->groups.group_components.data, ind,
								&grpcomp, 0, 1);
	}
	if (type == ecs_nonown) {
		uint32_t nid = ecs_new_subpool_range (&reg->groups.nonown_groups);
		ecs_expand_sparse (&reg->groups.nonown_ranges, gid);
		ecs_set_component (gid, &reg->groups.nonown_ranges,
						   &ecs_group_nonown_range, &nid);
	}
	// add the new group id to each of the components
	for (uint32_t i = 0; i < num_components; i++) {
		uint32_t ind = ecs_expand_pool (&reg->groups.component_groups,
										1, &ecs_component_groups);
		reg->groups.component_groups.dense[ind] = 0;//FIXME see commit message
		// the component id is the subrange id. The subrange was created when
		// the component pools were finalized in ECS_CreateComponentPools
		ind = ecs_move_component (&reg->groups.component_groups,
								  &reg->groups.components, components[i], ind,
								  &ecs_component_groups);
		Component_CopyElements (&ecs_component_groups,
								reg->groups.component_groups.data, ind,
								&gid, 0, 1);
		set_add (&reg->groups.grouped_components, components[i]);
	}

	return gid;
}

static ecs_grpown_t
ecs_get_group_type (uint32_t gid, ecs_registry_t *reg)
{
	auto g = &reg->groups;
	return *(ecs_grpown_t *) ecs_get_component (gid, &g->group_type,
												&ecs_group_type);
}

static int
ecs_range_cmp (const void *_a, const void *_b, void *_subpool)
{
	const uint32_t *a = _a;
	const uint32_t *b = _b;
	ecs_subpool_t *subpool = _subpool;
	return subpool->sorted[*a] - subpool->sorted[*b];
}

typedef struct ecs_grpctx_s {
	uint32_t *groups;
	ecs_subpool_t *subpool;
	ecs_registry_t *reg;
} ecs_grpctx_t;

static int
ecs_group_cmp (const void *_a, const void *_b, void *_ctx)
{
	const uint32_t *a = _a;
	const uint32_t *b = _b;
	ecs_grpctx_t *ctx = _ctx;
	auto g = &ctx->reg->groups;
	auto ra = ecs_get_subpool_range (&g->groups, *a);
	auto rb = ecs_get_subpool_range (&g->groups, *b);
	return (rb.end - rb.start) - (ra.end - ra.start);
}

static int
ecs_comp_group_cmp (const void *_a, const void *_b, void *_g)
{
	const uint32_t *gida = _a;
	const uint32_t *gidb = _b;
	ecs_groups_t *g = _g;
	auto typea = *(ecs_grpown_t *) ecs_get_component (*gida, &g->group_type,
													  &ecs_group_type);
	auto typeb = *(ecs_grpown_t *) ecs_get_component (*gidb, &g->group_type,
													  &ecs_group_type);
	auto rangea = ecs_get_subpool_range (&g->groups, *gida);
	auto rangeb = ecs_get_subpool_range (&g->groups, *gidb);
	if (typea == typeb) {
		uint32_t counta = rangea.end - rangea.start;
		uint32_t countb = rangeb.end - rangeb.start;
		if (counta == countb) {
			return gida - gidb;		// doesn't matter, just for stability
		}
		return countb - counta;		// want larger first
	}
	return typea - typeb;			// want fully-owning first
}

static void
swap_uint32 (uint32_t *a, uint32_t *b)
{
	uint32_t t = *a;
	*a = *b;
	*b = t;
}

static void
ecs_group_swp (void *_a, void *_b, void *_ctx)
{
	uint32_t *a = _a;
	uint32_t *b = _b;
	ecs_grpctx_t *ctx = _ctx;

	swap_uint32 (a, b);
	swap_uint32 (&ctx->subpool->dense[*a], &ctx->subpool->dense[*b]);
	swap_uint32 (&ctx->subpool->sorted[*a], &ctx->subpool->sorted[*b]);
}

void
ECS_SortGroups (ecs_registry_t *reg)
{
	auto g = &reg->groups;
	uint32_t num_groups = g->groups.num_ranges - g->groups.available;
	set_t sorted_groups = SET_STATIC_INIT (num_groups, alloca);
	set_t groups = SET_STATIC_INIT (num_groups, alloca);

	set_empty (&sorted_groups);
	for (uint32_t gid = 0; gid < num_groups; gid++) {
		if (ecs_get_group_type (gid, reg) != ecs_fullown) {
			continue;
		}
		if (set_is_member (&sorted_groups, gid)) {
			continue;
		}

		set_empty (&groups);
		auto range = ecs_get_subpool_range (&g->groups, gid);
		ecs_grpcomp_t *gc = Component_Address (&ecs_group_components,
											   g->group_components.data,
											   range.start);
		uint32_t count = range.end - range.start;
		for (uint32_t i = 0; i < count; i++) {
			uint32_t comp = gc[i].component;
			auto subpool = &reg->subpools[comp];
			auto crange = ecs_get_subpool_range (&g->components, comp);
			auto cg = &g->component_groups;
			uint32_t *grp = Component_Address (&ecs_component_groups, cg->data,
											   range.start);
			for (uint32_t c = crange.end - crange.start; c-- > 0; grp++) {
				if (ecs_get_group_type (*grp, reg) != ecs_fullown) {
					continue;
				}
				set_add (&groups, *grp);
			}
			uint32_t group_ids[set_count (&groups)];
			auto gp = group_ids;
			for (auto gi = set_first (&groups); gi; gi = set_next (gi)) {
				*gp++ = gi->element;
			}
			heapsort_r (group_ids, set_count (&groups), sizeof (uint32_t),
						ecs_range_cmp, subpool);
			ecs_grpctx_t ctx = {
				.groups = group_ids,
				.subpool = subpool,
				.reg = reg,
			};
			heapsort_s (group_ids, set_count (&groups), sizeof (uint32_t),
						ecs_group_cmp, ecs_group_swp, &ctx);
		}

		set_union (&sorted_groups, &groups);
	}

	for (auto ci = set_first (&g->grouped_components); ci; ci = set_next (ci)) {
		uint32_t comp = ci->element;
		auto range = ecs_get_subpool_range (&g->components, comp);
		uint32_t *gid = Component_Address (&ecs_component_groups,
										   g->component_groups.data,
										   range.start);
		uint32_t count = range.end - range.start;
		heapsort_r (gid, count, sizeof (uint32_t), ecs_comp_group_cmp, g);
	}
}
