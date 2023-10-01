#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/simd/types.h"
#include "QF/mathlib.h"
#include "QF/ecs.h"

#define DFL "\e[39;49m"
#define BLK "\e[30;40m"
#define RED "\e[31;40m"
#define GRN "\e[32;40m"
#define ONG "\e[33;40m"
#define BLU "\e[34;40m"
#define MAG "\e[35;40m"
#define CYN "\e[36;40m"
#define WHT "\e[37;40m"

enum test_components {
	test_subpool,
	test_obj,
	test_name,

	test_num_components
};

#define prent(e) (Ent_Generation (e) >> ENT_IDBITS), (Ent_Index (e))

static uint32_t
obj_rangeid (ecs_registry_t *reg, uint32_t ent, uint32_t comp)
{
	uint32_t    sp_comp = comp - (test_obj - test_subpool);
	return *(uint32_t *) Ent_GetComponent (ent, sp_comp, reg);
}

static const component_t test_components[] = {
	[test_subpool] = {
		.size = sizeof (uint32_t),
		.name = "subpool",
	},
	[test_obj] = {
		.size = sizeof (uint32_t),
		.name = "obj",
		.rangeid = obj_rangeid,
	},
	[test_name] = {
		.size = sizeof (const char *),
		.name = "name",
	},
};

static void
set_ent_name (uint32_t ent, uint32_t base, ecs_registry_t *reg,
			  const char *name)
{
	Ent_SetComponent (ent, base + test_name, reg, &name);
}

static void
dump_sp_ids (ecs_registry_t *reg, uint32_t comp)
{
	ecs_pool_t *pool = &reg->comp_pools[comp];
	uint32_t   *ent = pool->dense;
	uint32_t   *id = pool->data;

	for (uint32_t i = 0; i < pool->count; i++) {
		const char **n = Ent_GetComponent (ent[i], test_name, reg);
		printf ("ent[%d]: %2d, %2d %s\n", i, ent[i], id[i], *n);
	}
}

static int
check_subpool_ranges (ecs_subpool_t *subpool, uint32_t *expect)
{
	uint32_t    count = subpool->num_ranges - subpool->available;
	uint32_t   *range = subpool->ranges;
	int         ret = 0;

	while (count--) {
		uint32_t   *r = range++;
		uint32_t    e = *expect++;
		printf ("%2d: %2d %2d\n", (int)(r - subpool->ranges), *r, e);
		if (*r != e++) {
			ret = 1;
		}
	}
	return ret;
}

static int
check_subpool_sorted (ecs_subpool_t *subpool)
{
	uint32_t   *sorted = subpool->sorted;
	uint32_t   *ranges = subpool->ranges;
	uint32_t    count = subpool->num_ranges - subpool->available;

	for (uint32_t i = 0; i < count; i++) {
		printf ("sorted[%d]: %d %d\n", i, sorted[i], ranges[sorted[i]]);
	}
	for (uint32_t i = 0; i < count; i++) {
		for (uint32_t j = i + 1; j < count; j++) {
			if (sorted[j] == sorted[i]) {
				printf ("subpool sorted duplicated\n");
				return 1;
			}
			if (sorted[j] >= count) {
				printf ("subpool sorted out of bounds\n");
				return 1;
			}
			if (ranges[sorted[j]] > ranges[count - 1]) {
				printf ("subpool sorted bogus\n");
				return 1;
			}
		}
	}
	return 0;
}

static int
check_obj_comps (ecs_registry_t *reg, uint32_t comp, uint32_t *expect)
{
	ecs_pool_t *pool = &reg->comp_pools[comp];
	uint32_t   *val = pool->data;
	int         fail = 0;

	for (uint32_t i = 0; i < pool->count; i++) {
		const char **n = Ent_GetComponent (pool->dense[i], test_name, reg);
		printf ("val[%d]: %2d %2d %s\n", i, val[i], expect[i], *n);
		if (val[i] != expect[i]) {
			fail = 1;
		}
	}
	return fail;
}

