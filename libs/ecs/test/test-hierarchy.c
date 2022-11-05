#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/ecs/component.h"
#include "QF/ecs/hierarchy.h"

enum {
	test_href,
	test_name,

	test_num_components
};

static const component_t test_components[] = {
	[test_href] = {
		.size = sizeof (hierref_t),
		.create = 0,//create_href,
		.name = "href",
	},
	[test_name] = {
		.size = sizeof (const char *),
		.name = "name",
	},
};

ecs_registry_t *test_reg;

static int
check_hierarchy_size (hierarchy_t *h, uint32_t size)
{
	if (h->num_objects != size) {
		printf ("hierarchy does not have exactly %u transform\n", size);
		return 0;
	}
	ecs_registry_t *reg = h->reg;
	for (uint32_t i = 0; i < h->num_objects; i++) {
		hierref_t  *ref = Ent_GetComponent (h->ent[i], test_href, reg);
		char      **name = Ent_GetComponent (h->ent[i], test_name, reg);;
		if (ref->hierarchy != h) {
			printf ("transform %d (%s) does not point to hierarchy\n",
					i, *name);
		}
	}
	return 1;
}

static void
dump_hierarchy (hierarchy_t *h)
{
	ecs_registry_t *reg = h->reg;
	for (uint32_t i = 0; i < h->num_objects; i++) {
		hierref_t  *ref = Ent_GetComponent (h->ent[i], test_href, reg);
		char      **name = Ent_GetComponent (h->ent[i], test_name, reg);;
		printf ("%2d: %5s %2d %2d %2d %2d %2d\n", i, *name, h->ent[i],
				ref->index, h->parentIndex[i],
				h->childIndex[i], h->childCount[i]);
	}
	puts ("");
}

static void
dump_tree (hierarchy_t *h, uint32_t ind, int level)
{
	if (ind >= h->num_objects) {
		printf ("index %d out of bounds (%d)\n", ind, h->num_objects);
		return;
	}
	ecs_registry_t *reg = h->reg;
	char      **name = Ent_GetComponent (h->ent[ind], test_name, reg);;
	if (!level) {
		puts ("in: en pa ci cc |name");
	}
	printf ("%2d: %2d %2d %2d %2d |%*s%s\n", ind, h->ent[ind],
			h->parentIndex[ind], h->childIndex[ind], h->childCount[ind],
			level * 3, "", *name);

	for (uint32_t i = 0; i < h->childCount[ind]; i++) {
		dump_tree (h, h->childIndex[ind] + i, level + 1);
	}
	if (!level) {
		puts ("");
	}
}

static int
check_indices (uint32_t ent, uint32_t index, uint32_t parentIndex,
			   uint32_t childIndex, uint32_t childCount)
{
	ecs_registry_t *reg = test_reg;
	char      **entname = Ent_GetComponent (ent, test_name, reg);;
	hierref_t  *ref = Ent_GetComponent (ent, test_href, reg);
	hierarchy_t *h = ref->hierarchy;
	if (ref->index != index) {
		char      **name = Ent_GetComponent (h->ent[index], test_name, reg);;
		printf ("%s/%s index incorrect: expect %u got %u\n",
				*entname, *name,
				index, ref->index);
		return 0;
	}
	if (h->parentIndex[index] != parentIndex) {
		printf ("%s parent index incorrect: expect %u got %u\n",
				*entname, parentIndex, h->parentIndex[index]);
		return 0;
	}
	if (h->childIndex[index] != childIndex) {
		printf ("%s child index incorrect: expect %u got %u\n",
				*entname, childIndex, h->childIndex[index]);
		return 0;
	}
	if (h->childCount[index] != childCount) {
		printf ("%s child count incorrect: expect %u got %u\n",
				*entname, childCount, h->childCount[index]);
		return 0;
	}
	return 1;
}

static uint32_t
create_ent (uint32_t parent, const char *name)
{
	uint32_t    ent = ECS_NewEntity (test_reg);
	Ent_SetComponent (ent, test_name, test_reg, &name);
	hierref_t  *ref = Ent_AddComponent (ent, test_href, test_reg);

	if (parent != nullent) {
		hierref_t  *pref = Ent_GetComponent (parent, test_href, test_reg);
		ref->hierarchy = pref->hierarchy;
		ref->index = Hierarchy_InsertHierarchy (pref->hierarchy, 0,
												pref->index, 0);
	} else {
		ref->hierarchy = Hierarchy_New (test_reg, 0, 1);
		ref->index = 0;
	}
	ref->hierarchy->ent[ref->index] = ent;
	return ent;
}

