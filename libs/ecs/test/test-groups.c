#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/backtrace.h"
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

static const char *type_names[] = {
	"fully-owning",
	"partially-owning",
	"non-owning",
};

static bool
check_group (ecs_registry_t *reg, uint32_t gid,
			 ecs_grpcomp_t *grpcomp, uint32_t num_grpcomp, ecs_grpown_t type)
{
	auto g = &reg->groups;
	auto range = ecs_get_subpool_range (&g->groups, gid);
	ecs_grpcomp_t *gc = Component_Address (&ecs_group_components,
										   g->group_components.data,
										   range.start);
	uint32_t count = range.end - range.start;
	auto gtype = *(ecs_grpown_t *) ecs_get_component (gid, &g->group_type,
													  &ecs_group_type);
	printf ("group %d: %s%d"DFL" %s%s"DFL"\n", gid,
			count == num_grpcomp ? GRN : RED, count,
			type == gtype ? GRN : RED, type_names[gtype]);
	if (count != num_grpcomp) {
		return false;
	}
	bool ok = true;
	for (; count-- > 0; gc++, grpcomp++) {
		// group_components are expected to be in the order specified when
		// the group is defined as this will affect the order of component
		// pointers when fetching a group SOA
		const char *name = reg->components.a[gc->component].name;
		auto subpool = &reg->subpools[gc->component];
		bool match = gc->component == grpcomp->component
					&& gc->rangeid == grpcomp->rangeid;
		bool valid = true;
		uint32_t reverse_id = nullent;
		if (gc->rangeid != nullent) {
			auto ind = subpool->sorted[gc->rangeid];
			reverse_id = subpool->dense[ind];
		}
		valid = reverse_id == gc->rangeid;
		printf ("%s%d %s %d"DFL" %d %s%d"DFL"\n", match ? GRN : RED,
				gc->component, name, gc->rangeid,
				gc->rangeid != nullent ? subpool->sorted[gc->rangeid]
									   : nullent,
				valid ? GRN : RED, reverse_id);
		ok &= match & valid;
	}
	printf ("\n");
	return ok;
}

static bool
check_component_groups (ecs_registry_t *reg, uint32_t comp,
						uint32_t *groups, uint32_t num_groups)
{
	auto g = &reg->groups;
	auto range = ecs_get_subpool_range (&g->components, comp);
	uint32_t *gid = Component_Address (&ecs_component_groups,
									   g->component_groups.data,
									   range.start);
	const char *name = reg->components.a[comp].name;
	uint32_t count = range.end - range.start;
	printf ("component: %s%d %s %d" DFL ":", count == num_groups ? GRN : RED,
			comp, name, count);
	if (count != num_groups) {
		printf ("\n");
		return false;
	}
	bool ok = true;
	if (count) {
		for (; count-- > 0; gid++, groups++) {
			bool match = *gid == *groups;
			printf (" %s%d" DFL, match ? GRN : RED, *gid);
			ok &= match;
		}
	}
	printf ("\n");
	return ok;
}

static bool
check_group_ents (ecs_registry_t *reg, uint32_t gid, uint32_t num_ents)
{
	auto g = &reg->groups;
	bool ok = true;
	auto gtype = *(ecs_grpown_t *) ecs_get_component (gid, &g->group_type,
													  &ecs_group_type);
	auto range = ecs_get_subpool_range (&g->groups, gid);
	ecs_grpcomp_t *gc = Component_Address (&ecs_group_components,
										   g->group_components.data,
										   range.start);
	uint32_t cmp_count = range.end - range.start;
	if (gtype == ecs_nonown) {
		auto nid = *(uint32_t *) ecs_get_component (gid, &g->nonown_ranges,
													&ecs_group_nonown_range);
		auto ent_range = ecs_get_subpool_range (&g->nonown_groups, nid);
		uint32_t ent_count = ent_range.end - ent_range.start;
		printf ("non-owning: %s%d"DFL"\n",
				ent_count == num_ents ? GRN : RED, ent_count);
		ok &= ent_count == num_ents;
		uint32_t *ent = Component_Address (&ecs_group_nonown_entity,
										   g->nonown_entities.data,
										   ent_range.start);
		for (; ent_count-- > 0; ent++) {
			bool complete = true;
			for (uint32_t i = 0; i < cmp_count; i++) {
				uint32_t cmp = gc[i].component;
				complete &= Ent_HasComponent (*ent, cmp, reg);
			}
			printf (" %3x.%05x: %s",
					Ent_Generation (*ent) >> ENT_IDBITS, Ent_Index (*ent),
					complete ? GRN : RED);
			for (uint32_t i = 0; i < cmp_count; i++) {
				uint32_t cmp = gc[i].component;
				if (Ent_HasComponent (*ent, cmp, reg)) {
					printf (" %d", cmp);
				}
			}
			printf (DFL"\n");
		}
	} else {
		uint32_t last_count = num_ents;
		uint32_t *ents = nullptr;
		for (uint32_t i = 0; i < cmp_count; i++, gc++) {
			auto pool = &reg->comp_pools[gc->component];
			auto subpool = &reg->subpools[gc->component];
			if (gc->rangeid != nullent) {
				auto crange = ecs_get_subpool_range (subpool, gc->rangeid);
				uint32_t count = crange.end - crange.start;
				if (!ents) {
					ents = pool->dense + crange.start;
				}
				// all components in a group must have the same number of
				// entities in the group's subpool range
				bool same_count = count == last_count;
				last_count = count;
				bool same_ents = false;
				auto dense = pool->dense + crange.start;
				if (same_count) {
					same_ents = memcmp (ents, dense,
										sizeof (uint32_t[count])) == 0;
				}
				const char *name = reg->components.a[gc->component].name;
				printf ("%d %s %d: %s%d"DFL, gc->component, name, gc->rangeid,
						same_count ? GRN : RED, count);
				if (count) {
					printf ("%s", same_ents ? GRN : RED);
					for (; count-- > 0; dense++) {
						printf (" %x.%05x",
								Ent_Generation (*dense) >> ENT_IDBITS,
								Ent_Index (*dense));
					}
				}
				printf (DFL"\n");
				ok &= same_count & same_ents;
			}
		}
		printf ("\n");
	}
	return ok;
}