int
main (void)
{
	ecs_registry_t *reg = ECS_NewRegistry ();
	uint32_t base = ECS_RegisterComponents (reg, test_components,
											test_num_components);
	ECS_CreateComponentPools (reg);

	uint32_t    sp1 = ECS_NewSubpoolRange (reg, base + test_obj);
	uint32_t    sp2 = ECS_NewSubpoolRange (reg, base + test_obj);
	uint32_t    sp3 = ECS_NewSubpoolRange (reg, base + test_obj);

	printf ("%d.%d %d.%d %d.%d\n", prent (sp1), prent (sp2), prent (sp3));
	if (reg->subpools[base + test_subpool].num_ranges != 0
		|| reg->subpools[base + test_subpool].available != 0) {
		printf ("subpool not 0 count: %d %d\n",
				reg->subpools[base + test_subpool].num_ranges,
				reg->subpools[base + test_subpool].available);
		return 1;
	}
	if (reg->subpools[base + test_obj].num_ranges != 3
		|| reg->subpools[base + test_obj].available != 0) {
		printf ("obj not 3 count: %d %d\n",
				reg->subpools[base + test_obj].num_ranges,
				reg->subpools[base + test_obj].available);
		return 1;
	}
	for (uint32_t i = 0; i < reg->subpools[base + test_obj].num_ranges; i++) {
		if (reg->subpools[base + test_obj].ranges[i] != 0) {
			printf ("end %d not 0 count: %d\n", i,
					reg->subpools[base + test_obj].ranges[i]);
			return 1;
		}
	}

	uint32_t    enta = ECS_NewEntity (reg);
	uint32_t    entb = ECS_NewEntity (reg);
	uint32_t    entc = ECS_NewEntity (reg);
	uint32_t    entd = ECS_NewEntity (reg);
	uint32_t    ente = ECS_NewEntity (reg);
	uint32_t    entf = ECS_NewEntity (reg);
	uint32_t    entg = ECS_NewEntity (reg);
	uint32_t    enth = ECS_NewEntity (reg);

	Ent_SetComponent (enta, base + test_subpool, reg, &sp1);
	Ent_SetComponent (entb, base + test_subpool, reg, &sp1);
	Ent_SetComponent (entc, base + test_subpool, reg, &sp2);
	Ent_SetComponent (entd, base + test_subpool, reg, &sp3);
	Ent_SetComponent (ente, base + test_subpool, reg, &sp3);
	Ent_SetComponent (entf, base + test_subpool, reg, &sp2);
	Ent_SetComponent (entg, base + test_subpool, reg, &sp2);
	Ent_SetComponent (enth, base + test_subpool, reg, &sp1);

	set_ent_name (enta, base, reg, MAG"a"DFL);
	set_ent_name (entb, base, reg, MAG"b"DFL);
	set_ent_name (entc, base, reg, ONG"c"DFL);
	set_ent_name (entd, base, reg, CYN"d"DFL);
	set_ent_name (ente, base, reg, CYN"e"DFL);
	set_ent_name (entf, base, reg, ONG"f"DFL);
	set_ent_name (entg, base, reg, ONG"g"DFL);
	set_ent_name (enth, base, reg, MAG"h"DFL);

	dump_sp_ids (reg, base + test_subpool);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 0, 0, 0 })) {
		printf ("oops\n");
		return 1;
	}

	uint32_t val = 0;
	Ent_SetComponent (enta, base + test_obj, reg, &val); val++;
	Ent_SetComponent (entb, base + test_obj, reg, &val); val++;
	Ent_SetComponent (entc, base + test_obj, reg, &val); val++;
	Ent_SetComponent (entd, base + test_obj, reg, &val); val++;
	Ent_SetComponent (ente, base + test_obj, reg, &val); val++;
	Ent_SetComponent (entf, base + test_obj, reg, &val); val++;
	Ent_SetComponent (entg, base + test_obj, reg, &val); val++;
	Ent_SetComponent (enth, base + test_obj, reg, &val); val++;

	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 3, 6, 8 })) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 1, 7, 5, 6, 2, 4, 3 })) {
		printf ("oops\n");
		return 1;
	}

	Ent_RemoveComponent (entb, base + test_obj, reg);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 5, 7 })) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 2, 5, 6, 3, 4 })) {
		printf ("oops\n");
		return 1;
	}

	Ent_RemoveComponent (entd, base + test_obj, reg);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 5, 6 })) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 2, 5, 6, 4 })) {
		printf ("oops\n");
		return 1;
	}

	Ent_RemoveComponent (ente, base + test_obj, reg);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 5, 5 })) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 2, 5, 6 })) {
		printf ("oops\n");
		return 1;
	}

	Ent_SetComponent (entd, base + test_obj, reg, &val); val++;
	Ent_SetComponent (ente, base + test_obj, reg, &val); val++;
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 5, 7 })) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 2, 5, 6, 8, 9 })) {
		printf ("oops\n");
		return 1;
	}

	Ent_RemoveComponent (entc, base + test_obj, reg);
	Ent_RemoveComponent (entf, base + test_obj, reg);
	Ent_RemoveComponent (entg, base + test_obj, reg);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 2, 4 })) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 9, 8 })) {
		printf ("oops\n");
		return 1;
	}

	ECS_DelSubpoolRange (reg, base + test_obj, sp2);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 4 })) {
		printf ("oops\n");
		return 1;
	}

	sp2 = ECS_NewSubpoolRange (reg, base + test_obj);
	printf ("sp2: %d.%d\n", prent (sp2));
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 4, 4 })) {
		printf ("oops\n");
		return 1;
	}
	Ent_SetComponent (entc, base + test_subpool, reg, &sp2);
	Ent_SetComponent (entf, base + test_subpool, reg, &sp2);
	Ent_SetComponent (entg, base + test_subpool, reg, &sp2);
	Ent_SetComponent (entc, base + test_obj, reg, &val); val++;
	Ent_SetComponent (entf, base + test_obj, reg, &val); val++;
	Ent_SetComponent (entg, base + test_obj, reg, &val); val++;
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 4, 7})) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 9, 8, 10, 11, 12 })) {
		printf ("oops\n");
		return 1;
	}

	if (check_subpool_sorted (&reg->subpools[base + test_obj])) {
		printf ("oops\n");
		return 1;
	}
	ECS_MoveSubpoolLast (reg, base + test_obj, sp3);
	if (check_subpool_sorted (&reg->subpools[base + test_obj])) {
		printf ("oops\n");
		return 1;
	}
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 5, 7})) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 10, 11, 12, 9, 8 })) {
		printf ("oops\n");
		return 1;
	}

	ECS_DelRegistry (reg);
	return 0;
}