static void
set_parent (uint32_t child, uint32_t parent)
{
	if (parent != nullent) {
		hierref_t  *pref = Ent_GetComponent (parent, test_href, test_reg);
		hierref_t  *cref = Ent_GetComponent (child, test_href, test_reg);
		Hierarchy_SetParent (pref->hierarchy, pref->index,
							 cref->hierarchy, cref->index);
	} else {
		hierref_t  *cref = Ent_GetComponent (child, test_href, test_reg);
		Hierarchy_SetParent (0, nullent, cref->hierarchy, cref->index);
	}
}

static int
test_single_transform (void)
{
	uint32_t    ent = create_ent (nullent, "test");
	hierarchy_t *h;

	if (ent == nullent) {
		printf ("create_ent returned null\n");
		return 1;
	}
	hierref_t  *ref = Ent_GetComponent (ent, test_href, test_reg);
	if (!ref) {
		printf ("Ent_GetComponent(test_href) returned null\n");
		return 1;
	}
	if (!(h = ref->hierarchy)) {
		printf ("New entity has no hierarchy\n");
		return 1;
	}
	if (!check_hierarchy_size (h, 1)) { return 1; }
	if (!check_indices (ent, 0, nullent, 1, 0)) { return 1; }

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (h);

	return 0;
}

static int
test_parent_child_init (void)
{
	uint32_t    parent = create_ent (nullent, "parent");
	uint32_t    child = create_ent (parent, "child");

	hierref_t  *pref = Ent_GetComponent (parent, test_href, test_reg);
	hierref_t  *cref = Ent_GetComponent (child, test_href, test_reg);
	if (pref->hierarchy != cref->hierarchy) {
		printf ("parent and child transforms have separate hierarchies\n");
		return 1;
	}

	hierarchy_t *h = pref->hierarchy;

	if (!check_hierarchy_size (h, 2)) { return 1; }

	if (!check_indices (parent, 0, nullent, 1, 1)) { return 1; }
	if (!check_indices (child,  1, 0, 2, 0)) { return 1; }

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (h);

	return 0;
}

static int
test_parent_child_setparent (void)
{
	uint32_t    parent = create_ent (nullent, "parent");
	uint32_t    child = create_ent (nullent, "child");

	if (!check_indices (parent, 0, nullent, 1, 0)) { return 1; }
	if (!check_indices (child,  0, nullent, 1, 0)) { return 1; }

	hierref_t  *pref = Ent_GetComponent (parent, test_href, test_reg);
	hierref_t  *cref = Ent_GetComponent (child, test_href, test_reg);
	if (pref->hierarchy == cref->hierarchy) {
		printf ("parent and child entities have same hierarchy before"
				" set paret\n");
		return 1;
	}

	set_parent (child, parent);

	if (pref->hierarchy != cref->hierarchy) {
		printf ("parent and child transforms have separate hierarchies\n");
		return 1;
	}

	hierarchy_t *h = pref->hierarchy;

	if (!check_hierarchy_size (h, 2)) { return 1; }

	if (!check_indices (parent, 0, nullent, 1, 1)) { return 1; }
	if (!check_indices (child,  1, 0, 2, 0)) { return 1; }

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (h);

	return 0;
}

