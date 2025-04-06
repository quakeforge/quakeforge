#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/simd/types.h"
#include "QF/mathlib.h"
#include "QF/ecs.h"

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
dump_sp_ids (ecs_registry_t *reg, uint32_t comp, uint32_t t_name)
{
	ecs_pool_t *pool = &reg->comp_pools[comp];
	uint32_t   *ent = pool->dense;
	uint32_t   *id = pool->data;

	puts (GRN "dump_sp_ids" DFL);
	for (uint32_t i = 0; i < pool->count; i++) {
		const char **n = Ent_GetComponent (ent[i], t_name, reg);
		printf ("ent[%d]: %2d, %2d %s\n", i, ent[i], id[i], *n);
	}
}

static int
check_subpool_ranges (ecs_subpool_t *subpool, uint32_t *expect)
{
	uint32_t    count = subpool->num_ranges - subpool->available;
	uint32_t   *range = subpool->ranges;
	int         ret = 0;

	puts (GRN "check_subpool_ranges" DFL);
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

	puts (GRN "check_subpool_sorted" DFL);
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
check_obj_comps (ecs_registry_t *reg, uint32_t comp, uint32_t *expect,
				 uint32_t *ent_expect, uint32_t t_name)
{
	ecs_pool_t *pool = &reg->comp_pools[comp];
	uint32_t   *val = pool->data;
	uint32_t   *dense = pool->dense;
	uint32_t   *sparse = pool->dense;
	int         fail = 0;

	puts (GRN "check_obj_comps" DFL);
	for (uint32_t i = 0; i < pool->count; i++) {
		const char **n = Ent_GetComponent (dense[i], t_name, reg);
		const char **en = Ent_GetComponent (ent_expect[i], t_name, reg);
		bool ent_ok = dense[i] == ent_expect[i]
					&& i == sparse[Ent_Index(ent_expect[i])];
		printf ("val[%d]: %2d %2d %s %s %d %d%s\n", i,
				val[i], expect[i], *n, *en,
				dense[i], sparse[Ent_Index(dense[i])],
				ent_ok ? "" : RED "***" DFL);
		if (val[i] != expect[i] || !ent_ok) {
			fail = 1;
		}
	}
	return fail;
}

int
main (void)
{
	ecs_registry_t *reg = ECS_NewRegistry ("subpool");
	uint32_t base = ECS_RegisterComponents (reg, test_components,
											test_num_components);
	puts (ONG "setup" DFL);
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

	dump_sp_ids (reg, base + test_subpool, base + test_name);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 0, 0, 0 })) {
		printf ("oops\n");
		return 1;
	}

	puts (ONG "set test_obj" DFL);
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
						 (uint32_t[]) { 0, 1, 7, 5, 6, 2, 4, 3 },
						 (uint32_t[]) { 0, 1, 7, 5, 6, 2, 4, 3 },
						 base + test_name)) {
		printf ("oops\n");
		return 1;
	}

	puts (ONG "remove test_obj from b" DFL);
	Ent_RemoveComponent (entb, base + test_obj, reg);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 5, 7 })) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 2, 5, 6, 3, 4 },
						 (uint32_t[]) { 0, 7, 2, 5, 6, 3, 4 },
						 base + test_name)) {
		printf ("oops\n");
		return 1;
	}

	puts (ONG "remove test_obj from d" DFL);
	Ent_RemoveComponent (entd, base + test_obj, reg);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 5, 6 })) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 2, 5, 6, 4 },
						 (uint32_t[]) { 0, 7, 2, 5, 6, 4 },
						 base + test_name)) {
		printf ("oops\n");
		return 1;
	}

	puts (ONG "remove test_obj from e" DFL);
	Ent_RemoveComponent (ente, base + test_obj, reg);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 5, 5 })) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 2, 5, 6 },
						 (uint32_t[]) { 0, 7, 2, 5, 6 },
						 base + test_name)) {
		printf ("oops\n");
		return 1;
	}

	puts (ONG "add test_obj to d e" DFL);
	Ent_SetComponent (entd, base + test_obj, reg, &val); val++;
	Ent_SetComponent (ente, base + test_obj, reg, &val); val++;
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 5, 7 })) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 2, 5, 6, 8, 9 },
						 (uint32_t[]) { 0, 7, 2, 5, 6, 3, 4 },
						 base + test_name)) {
		printf ("oops\n");
		return 1;
	}

	puts (ONG "remove test_obj from c f g" DFL);
	Ent_RemoveComponent (entc, base + test_obj, reg);
	Ent_RemoveComponent (entf, base + test_obj, reg);
	Ent_RemoveComponent (entg, base + test_obj, reg);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 2, 4 })) {
		printf ("oops\n");
		return 1;
	}
	if (check_obj_comps (reg, base + test_obj,
						 (uint32_t[]) { 0, 7, 9, 8 },
						 (uint32_t[]) { 0, 7, 4, 3 },
						 base + test_name)) {
		printf ("oops\n");
		return 1;
	}

	puts (ONG "delete sp2" DFL);
	ECS_DelSubpoolRange (reg, base + test_obj, sp2);
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 4 })) {
		printf ("oops\n");
		return 1;
	}

	puts (ONG "new sp2" DFL);
	sp2 = ECS_NewSubpoolRange (reg, base + test_obj);
	printf ("sp2: %d.%d\n", prent (sp2));
	if (check_subpool_ranges (&reg->subpools[base + test_obj],
							  (uint32_t[]) { 2, 4, 4 })) {
		printf ("oops\n");
		return 1;
	}
	puts (ONG "add c f g" DFL);
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
						 (uint32_t[]) { 0, 7, 9, 8, 10, 11, 12 },
						 (uint32_t[]) { 0, 7, 4, 3,  2,  5,  6 },
						 base + test_name)) {
		printf ("oops\n");
		return 1;
	}
	if (check_subpool_sorted (&reg->subpools[base + test_obj])) {
		printf ("oops\n");
		return 1;
	}

	puts (ONG "move sp3 last" DFL);
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
						 (uint32_t[]) { 0, 7, 10, 11, 12, 9, 8 },
						 (uint32_t[]) { 0, 7,  2,  5,  6, 4, 3 },
						 base + test_name)) {
		printf ("oops\n");
		return 1;
	}

	ECS_DelRegistry (reg);
	return 0;
}
