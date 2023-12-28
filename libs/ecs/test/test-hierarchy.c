#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/ecs.h"

enum {
	test_href,
	test_name,
	test_highlight,

	test_num_components
};

static const component_t test_components[] = {
	[test_href] = {
		.size = sizeof (hierref_t),
		.create = 0,//create_href,
		.name = "href",
		.destroy = Hierref_DestroyComponent,
	},
	[test_name] = {
		.size = sizeof (const char *),
		.name = "name",
	},
	[test_highlight] = {
		.size = sizeof (byte),
		.name = "highlight",
	},
};

ecs_registry_t *test_reg;

#define DFL "\e[39;49m"
#define BLK "\e[30;40m"
#define RED "\e[31;40m"
#define GRN "\e[32;40m"
#define ONG "\e[33;40m"
#define BLU "\e[34;40m"
#define MAG "\e[35;40m"
#define CYN "\e[36;40m"
#define WHT "\e[37;40m"

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

static const char *
ref_index_color (uint32_t i, uint32_t rind)
{
	return rind != i ? RED : DFL;
}

static const char *
parent_index_color (hierarchy_t *h, uint32_t i)
{
	if (!i && h->parentIndex[i] == nullent) {
		return GRN;
	}
	if (h->parentIndex[i] >= i) {
		return RED;
	}
	uint32_t    ci = h->childIndex[h->parentIndex[i]];
	uint32_t    cc = h->childCount[h->parentIndex[i]];
	if (i < ci || i >= ci + cc) {
		return ONG;
	}
	return DFL;
}

static const char *
child_index_color (hierarchy_t *h, uint32_t i)
{
	if (h->childIndex[i] > h->num_objects
		|| h->childCount[i] > h->num_objects
		|| h->childIndex[i] + h->childCount[i] > h->num_objects) {
		return RED;
	}
	if (h->childIndex[i] <= i) {
		return ONG;
	}
	return DFL;
}

static const char *
child_count_color (hierarchy_t *h, uint32_t i)
{
	if (h->childIndex[i] > h->num_objects
		|| h->childCount[i] > h->num_objects
		|| h->childIndex[i] + h->childCount[i] > h->num_objects) {
		return RED;
	}
	return DFL;
}

static const char *
entity_color (hierarchy_t *h, uint32_t i)
{
	return h->ent[i] == nullent ? MAG : DFL;
}

static const char *
highlight_color (hierarchy_t *h, uint32_t i)
{
	uint32_t    ent = h->ent[i];
	if (ECS_EntValid (ent, test_reg)
		&& Ent_HasComponent (ent, test_highlight, test_reg)) {
		static char color_str[] = "\e[3.;4.m";
		byte       *color = Ent_GetComponent (ent, test_highlight, test_reg);
		if (*color) {
			byte        fg = *color & 0x0f;
			byte        bg = *color >> 4;
			color_str[3] = fg < 8 ? '0' + fg : '9';
			color_str[6] = bg < 8 ? '0' + bg : '9';
			return color_str;
		}
	}
	return "";
}