int
main (int argc, char **argv)
{
	BT_Init (argv[0]);
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
	uint32_t g5 = ECS_DefineGroup (reg, (uint32_t[]) {
			base + test_a1,
			base + test_b1,
			base + test_c1,
		}, 3, ecs_fullown);

	ECS_SortGroups (reg);

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
		{ base + test_a1, nullent },
		{ base + test_c1, 0 },
	};
	ecs_grpcomp_t g4_grpcomps[] = {
		{ base + test_b1, 0 },
		{ base + test_b2, 0 },
	};
	ecs_grpcomp_t g5_grpcomps[] = {
		{ base + test_a1, nullent },
		{ base + test_b1, nullent },
		{ base + test_c1, nullent },
	};
	bool ok = true;
#define grp(g) g, g##_grpcomps, countof (g##_grpcomps)
	ok &= check_group (reg, grp (g1), ecs_fullown);
	ok &= check_group (reg, grp (g2), ecs_fullown);
	ok &= check_group (reg, grp (g3), ecs_partown);
	ok &= check_group (reg, grp (g4), ecs_fullown);
	ok &= check_group (reg, grp (g5), ecs_nonown);

	uint32_t *comp_groups[] = {
		(uint32_t []) {1, 0, 2, 4},
		(uint32_t []) {1, 0},
		(uint32_t []) {1},
		(uint32_t []) {3, 4},
		(uint32_t []) {3},
		(uint32_t []) {2, 4},
	};
	uint32_t comp_group_counts[] = { 4, 2, 1, 2, 1, 2 };
	for (uint32_t i = 0; i < reg->components.size; i++) {
		uint32_t *cg = i < base ? nullptr : comp_groups[i - base];
		uint32_t cgc = i < base ? 0 : comp_group_counts[i - base];
		ok &= check_component_groups (reg, i, cg, cgc);
	}
	printf ("\n");

	auto g = &reg->groups;
	auto comp_set = set_new ();
	// all the test components must be in the grouped_components set, but
	// no other components
	set_add_range (comp_set, base, test_num_components);
	bool eq = set_is_equivalent (comp_set, &g->grouped_components);
	printf ("%s%s"DFL"\n\n", eq ? GRN : RED,
			set_as_string (&g->grouped_components));
	ok &= eq;

	uint32_t ent1 = ECS_NewEntity (reg);
	uint32_t ent2 = ECS_NewEntity (reg);
	uint32_t ent3 = ECS_NewEntity (reg);
	uint32_t ent4 = ECS_NewEntity (reg);
	uint32_t ent5 = ECS_NewEntity (reg);
	uint32_t ent6 = ECS_NewEntity (reg);

	Ent_AddGroup (ent1, g1, reg);
	Ent_AddGroup (ent2, g2, reg);
	Ent_AddGroup (ent1, g4, reg);
	Ent_SetComponent (ent3, base + test_a1, reg, nullptr);
	Ent_AddGroup (ent3, g3, reg);

	Ent_SetComponent (ent4, base + test_a1, reg, nullptr);
	Ent_SetComponent (ent4, base + test_c1, reg, nullptr);

	Ent_SetComponent (ent5, base + test_a1, reg, nullptr);
	Ent_SetComponent (ent5, base + test_b1, reg, nullptr);
	Ent_SetComponent (ent5, base + test_c1, reg, nullptr);

	Ent_SetComponent (ent6, base + test_a1, reg, nullptr);
	Ent_SetComponent (ent6, base + test_b1, reg, nullptr);
	Ent_SetComponent (ent6, base + test_c1, reg, nullptr);

	ok &= check_group_ents (reg, g1, 1);
	ok &= check_group_ents (reg, g2, 1);
	ok &= check_group_ents (reg, g3, 4);//g5 includes g3!!
	ok &= check_group_ents (reg, g4, 1);
	ok &= check_group_ents (reg, g5, 2);

	//ECS_PrintEntity (reg, ent1);
	//ECS_PrintEntity (reg, ent2);
	//ECS_PrintEntity (reg, ent3);
	//ECS_PrintEntity (reg, ent4);

	ECS_PrintRegistry (reg);
	ECS_DelRegistry (reg);
	return !ok;
}
