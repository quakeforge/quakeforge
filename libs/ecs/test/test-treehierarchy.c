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

static uint32_t comp_base;
#define t_href (comp_base + test_href)
#define t_name (comp_base + test_name)
#define t_highlight (comp_base + test_highlight)

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
check_hierarchy_size (hierref_t href, uint32_t size)
{
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, test_reg);
	if (h->num_objects != size) {
		printf ("hierarchy does not have exactly %u transform\n", size);
		return 0;
	}
	for (uint32_t i = 0; i < h->num_objects; i++) {
		auto ref = *(hierref_t *) Ent_GetComponent (h->ent[i], t_href,
													test_reg);
		char      **name = Ent_GetComponent (h->ent[i], t_name, test_reg);
		if (ref.id != href.id) {
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
	if (!i && h->parentIndex[i] == nullindex) {
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
	if (h->tree_mode) {
		if ((h->childCount[i] && h->childIndex[i] == nullindex)
			|| (!h->childCount[i] && h->childIndex[i] != nullindex)) {
			return RED;
		}
		if (h->childIndex[i] != nullindex
			&& h->childIndex[i] >= h->num_objects) {
			return RED;
		}
	} else {
		if (h->childIndex[i] > h->num_objects
			|| h->childCount[i] > h->num_objects
			|| h->childIndex[i] + h->childCount[i] > h->num_objects) {
			return RED;
		}
		if (h->childIndex[i] <= i) {
			return ONG;
		}
	}
	return DFL;
}

static const char *
child_count_color (hierarchy_t *h, uint32_t i)
{
	if (h->childCount[i] > h->num_objects) {
		return RED;
	}
	if (h->tree_mode) {
		if ((h->childCount[i] && h->childIndex[i] == nullindex)
			|| (!h->childCount[i] && h->childIndex[i] != nullindex)) {
			return RED;
		}
	} else {
		if (h->childIndex[i] > h->num_objects
			|| h->childIndex[i] + h->childCount[i] > h->num_objects) {
			return RED;
		}
	}
	return DFL;
}

static bool
check_for_loops (hierref_t href)
{
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, test_reg);
	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (!h->childCount[i]) {
			continue;
		}
		uint32_t n, c;
		for (n = h->childIndex[i], c = h->childCount[i];
			 n != nullindex; n = h->nextIndex[n], c--) {
			if (!c) {
				break;
			}
		}
		if (!c && n != nullindex) {
			printf ("too many children at %d\n", i);
			return false;
		}
		if (c && n == nullindex) {
			printf ("too few children at %d\n", i);
			return false;
		}
		if (c && n != nullindex) {
			printf ("what the what?!? at %d\n", i);
			return false;
		}
	}
	return true;
}

static bool
check_next_index (hierarchy_t *h, uint32_t i)
{
	if (i == 0) {
		// root never has siblings
		if (h->nextIndex[i] != nullindex) {
			return false;
		}
		return true;
	}
	uint32_t    p;
	if ((p = h->parentIndex[i]) >= h->num_objects
		|| h->childIndex[p] > h->num_objects
		|| h->childCount[p] > h->num_objects
		|| h->childIndex[p] + h->childCount[p] > h->num_objects
		|| h->lastIndex[p] >= h->num_objects
		|| (h->nextIndex[i] == nullindex && h->lastIndex[p] != i)
		|| (h->nextIndex[i] != nullindex && h->lastIndex[p] == i)) {
		return false;
	}
	if (h->nextIndex[i] != nullindex && h->parentIndex[h->nextIndex[i]] != p) {
		return false;
	}
	return true;
}

static const char *
next_index_color (hierarchy_t *h, uint32_t i)
{
	return check_next_index (h, i) ? DFL : RED;
}

static bool
check_last_index (hierarchy_t *h, uint32_t i)
{
	if ((h->childCount[i] && h->childIndex[i] >= h->num_objects)
		|| h->childCount[i] >= h->num_objects
		|| (h->childCount[i] && h->lastIndex[i] == nullindex)
		|| (!h->childCount[i] && h->lastIndex[i] != nullindex)) {
		return false;
	}
	if (h->lastIndex[i] != nullindex && h->parentIndex[h->lastIndex[i]] != i) {
		return false;
	}
	return true;
}