static void
dump_hierarchy (hierarchy_t *h)
{
	ecs_registry_t *reg = h->reg;
	puts ("in: ri pa ci cc en name");
	for (uint32_t i = 0; i < h->num_objects; i++) {
		uint32_t    rind = nullent;
		static char fake_name[] = ONG "null" DFL;
		static char *fake_nameptr = fake_name;
		char      **name = &fake_nameptr;
		if (ECS_EntValid (h->ent[i], reg)) {
			hierref_t  *ref = Ent_GetComponent (h->ent[i], test_href, reg);
			rind = ref->index;
			if (Ent_HasComponent (h->ent[i], test_name, reg)) {
				name = Ent_GetComponent (h->ent[i], test_name, reg);
			}
		}
		printf ("%2d: %s%2d %s%2d %s%2d %s%2d %s%2d"DFL" %s%s"DFL"\n", i,
				ref_index_color (i, rind), rind,
				parent_index_color (h, i), h->parentIndex[i],
				child_index_color (h, i), h->childIndex[i],
				child_count_color (h, i), h->childCount[i],
				entity_color (h, i), h->ent[i],
				highlight_color (h, i), *name);
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
	if (!level) {
		puts ("in: pa ci cc en|name");
	}
	static char fake_name[] = ONG "null" DFL;
	static char *fake_nameptr = fake_name;
	char      **name = &fake_nameptr;
	ecs_registry_t *reg = h->reg;
	if (ECS_EntValid (h->ent[ind], reg)
		&& Ent_HasComponent (h->ent[ind], test_name, reg)) {
		name = Ent_GetComponent (h->ent[ind], test_name, reg);;
	}
	printf ("%2d: %s%2d %s%2d %s%2d %s%2d"DFL"|%*s%s%s"DFL"\n", ind,
			parent_index_color (h, ind), h->parentIndex[ind],
			child_index_color (h, ind), h->childIndex[ind],
			child_count_color (h, ind), h->childCount[ind],
			entity_color (h, ind), h->ent[ind],
			level * 3, "", highlight_color (h, ind), *name);

	if (h->childIndex[ind] > ind) {
		for (uint32_t i = 0; i < h->childCount[ind]; i++) {
			if (h->childIndex[ind] + i >= h->num_objects) {
				break;
			}
			dump_tree (h, h->childIndex[ind] + i, level + 1);
		}
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
		ref->hierarchy = Hierarchy_New (test_reg, test_href, 0, 1);
		ref->index = 0;
	}
	ref->hierarchy->ent[ref->index] = ent;
	return ent;
}

static void
highlight_ent (uint32_t ent, byte color)
{
	Ent_SetComponent (ent, test_highlight, test_reg, &color);
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

	set_parent (B2, C1);
	dump_hierarchy (ref->hierarchy);
	dump_tree (ref->hierarchy, 0, 0);

	if (!check_hierarchy_size (ref->hierarchy, 13)) { return 1; }

	if (!check_indices (root, 0,  nullent, 1, 4)) { return 1; }
	if (!check_indices (  A,  1,  0,  5, 2)) { return 1; }
	if (!check_indices (  B,  2,  0,  7, 2)) { return 1; }
	if (!check_indices (  C,  3,  0,  9, 1)) { return 1; }
	if (!check_indices (  D,  4,  0, 10, 0)) { return 1; }
	if (!check_indices ( A1,  5,  1, 10, 1)) { return 1; }
	if (!check_indices ( A2,  6,  1, 11, 0)) { return 1; }
	if (!check_indices ( B1,  7,  2, 11, 0)) { return 1; }
	if (!check_indices ( B3,  8,  2, 11, 0)) { return 1; }
	if (!check_indices ( C1,  9,  3, 11, 1)) { return 1; }
	if (!check_indices (A1a, 10,  5, 12, 0)) { return 1; }
	if (!check_indices ( B2, 11,  9, 12, 1)) { return 1; }
	if (!check_indices (B2a, 12, 11, 13, 0)) { return 1; }

	uint32_t    A1b = create_ent (A1, "A1b");
	uint32_t    A1c = create_ent (A1, "A1c");
	uint32_t    B2a1 = create_ent (B2a, "B2a1");
	uint32_t    B2a2 = create_ent (B2a, "B2a2");
	uint32_t    B2b = create_ent (B2, "B2b");
	uint32_t    B2b1 = create_ent (B2b, "B2b1");
	uint32_t    B2b2 = create_ent (B2b, "B2b2");

	dump_hierarchy (ref->hierarchy);
	dump_tree (ref->hierarchy, 0, 0);

	if (!check_hierarchy_size (ref->hierarchy, 20)) { return 1; }

	if (!check_indices (root,  0,  nullent, 1, 4)) { return 1; }
	if (!check_indices (  A,   1,  0,  5, 2)) { return 1; }
	if (!check_indices (  B,   2,  0,  7, 2)) { return 1; }
	if (!check_indices (  C,   3,  0,  9, 1)) { return 1; }
	if (!check_indices (  D,   4,  0, 10, 0)) { return 1; }
	if (!check_indices ( A1,   5,  1, 10, 3)) { return 1; }
	if (!check_indices ( A2,   6,  1, 13, 0)) { return 1; }
	if (!check_indices ( B1,   7,  2, 13, 0)) { return 1; }
	if (!check_indices ( B3,   8,  2, 13, 0)) { return 1; }
	if (!check_indices ( C1,   9,  3, 13, 1)) { return 1; }
	if (!check_indices (A1a,  10,  5, 14, 0)) { return 1; }
	if (!check_indices (A1b,  11,  5, 14, 0)) { return 1; }
	if (!check_indices (A1c,  12,  5, 14, 0)) { return 1; }
	if (!check_indices ( B2,  13,  9, 14, 2)) { return 1; }
	if (!check_indices (B2a,  14, 13, 16, 2)) { return 1; }
	if (!check_indices (B2b,  15, 13, 18, 2)) { return 1; }
	if (!check_indices (B2a1, 16, 14, 20, 0)) { return 1; }
	if (!check_indices (B2a2, 17, 14, 20, 0)) { return 1; }
	if (!check_indices (B2b1, 18, 15, 20, 0)) { return 1; }
	if (!check_indices (B2b2, 19, 15, 20, 0)) { return 1; }

	set_parent (B2, root);
	dump_hierarchy (ref->hierarchy);
	dump_tree (ref->hierarchy, 0, 0);

	if (!check_hierarchy_size (ref->hierarchy, 20)) { return 1; }

	if (!check_indices (root,  0, nullent, 1, 5)) { return 1; }
	if (!check_indices (  A,   1,  0,  6, 2)) { return 1; }
	if (!check_indices (  B,   2,  0,  8, 2)) { return 1; }
	if (!check_indices (  C,   3,  0, 10, 1)) { return 1; }
	if (!check_indices (  D,   4,  0, 11, 0)) { return 1; }
	if (!check_indices ( B2,   5,  0, 11, 2)) { return 1; }
	if (!check_indices ( A1,   6,  1, 13, 3)) { return 1; }
	if (!check_indices ( A2,   7,  1, 16, 0)) { return 1; }
	if (!check_indices ( B1,   8,  2, 16, 0)) { return 1; }
	if (!check_indices ( B3,   9,  2, 16, 0)) { return 1; }
	if (!check_indices ( C1,  10,  3, 16, 0)) { return 1; }
	if (!check_indices (B2a,  11,  5, 16, 2)) { return 1; }
	if (!check_indices (B2b,  12,  5, 18, 2)) { return 1; }
	if (!check_indices (A1a,  13,  6, 20, 0)) { return 1; }
	if (!check_indices (A1b,  14,  6, 20, 0)) { return 1; }
	if (!check_indices (A1c,  15,  6, 20, 0)) { return 1; }
	if (!check_indices (B2a1, 16, 11, 20, 0)) { return 1; }
	if (!check_indices (B2a2, 17, 11, 20, 0)) { return 1; }
	if (!check_indices (B2b1, 18, 12, 20, 0)) { return 1; }
	if (!check_indices (B2b2, 19, 12, 20, 0)) { return 1; }

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (ref->hierarchy);

	return 0;
}

static int
test_build_hierarchy4 (void)
{
	printf ("test_build_hierarchy4\n");

	uint32_t    hud          = create_ent (nullent,     "hud");
	uint32_t    mt           = create_ent (hud,         "mt");
	uint32_t    mt_b         = create_ent (mt,          "mt_b");
	uint32_t    mt_b3        = create_ent (mt_b,        "mt_b3");
	uint32_t    main         = create_ent (hud,         "main");
	uint32_t    main_mf      = create_ent (main,        "main_mf");
	uint32_t    main_mf_b    = create_ent (main_mf,     "main_mf_b");
	uint32_t    main_mf_b7   = create_ent (main_mf_b,   "main_mf_b7");
	uint32_t    main_i       = create_ent (main,        "main_i");
	uint32_t    main_i_f     = create_ent (main_i,      "main_i_f");
	uint32_t    main_i_f_b   = create_ent (main_i_f,    "main_i_f_b");
	uint32_t    main_i_f_b4  = create_ent (main_i_f_b,  "main_i_f_b4");
	uint32_t    main_i_s     = create_ent (main_i,      "main_i_s");
	uint32_t    main_i_s4    = create_ent (main_i_s,    "main_i_s4");
	uint32_t    main_i_i     = create_ent (main_i,      "main_i_i");
	uint32_t    main_i_i4    = create_ent (main_i_i,    "main_i_i4");
	uint32_t    main_i_a     = create_ent (main_i,      "main_i_a");
	uint32_t    main_i_a_w   = create_ent (main_i_a,    "main_i_a_w");
	uint32_t    main_i_a_w7  = create_ent (main_i_a_w,  "main_i_a_w7");
	uint32_t    main_i_a_ma  = create_ent (main_i_a,    "main_i_a_ma");
	uint32_t    main_i_a_ma4 = create_ent (main_i_a_ma, "main_i_a_ma4");
	uint32_t    main_sb      = create_ent (main,        "main_sb");
	uint32_t    main_sb_a    = create_ent (main_sb,     "main_sb_a");
	uint32_t    main_sb_a4   = create_ent (main_sb_a,   "main_sb_a4");
	uint32_t    main_sb_f    = create_ent (main_sb,     "main_sb_f");
	uint32_t    main_sb_h    = create_ent (main_sb,     "main_sb_h");
	uint32_t    main_sb_h3   = create_ent (main_sb_h,   "main_sb_h3");
	uint32_t    main_sb_A    = create_ent (main_sb,     "main_sb_A");
	uint32_t    main_sb_A4   = create_ent (main_sb_A,   "main_sb_A4");
	uint32_t    main_t0      = create_ent (main,        "main_t0");
	uint32_t    main_t1      = create_ent (main,        "main_t1");
	uint32_t    main_S       = create_ent (main,        "main_S");
	uint32_t    main_S_m     = create_ent (main_S,      "main_S_m");
	uint32_t    main_S_s     = create_ent (main_S,      "main_S_s");
	uint32_t    main_S_t     = create_ent (main_S,      "main_S_t");
	uint32_t    main_S_a     = create_ent (main_S,      "main_S_a");
	uint32_t    main_S_a_n   = create_ent (main_S_a,    "main_S_a_n");

	highlight_ent (main_i_a, 0x06);
	highlight_ent (main_i_a_w, 0x05);
	highlight_ent (main_i_a_ma, 0x05);
	highlight_ent (main_i_a_w7, 0x02);
	highlight_ent (main_i_a_ma4, 0x02);

	highlight_ent (main_sb_a, 0x03);
	highlight_ent (main_sb_h, 0x03);
	highlight_ent (main_sb_A, 0x03);
	highlight_ent (main_S_a, 0x03);
	highlight_ent (main_i_f_b, 0x03);
	highlight_ent (main_sb_a4, 0x01);
	highlight_ent (main_sb_h3, 0x01);
	highlight_ent (main_sb_A4, 0x01);
	highlight_ent (main_S_a_n, 0x01);
	highlight_ent (main_i_f_b4, 0x01);

	hierref_t  *ref = Ent_GetComponent (hud, test_href, test_reg);
	dump_hierarchy (ref->hierarchy);
	dump_tree (ref->hierarchy, 0, 0);

	if (!check_hierarchy_size (ref->hierarchy, 37)) { return 1; }

	if (!check_indices (hud,       0, nullent, 1, 2)) { return 1; }
	if (!check_indices (mt,            1,  0,  3, 1)) { return 1; }
	if (!check_indices (main,          2,  0,  4, 6)) { return 1; }
	if (!check_indices (mt_b,          3,  1, 10, 1)) { return 1; }
	if (!check_indices (main_mf,       4,  2, 11, 1)) { return 1; }
	if (!check_indices (main_i,        5,  2, 12, 4)) { return 1; }
	if (!check_indices (main_sb,       6,  2, 16, 4)) { return 1; }
	if (!check_indices (main_t0,       7,  2, 20, 0)) { return 1; }
	if (!check_indices (main_t1,       8,  2, 20, 0)) { return 1; }
	if (!check_indices (main_S,        9,  2, 20, 4)) { return 1; }
	if (!check_indices (mt_b3,        10,  3, 24, 0)) { return 1; }
	if (!check_indices (main_mf_b,    11,  4, 24, 1)) { return 1; }
	if (!check_indices (main_i_f,     12,  5, 25, 1)) { return 1; }
	if (!check_indices (main_i_s,     13,  5, 26, 1)) { return 1; }
	if (!check_indices (main_i_i,     14,  5, 27, 1)) { return 1; }
	if (!check_indices (main_i_a,     15,  5, 28, 2)) { return 1; }
	if (!check_indices (main_sb_a,    16,  6, 30, 1)) { return 1; }
	if (!check_indices (main_sb_f,    17,  6, 31, 0)) { return 1; }
	if (!check_indices (main_sb_h,    18,  6, 31, 1)) { return 1; }
	if (!check_indices (main_sb_A,    19,  6, 32, 1)) { return 1; }
	if (!check_indices (main_S_m,     20,  9, 33, 0)) { return 1; }
	if (!check_indices (main_S_s,     21,  9, 33, 0)) { return 1; }
	if (!check_indices (main_S_t,     22,  9, 33, 0)) { return 1; }
	if (!check_indices (main_S_a,     23,  9, 33, 1)) { return 1; }
	if (!check_indices (main_mf_b7,   24, 11, 34, 0)) { return 1; }
	if (!check_indices (main_i_f_b,   25, 12, 34, 1)) { return 1; }
	if (!check_indices (main_i_s4,    26, 13, 35, 0)) { return 1; }
	if (!check_indices (main_i_i4,    27, 14, 35, 0)) { return 1; }
	if (!check_indices (main_i_a_w,   28, 15, 35, 1)) { return 1; }
	if (!check_indices (main_i_a_ma,  29, 15, 36, 1)) { return 1; }
	if (!check_indices (main_sb_a4,   30, 16, 37, 0)) { return 1; }
	if (!check_indices (main_sb_h3,   31, 18, 37, 0)) { return 1; }
	if (!check_indices (main_sb_A4,   32, 19, 37, 0)) { return 1; }
	if (!check_indices (main_S_a_n,   33, 23, 37, 0)) { return 1; }
	if (!check_indices (main_i_f_b4,  34, 25, 37, 0)) { return 1; }
	if (!check_indices (main_i_a_w7,  35, 28, 37, 0)) { return 1; }
	if (!check_indices (main_i_a_ma4, 36, 29, 37, 0)) { return 1; }

	set_parent (main_i_a, hud);
	dump_hierarchy (ref->hierarchy);
	dump_tree (ref->hierarchy, 0, 0);

	if (!check_indices (hud,       0, nullent, 1, 3)) { return 1; }
	if (!check_indices (mt,            1,  0,  4, 1)) { return 1; }
	if (!check_indices (main,          2,  0,  5, 6)) { return 1; }
	if (!check_indices (main_i_a,      3,  0, 11, 2)) { return 1; }
	if (!check_indices (mt_b,          4,  1, 13, 1)) { return 1; }
	if (!check_indices (main_mf,       5,  2, 14, 1)) { return 1; }
	if (!check_indices (main_i,        6,  2, 15, 3)) { return 1; }
	if (!check_indices (main_sb,       7,  2, 18, 4)) { return 1; }
	if (!check_indices (main_t0,       8,  2, 22, 0)) { return 1; }
	if (!check_indices (main_t1,       9,  2, 22, 0)) { return 1; }
	if (!check_indices (main_S,       10,  2, 22, 4)) { return 1; }
	if (!check_indices (main_i_a_w,   11,  3, 26, 1)) { return 1; }
	if (!check_indices (main_i_a_ma,  12,  3, 27, 1)) { return 1; }
	if (!check_indices (mt_b3,        13,  4, 28, 0)) { return 1; }
	if (!check_indices (main_mf_b,    14,  5, 28, 1)) { return 1; }
	if (!check_indices (main_i_f,     15,  6, 29, 1)) { return 1; }
	if (!check_indices (main_i_s,     16,  6, 30, 1)) { return 1; }
	if (!check_indices (main_i_i,     17,  6, 31, 1)) { return 1; }
	if (!check_indices (main_sb_a,    18,  7, 32, 1)) { return 1; }
	if (!check_indices (main_sb_f,    19,  7, 33, 0)) { return 1; }
	if (!check_indices (main_sb_h,    20,  7, 33, 1)) { return 1; }
	if (!check_indices (main_sb_A,    21,  7, 34, 1)) { return 1; }
	if (!check_indices (main_S_m,     22, 10, 35, 0)) { return 1; }
	if (!check_indices (main_S_s,     23, 10, 35, 0)) { return 1; }
	if (!check_indices (main_S_t,     24, 10, 35, 0)) { return 1; }
	if (!check_indices (main_S_a,     25, 10, 35, 1)) { return 1; }
	if (!check_indices (main_i_a_w7,  26, 11, 36, 0)) { return 1; }
	if (!check_indices (main_i_a_ma4, 27, 12, 36, 0)) { return 1; }
	if (!check_indices (main_mf_b7,   28, 14, 36, 0)) { return 1; }
	if (!check_indices (main_i_f_b,   29, 15, 36, 1)) { return 1; }
	if (!check_indices (main_i_s4,    30, 16, 37, 0)) { return 1; }
	if (!check_indices (main_i_i4,    31, 17, 37, 0)) { return 1; }
	if (!check_indices (main_sb_a4,   32, 18, 37, 0)) { return 1; }
	if (!check_indices (main_sb_h3,   33, 20, 37, 0)) { return 1; }
	if (!check_indices (main_sb_A4,   34, 21, 37, 0)) { return 1; }
	if (!check_indices (main_S_a_n,   35, 25, 37, 0)) { return 1; }
	if (!check_indices (main_i_f_b4,  36, 29, 37, 0)) { return 1; }

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (ref->hierarchy);

	return 0;
}

int
main (void)
{
	test_reg = ECS_NewRegistry ("hierarchy");
	ECS_RegisterComponents (test_reg, test_components, test_num_components);
	ECS_CreateComponentPools (test_reg);

	if (test_single_transform ()) { return 1; }
	if (test_parent_child_init ()) { return 1; }
	if (test_parent_child_setparent ()) { return 1; }
	if (test_build_hierarchy ()) { return 1; }
	if (test_build_hierarchy2 ()) { return 1; }
	if (test_build_hierarchy3 ()) { return 1; }
	if (test_build_hierarchy4 ()) { return 1; }

	ECS_DelRegistry (test_reg);

	return 0;
}