static int
test_build_hierarchy (void)
{
	printf ("test_build_hierarchy\n");

	uint32_t    root = create_ent (nullent, "root");
	uint32_t    A = create_ent (root, "A");
	uint32_t    B = create_ent (root, "B");
	uint32_t    C = create_ent (root, "C");

	hierref_t  *ref = Ent_GetComponent (root, test_href, test_reg);

	if (!check_indices (root, 0, nullent, 1, 3)) { return 1; }
	if (!check_indices (A, 1, 0, 4, 0)) { return 1; }
	if (!check_indices (B, 2, 0, 4, 0)) { return 1; }
	if (!check_indices (C, 3, 0, 4, 0)) { return 1; }

	uint32_t    B1 = create_ent (B, "B1");

	if (!check_indices (root, 0, nullent, 1, 3)) { return 1; }
	if (!check_indices ( A, 1, 0, 4, 0)) { return 1; }
	if (!check_indices ( B, 2, 0, 4, 1)) { return 1; }
	if (!check_indices ( C, 3, 0, 5, 0)) { return 1; }
	if (!check_indices (B1, 4, 2, 5, 0)) { return 1; }

	uint32_t    A1 = create_ent (A, "A1");

	if (!check_indices (root, 0, nullent, 1, 3)) { return 1; }
	if (!check_indices ( A, 1, 0, 4, 1)) { return 1; }
	if (!check_indices ( B, 2, 0, 5, 1)) { return 1; }
	if (!check_indices ( C, 3, 0, 6, 0)) { return 1; }
	if (!check_indices (A1, 4, 1, 6, 0)) { return 1; }
	if (!check_indices (B1, 5, 2, 6, 0)) { return 1; }
	uint32_t    A1a = create_ent (A1, "A1a");
	uint32_t    B2 = create_ent (B, "B2");
	uint32_t    A2 = create_ent (A, "A2");
	uint32_t    B3 = create_ent (B, "B3");
	uint32_t    B2a = create_ent (B2, "B2a");

	if (!check_hierarchy_size (ref->hierarchy, 11)) { return 1; }

	if (!check_indices (root, 0, nullent, 1, 3)) { return 1; }
	if (!check_indices (  A,  1, 0,  4, 2)) { return 1; }
	if (!check_indices (  B,  2, 0,  6, 3)) { return 1; }
	if (!check_indices (  C,  3, 0,  9, 0)) { return 1; }
	if (!check_indices ( A1,  4, 1,  9, 1)) { return 1; }
	if (!check_indices ( A2,  5, 1, 10, 0)) { return 1; }
	if (!check_indices ( B1,  6, 2, 10, 0)) { return 1; }
	if (!check_indices ( B2,  7, 2, 10, 1)) { return 1; }
	if (!check_indices ( B3,  8, 2, 11, 0)) { return 1; }
	if (!check_indices (A1a,  9, 4, 11, 0)) { return 1; }
	if (!check_indices (B2a, 10, 7, 11, 0)) { return 1; }

	uint32_t    D = create_ent (root, "D");

	if (!check_hierarchy_size (ref->hierarchy, 12)) { return 1; }

	if (!check_indices (root, 0, nullent, 1, 4)) { return 1; }
	if (!check_indices (  A,  1, 0,  5, 2)) { return 1; }
	if (!check_indices (  B,  2, 0,  7, 3)) { return 1; }
	if (!check_indices (  C,  3, 0, 10, 0)) { return 1; }
	if (!check_indices (  D,  4, 0, 10, 0)) { return 1; }
	if (!check_indices ( A1,  5, 1, 10, 1)) { return 1; }
	if (!check_indices ( A2,  6, 1, 11, 0)) { return 1; }
	if (!check_indices ( B1,  7, 2, 11, 0)) { return 1; }
	if (!check_indices ( B2,  8, 2, 11, 1)) { return 1; }
	if (!check_indices ( B3,  9, 2, 12, 0)) { return 1; }
	if (!check_indices (A1a, 10, 5, 12, 0)) { return 1; }
	if (!check_indices (B2a, 11, 8, 12, 0)) { return 1; }

	dump_hierarchy (ref->hierarchy);
	uint32_t    C1 = create_ent (C, "C1");
	dump_hierarchy (ref->hierarchy);
	if (!check_hierarchy_size (ref->hierarchy, 13)) { return 1; }

	if (!check_indices (root, 0, nullent, 1, 4)) { return 1; }
	if (!check_indices (  A,  1, 0,  5, 2)) { return 1; }
	if (!check_indices (  B,  2, 0,  7, 3)) { return 1; }
	if (!check_indices (  C,  3, 0, 10, 1)) { return 1; }
	if (!check_indices (  D,  4, 0, 11, 0)) { return 1; }
	if (!check_indices ( A1,  5, 1, 11, 1)) { return 1; }
	if (!check_indices ( A2,  6, 1, 12, 0)) { return 1; }
	if (!check_indices ( B1,  7, 2, 12, 0)) { return 1; }
	if (!check_indices ( B2,  8, 2, 12, 1)) { return 1; }
	if (!check_indices ( B3,  9, 2, 13, 0)) { return 1; }
	if (!check_indices ( C1, 10, 3, 13, 0)) { return 1; }
	if (!check_indices (A1a, 11, 5, 13, 0)) { return 1; }
	if (!check_indices (B2a, 12, 8, 13, 0)) { return 1; }

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (ref->hierarchy);

	return 0;
}