static const char *
last_index_color (hierarchy_t *h, uint32_t i)
{
	return check_last_index (h, i) ? DFL : RED;
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
		&& Ent_HasComponent (ent, t_highlight, test_reg)) {
		static char color_str[] = "\e[3.;4.m";
		byte       *color = Ent_GetComponent (ent, t_highlight, test_reg);
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
print_header (hierarchy_t *h)
{
	if (h->tree_mode) {
		puts ("in: ri pa ci cc ni li en|name");
	} else {
		puts ("in: ri pa ci cc en|name");
	}
}

static void
print_line (hierarchy_t *h, uint32_t ind, int level)
{
	ecs_registry_t *reg = h->reg;
	uint32_t    rind = nullindex;
	static char fake_name[] = ONG "null" DFL;
	static char *fake_nameptr = fake_name;
	char      **name = &fake_nameptr;
	if (ECS_EntValid (h->ent[ind], reg)) {
		hierref_t  *ref = Ent_GetComponent (h->ent[ind], t_href, reg);
		rind = ref->index;
		if (Ent_HasComponent (h->ent[ind], t_name, reg)) {
			name = Ent_GetComponent (h->ent[ind], t_name, reg);
		}
	}
	printf ("%2d: %s%2d %s%2d %s%2d %s%2d", ind,
			ref_index_color (ind, rind), rind,
			parent_index_color (h, ind), h->parentIndex[ind],
			child_index_color (h, ind), h->childIndex[ind],
			child_count_color (h, ind), h->childCount[ind]);
	if (h->tree_mode) {
		printf (" %s%2d %s%2d",
				next_index_color (h, ind), h->nextIndex[ind],
				last_index_color (h, ind), h->lastIndex[ind]);
	}
	printf (" %s%2d"DFL"|%*s%s%s"DFL"\n",
			entity_color (h, ind), h->ent[ind],
			level * 3, "", highlight_color (h, ind), *name);
}

static void
dump_hierarchy (hierref_t href)
{
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, test_reg);
	print_header (h);
	for (uint32_t i = 0; i < h->num_objects; i++) {
		print_line (h, i, 0);
	}
	puts ("");
}

static void
dump_tree (hierref_t href, int level)
{
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, test_reg);
	uint32_t    ind = href.index;

	if (ind >= h->num_objects) {
		printf ("index %d out of bounds (%d)\n", ind, h->num_objects);
		return;
	}
	if (!level) {
		print_header (h);
	}
	print_line (h, ind, level);

	if (h->tree_mode) {
		uint32_t    count = h->childCount[ind];
		uint32_t    child;
		for (child = h->childIndex[ind]; count && child != nullindex;
			 child = h->nextIndex[child], count--) {
			hierref_t   cref = {
				.id = href.id,
				.index = child,
			};
			dump_tree (cref, level + 1);
		}
	} else {
		if (h->childIndex[ind] > ind) {
			for (uint32_t i = 0; i < h->childCount[ind]; i++) {
				if (h->childIndex[ind] + i >= h->num_objects) {
					break;
				}
				hierref_t   cref = {
					.id = href.id,
					.index = h->childIndex[ind] + i,
				};
				dump_tree (cref, level + 1);
			}
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
	char      **entname = Ent_GetComponent (ent, t_name, reg);;
	auto href = *(hierref_t *) Ent_GetComponent (ent, t_href, reg);
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, reg);
	if (href.index != index) {
		char      **name = Ent_GetComponent (h->ent[index], t_name, reg);;
		printf ("%s/%s index incorrect: expect %u got %u\n",
				*entname, *name,
				index, href.index);
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

static bool
check_next_last_indices (hierref_t href)
{
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, test_reg);
	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (!check_next_index (h, i)) {
			printf ("incorrect next index at %d: %d\n", i, h->nextIndex[i]);
			return false;
		}
		if (!check_last_index (h, i)) {
			printf ("incorrect last index at %d: %d\n", i, h->lastIndex[i]);
			return false;
		}
	}
	return true;
}

