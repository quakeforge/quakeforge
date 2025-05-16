#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/heapsort.h"
#include "QF/simd/types.h"
#include "QF/mathlib.h"
#include "QF/ecs.h"

enum test_components {
	test_a1,
	test_a2,
	test_a3,

	test_b1,
	test_b2,

	test_c1,

	test_num_components
};

#define prent(e) (Ent_Generation (e) >> ENT_IDBITS), (Ent_Index (e))

static const component_t test_components[] = {
	[test_a1] = {
		.size = sizeof (uint32_t),
		.name = "a1",
	},
	[test_a2] = {
		.size = sizeof (uint32_t),
		.name = "a2",
	},
	[test_a3] = {
		.size = sizeof (uint32_t),
		.name = "a3",
	},

	[test_b1] = {
		.size = sizeof (uint32_t),
		.name = "b1",
	},
	[test_b2] = {
		.size = sizeof (uint32_t),
		.name = "b2",
	},

	[test_c1] = {
		.size = sizeof (uint32_t),
		.name = "c1",
	},
};

static bool
check_group (ecs_registry_t *reg, uint32_t gid,
			 ecs_grpcomp_t *grpcomp, uint32_t num_grpcomp)
{
	auto range = ecs_get_subpool_range (&reg->groups.groups, gid);
	ecs_grpcomp_t *gc = Component_Address (&ecs_group_components,
										   reg->groups.group_components.data,
										   range.start);
	uint32_t count = range.end - range.start;
	printf ("group %d: %s%d" DFL "\n", gid, count == num_grpcomp ? GRN : RED,
			count);
	if (count != num_grpcomp) {
		return false;
	}
	bool ok = true;
	for (; count-- > 0; gc++, grpcomp++) {
		// group_components are expected to be in the order specified when
		// the group is defined as this will affect the order of component
		// pointers when fetching a group SOA
		const char *name = reg->components.a[gc->component].name;
		bool match = gc->component == grpcomp->component
					&& gc->rangeid == grpcomp->rangeid;
		printf ("%s%d %s %d" DFL "\n", match ? GRN : RED, gc->component, name,
				gc->rangeid);
		ok &= match;
	}
	printf ("\n");
	return ok;
}

static int
grp_cmp (const void *_a, const void *_b)
{
	const uint32_t *a = _a;
	const uint32_t *b = _b;
	return *a - *b;
}

static bool
check_component_groups (ecs_registry_t *reg, uint32_t comp,
						uint32_t *groups, uint32_t num_groups)
{
	auto range = ecs_get_subpool_range (&reg->groups.components, comp);
	uint32_t *gid = Component_Address (&ecs_component_groups,
									   reg->groups.component_groups.data,
									   range.start);
	const char *name = reg->components.a[comp].name;
	uint32_t count = range.end - range.start;
	printf ("component: %s%d %s %d" DFL ":", count == num_groups ? GRN : RED,
			comp, name, count);
	if (count != num_groups) {
		return false;
	}
	bool ok = true;
	if (count) {
		// component_groups can be in any order as the data is just a list
		// of the groups the component is in
		uint32_t sorted_groups[count];
		memcpy (sorted_groups, gid, count * sizeof (uint32_t));
		heapsort (sorted_groups, count, sizeof (uint32_t), grp_cmp);
		gid = sorted_groups;

		for (; count-- > 0; gid++, groups++) {
			bool match = *gid == *groups;
			printf (" %s%d" DFL, match ? GRN : RED, *gid);
			ok &= match;
		}
	}
	printf ("\n");
	return ok;
}

int
main (void)
{
	ecs_registry_t *reg = ECS_NewRegistry ("groups");
	uint32_t base = ECS_RegisterComponents (reg, test_components,
											test_num_components);
	ECS_CreateComponentPools (reg);

	printf (ONG "setup" DFL ": %u\n", base);
	uint32_t g1 = ECS_DefineGroup (reg, (uint32_t[]) {
			base + test_a1,
			base + test_a2,
		}, 2, ecs_fullown);
	uint32_t g2 = ECS_DefineGroup (reg, (uint32_t[]) {
			base + test_a1,
			base + test_a2,
			base + test_a3,
		}, 3, ecs_fullown);
	uint32_t g3 = ECS_DefineGroup (reg, (uint32_t[]) {
			base + test_a1,
			base + test_c1,
		}, 2, ecs_partown);
	uint32_t g4 = ECS_DefineGroup (reg, (uint32_t[]) {
			base + test_b1,
			base + test_b2,
		}, 2, ecs_fullown);

	ecs_grpcomp_t g1_grpcomps[] = {
		{ base + test_a1, 0 },
		{ base + test_a2, 0 },
	};
	ecs_grpcomp_t g2_grpcomps[] = {
		{ base + test_a1, 1 },
		{ base + test_a2, 1 },
		{ base + test_a3, 0 },
	};
	ecs_grpcomp_t g3_grpcomps[] = {
		{ base + test_a1, 2 },
		{ base + test_c1, 0 },
	};
	ecs_grpcomp_t g4_grpcomps[] = {
		{ base + test_b1, 0 },
		{ base + test_b2, 0 },
	};
	bool ok = true;
	ok &= check_group (reg, g1, g1_grpcomps, countof (g1_grpcomps));
	ok &= check_group (reg, g2, g2_grpcomps, countof (g2_grpcomps));
	ok &= check_group (reg, g3, g3_grpcomps, countof (g3_grpcomps));
	ok &= check_group (reg, g4, g4_grpcomps, countof (g4_grpcomps));

	uint32_t *comp_groups[] = {
		(uint32_t []) {0, 1, 2},
		(uint32_t []) {0, 1},
		(uint32_t []) {1},
		(uint32_t []) {3},
		(uint32_t []) {3},
		(uint32_t []) {2},
	};
	uint32_t comp_group_counts[] = { 3, 2, 1, 1, 1, 1 };
	for (uint32_t i = 0; i < reg->components.size; i++) {
		uint32_t *cg = i < base ? nullptr : comp_groups[i - base];
		uint32_t cgc = i < base ? 0 : comp_group_counts[i - base];
		ok &= check_component_groups (reg, i, cg, cgc);
	}
	uint32_t ent1 = ECS_NewEntity (reg);
	uint32_t ent2 = ECS_NewEntity (reg);
	uint32_t ent3 = ECS_NewEntity (reg);

	Ent_AddGroup (ent1, g1, reg);
	Ent_AddGroup (ent2, g2, reg);
	Ent_AddGroup (ent1, g4, reg);
	Ent_SetComponent (ent3, base + test_a1, reg, nullptr);
	Ent_AddGroup (ent3, g3, reg);

	ECS_DelRegistry (reg);
	return !ok;
}