static int
test_build_hierarchy2 (void)
{
	printf ("test_build_hierarchy2\n");

	uint32_t    root = create_ent (nullent, "root");
	uint32_t    A = create_ent (root, "A");
	uint32_t    B = create_ent (root, "B");
	uint32_t    C = create_ent (root, "C");
	uint32_t    B1 = create_ent (B, "B1");
	uint32_t    A1 = create_ent (A, "A1");
	uint32_t    A1a = create_ent (A1, "A1a");
	uint32_t    B2 = create_ent (B, "B2");
	uint32_t    A2 = create_ent (A, "A2");
	uint32_t    B3 = create_ent (B, "B3");
	uint32_t    B2a = create_ent (B2, "B2a");
	uint32_t    D = create_ent (root, "D");
	uint32_t    C1 = create_ent (C, "C1");

	hierref_t  *ref = Ent_GetComponent (root, test_href, test_reg);

	if (!check_hierarchy_size (ref->hierarchy, 13)) { return 1; }

	if (!check_indices (root, 0, nullent, 1, 4)) { return 1; }
	if (!check_indices (  A,  1, 0,  5, 2)) { return 1; }
	if (!check_indices (  B,  2, 0,  7, 3)) { return 1; }
	if (!check_indices (  C,  3, 0, 10, 1)) { return 1; }
	if (!check_indices (  D,  4, 0, 11, 0)) { return 1; }
	if (!check_indices ( A1,  5, 1, 11, 1)) { return 1; }
	if (!check_indices ( A2,  6, 1, 12, 0)) { return 1; }
	if (!check_indices ( B1,  7, 2, 12, 0)) { return 1; }
	if (!check_indices ( B2,  8, 2, 12, 1)) { return 1; }
	if (!check_indices ( B3,  9, 2, 13, 0)) { return 1; }
	if (!check_indices ( C1, 10, 3, 13, 0)) { return 1; }
	if (!check_indices (A1a, 11, 5, 13, 0)) { return 1; }
	if (!check_indices (B2a, 12, 8, 13, 0)) { return 1; }

	uint32_t    T = create_ent (nullent, "T");
	uint32_t    X = create_ent (T, "X");
	uint32_t    Y = create_ent (T, "Y");
	uint32_t    Z = create_ent (T, "Z");
	uint32_t    Y1 = create_ent (Y, "Y1");
	uint32_t    X1 = create_ent (X, "X1");
	uint32_t    X1a = create_ent (X1, "X1a");
	uint32_t    Y2 = create_ent (Y, "Y2");
	uint32_t    X2 = create_ent (X, "X2");
	uint32_t    Y3 = create_ent (Y, "Y3");
	uint32_t    Y2a = create_ent (Y2, "Y2a");
	uint32_t    Z1 = create_ent (Z, "Z1");


	hierref_t  *Tref = Ent_GetComponent (T, test_href, test_reg);
	dump_hierarchy (Tref->hierarchy);
	if (!check_hierarchy_size (Tref->hierarchy, 12)) { return 1; }

	if (!check_indices (  T,  0, nullent, 1, 3)) { return 1; }
	if (!check_indices (  X,  1, 0,  4, 2)) { return 1; }
	if (!check_indices (  Y,  2, 0,  6, 3)) { return 1; }
	if (!check_indices (  Z,  3, 0,  9, 1)) { return 1; }
	if (!check_indices ( X1,  4, 1, 10, 1)) { return 1; }
	if (!check_indices ( X2,  5, 1, 11, 0)) { return 1; }
	if (!check_indices ( Y1,  6, 2, 11, 0)) { return 1; }
	if (!check_indices ( Y2,  7, 2, 11, 1)) { return 1; }
	if (!check_indices ( Y3,  8, 2, 12, 0)) { return 1; }
	if (!check_indices ( Z1,  9, 3, 12, 0)) { return 1; }
	if (!check_indices (X1a, 10, 4, 12, 0)) { return 1; }
	if (!check_indices (Y2a, 11, 7, 12, 0)) { return 1; }

	set_parent (T, B);

	dump_hierarchy (ref->hierarchy);

	if (!check_hierarchy_size (ref->hierarchy, 25)) { return 1; }

	if (!check_indices (root, 0, nullent, 1, 4)) { return 1; }
	if (!check_indices (  A,  1,  0,  5, 2)) { return 1; }
	if (!check_indices (  B,  2,  0,  7, 4)) { return 1; }
	if (!check_indices (  C,  3,  0, 11, 1)) { return 1; }
	if (!check_indices (  D,  4,  0, 12, 0)) { return 1; }
	if (!check_indices ( A1,  5,  1, 12, 1)) { return 1; }
	if (!check_indices ( A2,  6,  1, 13, 0)) { return 1; }
	if (!check_indices ( B1,  7,  2, 13, 0)) { return 1; }
	if (!check_indices ( B2,  8,  2, 13, 1)) { return 1; }
	if (!check_indices ( B3,  9,  2, 14, 0)) { return 1; }
	if (!check_indices (  T, 10,  2, 14, 3)) { return 1; }
	if (!check_indices ( C1, 11,  3, 17, 0)) { return 1; }
	if (!check_indices (A1a, 12,  5, 17, 0)) { return 1; }
	if (!check_indices (B2a, 13,  8, 17, 0)) { return 1; }
	if (!check_indices (  X, 14, 10, 17, 2)) { return 1; }
	if (!check_indices (  Y, 15, 10, 19, 3)) { return 1; }
	if (!check_indices (  Z, 16, 10, 22, 1)) { return 1; }
	if (!check_indices ( X1, 17, 14, 23, 1)) { return 1; }
	if (!check_indices ( X2, 18, 14, 24, 0)) { return 1; }
	if (!check_indices ( Y1, 19, 15, 24, 0)) { return 1; }
	if (!check_indices ( Y2, 20, 15, 24, 1)) { return 1; }
	if (!check_indices ( Y3, 21, 15, 25, 0)) { return 1; }
	if (!check_indices ( Z1, 22, 16, 25, 0)) { return 1; }
	if (!check_indices (X1a, 23, 17, 25, 0)) { return 1; }
	if (!check_indices (Y2a, 24, 20, 25, 0)) { return 1; }

	set_parent (Y, nullent);

	dump_hierarchy (ref->hierarchy);
	hierref_t  *Yref = Ent_GetComponent (Y, test_href, test_reg);
	dump_hierarchy (Yref->hierarchy);
	if (!check_hierarchy_size (ref->hierarchy, 20)) { return 1; }
	if (!check_hierarchy_size (Yref->hierarchy, 5)) { return 1; }

	if (!check_indices (root, 0, nullent, 1, 4)) { return 1; }
	if (!check_indices (  A,  1,  0,  5, 2)) { return 1; }
	if (!check_indices (  B,  2,  0,  7, 4)) { return 1; }
	if (!check_indices (  C,  3,  0, 11, 1)) { return 1; }
	if (!check_indices (  D,  4,  0, 12, 0)) { return 1; }
	if (!check_indices ( A1,  5,  1, 12, 1)) { return 1; }
	if (!check_indices ( A2,  6,  1, 13, 0)) { return 1; }
	if (!check_indices ( B1,  7,  2, 13, 0)) { return 1; }
	if (!check_indices ( B2,  8,  2, 13, 1)) { return 1; }
	if (!check_indices ( B3,  9,  2, 14, 0)) { return 1; }
	if (!check_indices (  T, 10,  2, 14, 2)) { return 1; }
	if (!check_indices ( C1, 11,  3, 16, 0)) { return 1; }
	if (!check_indices (A1a, 12,  5, 16, 0)) { return 1; }
	if (!check_indices (B2a, 13,  8, 16, 0)) { return 1; }
	if (!check_indices (  X, 14, 10, 16, 2)) { return 1; }
	if (!check_indices (  Z, 15, 10, 18, 1)) { return 1; }
	if (!check_indices ( X1, 16, 14, 19, 1)) { return 1; }
	if (!check_indices ( X2, 17, 14, 20, 0)) { return 1; }
	if (!check_indices ( Z1, 18, 15, 20, 0)) { return 1; }
	if (!check_indices (X1a, 19, 16, 20, 0)) { return 1; }

	if (!check_indices (  Y, 0, nullent, 1, 3)) { return 1; }
	if (!check_indices ( Y1, 1, 0, 4, 0)) { return 1; }
	if (!check_indices ( Y2, 2, 0, 4, 1)) { return 1; }
	if (!check_indices ( Y3, 3, 0, 5, 0)) { return 1; }
	if (!check_indices (Y2a, 4, 2, 5, 0)) { return 1; }

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (ref->hierarchy);
	Hierarchy_Delete (Yref->hierarchy);

	return 0;
}