static uint32_t
create_ent (uint32_t parent, const char *name)
{
	uint32_t    ent = ECS_NewEntity (test_reg);
	Ent_SetComponent (ent, t_name, test_reg, &name);
	hierref_t  *ref = Ent_AddComponent (ent, t_href, test_reg);

	if (parent != nullindex) {
		auto pref = *(hierref_t *) Ent_GetComponent (parent, t_href, test_reg);
		*ref = Hierarchy_InsertHierarchy (pref, nullhref, test_reg);
	} else {
		ref->id = Hierarchy_New (test_reg, t_href, 0, 1);
		ref->index = 0;
	}
	hierarchy_t *h = Ent_GetComponent (ref->id, ecs_hierarchy, test_reg);
	h->ent[ref->index] = ent;
	return ent;
}

static void
set_tree_mode (hierref_t href, bool tree_mode)
{
	printf ("set_tree_mode: %s\n", tree_mode ? "true" : "false");
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, test_reg);
	Hierarchy_SetTreeMode (h, tree_mode);
}
#if 0
static void
highlight_ent (uint32_t ent, byte color)
{
	Ent_SetComponent (ent, t_highlight, test_reg, &color);
}

static void
set_parent (uint32_t child, uint32_t parent)
{
	if (parent != nullindex) {
		hierref_t  *pref = Ent_GetComponent (parent, t_href, test_reg);
		hierref_t  *cref = Ent_GetComponent (child, t_href, test_reg);
		Hierarchy_SetParent (pref->hierarchy, pref->index,
							 cref->hierarchy, cref->index);
	} else {
		hierref_t  *cref = Ent_GetComponent (child, t_href, test_reg);
		Hierarchy_SetParent (0, nullindex, cref->hierarchy, cref->index);
	}
}
#endif
static int
test_build_hierarchy (void)
{
	printf ("test_build_hierarchy\n");

	uint32_t    root = create_ent (nullent, "root");
	uint32_t    A = create_ent (root, "A");
	uint32_t    B = create_ent (root, "B");
	uint32_t    C = create_ent (root, "C");

	hierref_t  *ref = Ent_GetComponent (root, t_href, test_reg);

	if (!check_indices (root, 0, nullindex, 1, 3)) { return 1; }
	if (!check_indices (A, 1, 0, 4, 0)) { return 1; }
	if (!check_indices (B, 2, 0, 4, 0)) { return 1; }
	if (!check_indices (C, 3, 0, 4, 0)) { return 1; }

	uint32_t    B1 = create_ent (B, "B1");

	if (!check_indices (root, 0, nullindex, 1, 3)) { return 1; }
	if (!check_indices ( A, 1, 0, 4, 0)) { return 1; }
	if (!check_indices ( B, 2, 0, 4, 1)) { return 1; }
	if (!check_indices ( C, 3, 0, 5, 0)) { return 1; }
	if (!check_indices (B1, 4, 2, 5, 0)) { return 1; }

	uint32_t    A1 = create_ent (A, "A1");

	if (!check_indices (root, 0, nullindex, 1, 3)) { return 1; }
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

	if (!check_hierarchy_size (*ref, 11)) { return 1; }

	if (!check_indices (root, 0, nullindex, 1, 3)) { return 1; }
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

	if (!check_hierarchy_size (*ref, 12)) { return 1; }

	if (!check_indices (root, 0, nullindex, 1, 4)) { return 1; }
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

	dump_hierarchy (*ref);
	uint32_t    C1 = create_ent (C, "C1");
	dump_hierarchy (*ref);
	if (!check_hierarchy_size (*ref, 13)) { return 1; }

	if (!check_indices (root, 0, nullindex, 1, 4)) { return 1; }
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

	dump_tree (*ref, 0);
	set_tree_mode (*ref, true);
	//ref->hierarchy->tree_mode = true;
	dump_hierarchy (*ref);
	dump_tree (*ref, 0);
	if (!check_for_loops (*ref)) { return 1; }
	if (!check_next_last_indices (*ref)) { return 1; }

	create_ent (root, "E");
	create_ent (B1, "B1a");
	create_ent (A2, "A2a");
	dump_hierarchy (*ref);
	dump_tree (*ref, 0);
	if (!check_for_loops (*ref)) { return 1; }
	if (!check_next_last_indices (*ref)) { return 1; }

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (ref->id, test_reg);

	return 0;
}

