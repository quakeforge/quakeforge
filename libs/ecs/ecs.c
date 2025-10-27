/*
	ecs.c

	Entity Component System

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

#define IMPLEMENT_ECS_Funcs
#include "QF/ecs.h"

static void
ecs_hierarchy_create (void *hierarchy)
{
	Hierarchy_Create (hierarchy);
}

static void
ecs_hierarchy_destroy (void *hierarchy, ecs_registry_t *reg)
{
	Hierarchy_Destroy (hierarchy);
}

static const component_t ecs_components[ecs_comp_count] = {
	[ecs_name] = {
		.size = sizeof (char *),
		.name = "name",
	},
	[ecs_hierarchy] = {
		.size = sizeof (hierarchy_t),
		.name = "hierarchy",
		.create = ecs_hierarchy_create,
		.destroy = ecs_hierarchy_destroy,
	},
};

VISIBLE ecs_registry_t *
ECS_NewRegistry (const char *name)
{
	ecs_registry_t *reg = calloc (1, sizeof (ecs_registry_t));
	reg->name = name;
	reg->components = (componentset_t) DARRAY_STATIC_INIT (32);
	reg->entities.next = Ent_Index (nullent);
	ECS_RegisterComponents (reg, ecs_components, ecs_comp_count);
	return reg;
}

VISIBLE void
ECS_DelRegistry (ecs_registry_t *registry)
{
	qfZoneScoped (true);
	if (!registry) {
		return;
	}
	registry->locked = 1;
	for (uint32_t i = registry->components.size; i-- > 0 ;) {
		__auto_type comp = &registry->components.a[i];
		__auto_type pool = &registry->comp_pools[i];
		Component_DestroyElements (comp, pool->data, 0, pool->count, registry);
		pool->count = 0;
	}
	free (registry->entities.ids);
	for (uint32_t i = 0; i < registry->components.size; i++) {
		free (registry->comp_pools[i].sparse);
		free (registry->comp_pools[i].dense);
		free (registry->comp_pools[i].data);

		free (registry->subpools[i].sorted);
		free (registry->subpools[i].ranges);
		free (registry->subpools[i].rangeids);
	}
	DARRAY_CLEAR (&registry->components);
	free (registry->subpools);
	free (registry->comp_pools);
	free (registry);
}

VISIBLE uint32_t
ECS_RegisterComponents (ecs_registry_t *registry,
						const component_t *components, uint32_t count)
{
	uint32_t    base = registry->components.size;
	DARRAY_RESIZE (&registry->components, base + count);
	memcpy (registry->components.a + base, components,
			count * sizeof (component_t));
	return base;
}

VISIBLE void
ECS_CreateComponentPools (ecs_registry_t *registry)
{
	uint32_t    count = registry->components.size;
	registry->comp_pools = calloc (count, sizeof (ecs_pool_t));
	registry->subpools = calloc (count, sizeof (ecs_subpool_t));
	size_t      size = registry->entities.max_ids * sizeof (uint32_t);
	for (uint32_t i = 0; i < count; i++) {
		registry->comp_pools[i].sparse = malloc (size);
		memset (registry->comp_pools[i].sparse, nullent, size);
		registry->subpools[i].next = Ent_Index (nullent);
	}
	registry->groups.group_components.sparse
		= &registry->component_groups_sparse_hack;
	registry->groups.component_groups.sparse
		= &registry->component_groups_sparse_hack;
	for (uint32_t i = 0; i < count; i++) {
		uint32_t id = ecs_new_subpool_range (&registry->groups.components);
		if (id != i) {
			Sys_Error ("components rangeid != component: %d != %d", id, i);
		}
	}
}

typedef struct {
	__compar_d_fn_t cmp;
	void       *arg;
	ecs_pool_t *pool;
	const component_t *comp;
} ecs_sort_t;

static int
ecs_compare (const void *a, const void *b, void *arg)
{
	ecs_sort_t *sortctx = arg;
	return sortctx->cmp (a, b, sortctx->arg);
}

static void
swap_uint32 (uint32_t *a, uint32_t *b)
{
	uint32_t    t = *a;
	*a = *b;
	*b = t;
}

static void
ecs_swap (void *_a, void *_b, void *arg)
{
	ecs_sort_t *sortctx = arg;
	size_t      size = sortctx->comp->size;
	ecs_pool_t *pool = sortctx->pool;
	uint32_t   *a = _a;
	uint32_t   *b = _b;
	uint32_t    a_ind = a - pool->dense;
	uint32_t    b_ind = b - pool->dense;
	uint32_t    a_ent_ind = Ent_Index (pool->dense[a_ind]);
	uint32_t    b_ent_ind = Ent_Index (pool->dense[b_ind]);
	__auto_type a_data = (byte *) pool->data + a_ind * size;
	__auto_type b_data = (byte *) pool->data + b_ind * size;
	Component_SwapElements (sortctx->comp, a_data, b_data);
	swap_uint32 (a, b);
	swap_uint32 (&pool->sparse[a_ent_ind], &pool->sparse[b_ent_ind]);
}

VISIBLE void
ECS_SortComponentPoolRange (ecs_registry_t *registry, uint32_t component,
						    ecs_range_t range, __compar_d_fn_t cmp, void *arg)
{
	if (component >= registry->components.size) {
		Sys_Error ("ECS_SortComponentPoolRange: invalid component: %u",
				   component);
	}
	ecs_pool_t *pool = &registry->comp_pools[component];
	if (!pool->count || range.end <= range.start) {
		return;
	}
	uint32_t    count = range.end - range.start;
	uint32_t   *start = pool->dense + range.start;
	__auto_type comp = &registry->components.a[component];
	ecs_sort_t sortctx = { .cmp = cmp, .arg = arg, .pool = pool, .comp = comp };
	heapsort_s (start, count, sizeof (uint32_t),
				ecs_compare, ecs_swap, &sortctx);
}

VISIBLE void
ECS_SortComponentPool (ecs_registry_t *registry, uint32_t component,
					   __compar_d_fn_t cmp, void *arg)
{
	if (component >= registry->components.size) {
		Sys_Error ("ECS_SortComponentPool: invalid component: %u", component);
	}
	ecs_pool_t *pool = &registry->comp_pools[component];
	ecs_range_t range = { .start = 0, .end = pool->count };
	ECS_SortComponentPoolRange (registry, component, range, cmp, arg);
}

VISIBLE uint32_t
ECS_NewId (ecs_idpool_t *idpool)
{
	uint32_t    id;

	if (idpool->available) {
		idpool->available--;
		uint32_t    next = idpool->next;
		id = next | Ent_Generation (idpool->ids[next]);
		idpool->next = Ent_Index (idpool->ids[next]);
		idpool->ids[next] = id;
	} else {
		if (idpool->num_ids == Ent_Index (nullent)) {
			Sys_Error ("ECS_NewEntity: out of entities");
		}
		if (idpool->num_ids == idpool->max_ids) {
			idpool->max_ids += ENT_GROW;
			size_t      size = idpool->max_ids * sizeof (uint32_t);
			idpool->ids = realloc (idpool->ids, size);
		}
		id = idpool->num_ids++;
		// id starts out with generation 0
		idpool->ids[id] = id;
	}
	return id;
}

VISIBLE bool
ECS_DelId (ecs_idpool_t *idpool, uint32_t id)
{
	uint32_t    ind = Ent_Index (id);
	if (ind >= idpool->num_ids || idpool->ids[ind] != id) {
		return false;
	}

	uint32_t    next = idpool->next | Ent_NextGen (Ent_Generation (id));
	idpool->ids[ind] = next;
	idpool->next = ind;
	idpool->available++;
	return true;
}

VISIBLE uint32_t
ECS_NewEntity (ecs_registry_t *registry)
{
	uint32_t    old_max_ids = registry->entities.max_ids;
	uint32_t    ent = ECS_NewId (&registry->entities);
	// The id pool grew, so grow the sparse arrays in the component pools
	// to match
	if (old_max_ids != registry->entities.max_ids) {
		size_t      size = registry->entities.max_ids * sizeof (uint32_t);
		for (uint32_t i = 0; i < registry->components.size; i++) {
			uint32_t   *sparse = registry->comp_pools[i].sparse;
			sparse = realloc (sparse, size);
			memset (sparse + registry->entities.max_ids - ENT_GROW, nullent,
					ENT_GROW * sizeof (uint32_t));
			registry->comp_pools[i].sparse = sparse;
		}
	}
	return ent;
}

VISIBLE void
ECS_DelEntity (ecs_registry_t *registry, uint32_t ent)
{
	if (registry->locked) {
		// the registry is being deleted and mass entity and component
		// deletions are going on
		return;
	}
	if (!ECS_DelId (&registry->entities, ent)) {
		return;
	}

	for (uint32_t i = 0; i < registry->components.size; i++) {
		Ent_RemoveComponent (ent, i, registry);
	}
}

VISIBLE void
ECS_RemoveEntities (ecs_registry_t *registry, uint32_t component)
{
	ecs_pool_t *pool = &registry->comp_pools[component];
	const component_t *comp = &registry->components.a[component];
	__auto_type destroy = comp->destroy;
	if (destroy) {
		byte       *data = registry->comp_pools[component].data;
		for (uint32_t i = 0; i < pool->count; i++) {
			destroy (data + i * comp->size, registry);
		}
	}
	pool->count = 0;
}

VISIBLE void
ECS_PrintEntity (ecs_registry_t *registry, uint32_t ent)
{
	bool valid = ECS_EntValid (ent, registry);
	printf ("%08x %s\n", ent,
			valid ? "valid"
			      : ent < registry->entities.num_ids ? "deleted"
				  : ent == nullent ? "null"
				  : "invalid");
	if (!valid) {
		return;
	}
	for (size_t i = 0; i < registry->components.size; i++) {
		if (!Ent_HasComponent (ent, i, registry)) {
			continue;
		}
		printf ("%3zd %s\n", i, registry->components.a[i].name);
	}
}

VISIBLE void
ECS_PrintRegistry (ecs_registry_t *registry)
{
	printf ("%s %d\n", registry->name, registry->entities.num_ids);
	for (size_t i = 0; i < registry->components.size; i++) {
		printf ("%3zd %7d %s\n", i, registry->comp_pools[i].count,
				registry->components.a[i].name);
		auto subpool = &registry->subpools[i];
		if (subpool->num_ranges) {
			printf ("   next     : %x\n", subpool->next);
			printf ("   available: %x\n", subpool->available);
			for (uint32_t j = 0; j < subpool->num_ranges; j++) {
				uint32_t rid = subpool->rangeids[j];
				uint32_t sind = subpool->sorted[j];
				uint32_t end = subpool->ranges[sind];
				uint32_t start = sind ? subpool->ranges[sind - 1] : 0;
				printf ("   %5x: %3x.%05x %d %d-%d\n", j,
						Ent_Generation (rid) >> ENT_IDBITS, Ent_Index (rid),
						sind, start, end);
			}
			printf ("\n");
		}
	}
}