static int
test_build_hierarchy3 (void)
{
	printf ("test_build_hierarchy3\n");

	uint32_t    root = create_ent (nullent, "root");
	uint32_t    A = create_ent (root, "A");
	uint32_t    B = create_ent (root, "B");
	uint32_t    C = create_ent (root, "C");
	uint32_t    B1 = create_ent (B, "B1");
	uint32_t    A1 = create_ent (A, "A1");
	uint32_t    A1a = create_ent (A1, "A1a");
	uint32_t    B2 = create_ent (B, "B2");
	uint32_t    A2 = create_ent (A, "A2");
	uint32_t    B3 = create_ent (B, "B3");
	uint32_t    B2a = create_ent (B2, "B2a");
	uint32_t    D = create_ent (root, "D");
	uint32_t    C1 = create_ent (C, "C1");

	hierref_t  *ref = Ent_GetComponent (root, test_href, test_reg);
	dump_hierarchy (ref->hierarchy);
	dump_tree (ref->hierarchy, 0, 0);

	if (!check_hierarchy_size (ref->hierarchy, 13)) { return 1; }
	if (!check_indices (root, 0, nullent, 1, 4)) { return 1; }
	if (!check_indices (  A,  1, 0,  5, 2)) { return 1; }
	if (!check_indices (  B,  2, 0,  7, 3)) { return 1; }
	if (!check_indices (  C,  3, 0, 10, 1)) { return 1; }
	if (!check_indices (  D,  4, 0, 11, 0)) { return 1; }
	if (!check_indices ( A1,  5, 1, 11, 1)) { return 1; }
	if (!check_indices ( A2,  6, 1, 12, 0)) { return 1; }
	if (!check_indices ( B1,  7, 2, 12, 0)) { return 1; }
	if (!check_indices ( B2,  8, 2, 12, 1)) { return 1; }
	if (!check_indices ( B3,  9, 2, 13, 0)) { return 1; }
	if (!check_indices ( C1, 10, 3, 13, 0)) { return 1; }
	if (!check_indices (A1a, 11, 5, 13, 0)) { return 1; }
	if (!check_indices (B2a, 12, 8, 13, 0)) { return 1; }

	set_parent (B2, nullent);
	dump_hierarchy (ref->hierarchy);
	dump_tree (ref->hierarchy, 0, 0);
	hierref_t  *B2ref = Ent_GetComponent (B2, test_href, test_reg);
	dump_hierarchy (B2ref->hierarchy);
	dump_tree (B2ref->hierarchy, 0, 0);

	if (!check_hierarchy_size (ref->hierarchy, 11)) { return 1; }
	if (!check_hierarchy_size (B2ref->hierarchy, 2)) { return 1; }

	if (!check_indices (root, 0, nullent, 1, 4)) { return 1; }
	if (!check_indices (  A,  1, 0,  5, 2)) { return 1; }
	if (!check_indices (  B,  2, 0,  7, 2)) { return 1; }
	if (!check_indices (  C,  3, 0,  9, 1)) { return 1; }
	if (!check_indices (  D,  4, 0, 10, 0)) { return 1; }
	if (!check_indices ( A1,  5, 1, 10, 1)) { return 1; }
	if (!check_indices ( A2,  6, 1, 11, 0)) { return 1; }
	if (!check_indices ( B1,  7, 2, 11, 0)) { return 1; }
	if (!check_indices ( B3,  8, 2, 11, 0)) { return 1; }
	if (!check_indices ( C1,  9, 3, 11, 0)) { return 1; }
	if (!check_indices (A1a, 10, 5, 11, 0)) { return 1; }

	if (!check_indices ( B2, 0, nullent, 1, 1)) { return 1; }
	if (!check_indices (B2a, 1, 0, 2, 0)) { return 1; }

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (ref->hierarchy);

	return 0;
}

int
main (void)
{
	test_reg = ECS_NewRegistry ();
	ECS_RegisterComponents (test_reg, test_components, test_num_components);
	test_reg->href_comp = test_href;

	if (test_single_transform ()) { return 1; }
	if (test_parent_child_init ()) { return 1; }
	if (test_parent_child_setparent ()) { return 1; }
	if (test_build_hierarchy ()) { return 1; }
	if (test_build_hierarchy2 ()) { return 1; }
	if (test_build_hierarchy3 ()) { return 1; }

	ECS_DelRegistry (test_reg);

	return 0;
}