static int
test_build_hierarchy2 (void)
{
	printf ("test_build_hierarchy2\n");

	uint32_t    root = create_ent (nullent, "root");
	hierref_t  *ref = Ent_GetComponent (root, t_href, test_reg);
	set_tree_mode (*ref, true);
	uint32_t    A = create_ent (root, "A");
	uint32_t    A1 = create_ent (A, "A1");
	uint32_t    A1a = create_ent (A1, "A1a");
	uint32_t    A2 = create_ent (A, "A2");
	uint32_t    B = create_ent (root, "B");
	uint32_t    B1 = create_ent (B, "B1");
	uint32_t    B2 = create_ent (B, "B2");
	uint32_t    B2a = create_ent (B2, "B2a");
	uint32_t    B3 = create_ent (B, "B3");
	uint32_t    C = create_ent (root, "C");
	uint32_t    C1 = create_ent (C, "C1");
	uint32_t    D = create_ent (root, "D");

//check_indices (ent, index, parentIndex, childIndex, childCount)
#define ni nullindex
	if (!check_indices (root, 0, ni,  1, 4)) { return 1; }
	if (!check_indices (  A,  1,  0,  2, 2)) { return 1; }
	if (!check_indices ( A1,  2,  1,  3, 1)) { return 1; }
	if (!check_indices (A1a,  3,  2, ni, 0)) { return 1; }
	if (!check_indices ( A2,  4,  1, ni, 0)) { return 1; }
	if (!check_indices (  B,  5,  0,  6, 3)) { return 1; }
	if (!check_indices ( B1,  6,  5, ni, 0)) { return 1; }
	if (!check_indices ( B2,  7,  5,  8, 1)) { return 1; }
	if (!check_indices (B2a,  8,  7, ni, 0)) { return 1; }
	if (!check_indices ( B3,  9,  5, ni, 0)) { return 1; }
	if (!check_indices (  C, 10,  0, 11, 1)) { return 1; }
	if (!check_indices ( C1, 11, 10, ni, 0)) { return 1; }
	if (!check_indices (  D, 12,  0, ni, 0)) { return 1; }
	if (!check_next_last_indices (*ref)) { return 1; }
	if (!check_for_loops (*ref)) { return 1; }

	dump_hierarchy (*ref);
	dump_tree (*ref, 0);
	set_tree_mode (*ref, true);// shouldn't do anything
	if (!check_indices (root, 0, ni,  1, 4)) { return 1; }
	if (!check_indices (  A,  1,  0,  2, 2)) { return 1; }
	if (!check_indices ( A1,  2,  1,  3, 1)) { return 1; }
	if (!check_indices (A1a,  3,  2, ni, 0)) { return 1; }
	if (!check_indices ( A2,  4,  1, ni, 0)) { return 1; }
	if (!check_indices (  B,  5,  0,  6, 3)) { return 1; }
	if (!check_indices ( B1,  6,  5, ni, 0)) { return 1; }
	if (!check_indices ( B2,  7,  5,  8, 1)) { return 1; }
	if (!check_indices (B2a,  8,  7, ni, 0)) { return 1; }
	if (!check_indices ( B3,  9,  5, ni, 0)) { return 1; }
	if (!check_indices (  C, 10,  0, 11, 1)) { return 1; }
	if (!check_indices ( C1, 11, 10, ni, 0)) { return 1; }
	if (!check_indices (  D, 12,  0, ni, 0)) { return 1; }
	if (!check_next_last_indices (*ref)) { return 1; }
	if (!check_for_loops (*ref)) { return 1; }

ECS_PrintEntity (test_reg, root);
	set_tree_mode (*ref, false);
ECS_PrintEntity (test_reg, root);
	dump_hierarchy (*ref);
	dump_tree (*ref, 0);

	if (!check_indices (root, 0, nullindex, 1, 4)) { return 1; }
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
	Hierarchy_Delete (ref->id, test_reg);

	return 0;
}

int
main (void)
{
	test_reg = ECS_NewRegistry ("tree");
	comp_base = ECS_RegisterComponents (test_reg, test_components,
										test_num_components);
	ECS_CreateComponentPools (test_reg);

	if (test_build_hierarchy ()) { return 1; }
	if (test_build_hierarchy2 ()) { return 1; }

	ECS_DelRegistry (test_reg);

	return 0;
}
