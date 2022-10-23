#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/scene/component.h"
#include "QF/scene/hierarchy.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

scene_t *scene;

// NOTE: these are the columns of the matrix! (not that it matters for a
// symmetrical matrix, but...)
mat4f_t identity = {
	{ 1, 0, 0, 0 },
	{ 0, 1, 0, 0 },
	{ 0, 0, 1, 0 },
	{ 0, 0, 0, 1 },
};
vec4f_t one = { 1, 1, 1, 1 };

static int
vec4_equal (vec4f_t a, vec4f_t b)
{
	vec4i_t     res = a != b;
	return !(res[0] || res[1] || res[2] || res[3]);
}

static int
mat4_equal (const mat4f_t a, const mat4f_t b)
{
	vec4i_t     res = {};

	for (int i = 0; i < 4; i++) {
		res |= a[i] != b[i];
	}
	return !(res[0] || res[1] || res[2] || res[3]);
}

static int
check_hierarchy_size (transform_t t, uint32_t size)
{
	hierarchy_t *h = Transform_GetRef (t)->hierarchy;
	if (h->num_objects != size) {
		printf ("hierarchy does not have exactly %u transform\n", size);
		return 0;
	}
	char      **name = h->components[transform_type_name];
	ecs_registry_t *reg = h->scene->reg;
	for (uint32_t i = 0; i < h->num_objects; i++) {
		hierref_t  *ref = Ent_GetComponent (h->ent[i], scene_href, reg);
		if (ref->hierarchy != h) {
			printf ("transform %d (%s) does not point to hierarchy\n",
					i, name[i]);
		}
	}
	return 1;
}

static void
dump_hierarchy (transform_t t)
{
	hierarchy_t *h = Transform_GetRef (t)->hierarchy;
	char      **name = h->components[transform_type_name];
	ecs_registry_t *reg = h->scene->reg;
	for (uint32_t i = 0; i < h->num_objects; i++) {
		hierref_t  *ref = Ent_GetComponent (h->ent[i], scene_href, reg);
		printf ("%2d: %5s %2u %2u %2u %2u %2u\n", i, name[i], h->ent[i],
				ref->index, h->parentIndex[i],
				h->childIndex[i], h->childCount[i]);
	}
	puts ("");
}

static int
check_indices (transform_t transform, uint32_t index, uint32_t parentIndex,
			   uint32_t childIndex, uint32_t childCount)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	char      **name = h->components[transform_type_name];
	if (ref->index != index) {
		printf ("%s/%s index incorrect: expect %u got %u\n",
				name[ref->index], name[index],
				index, ref->index);
		return 0;
	}
	if (h->parentIndex[index] != parentIndex) {
		printf ("%s parent index incorrect: expect %u got %u\n",
				name[index], parentIndex, h->parentIndex[index]);
		return 0;
	}
	if (h->childIndex[index] != childIndex) {
		printf ("%s child index incorrect: expect %u got %u\n",
				name[index], childIndex, h->childIndex[index]);
		return 0;
	}
	if (h->childCount[index] != childCount) {
		printf ("%s child count incorrect: expect %u got %u\n",
				name[index], childCount, h->childCount[index]);
		return 0;
	}
	return 1;
}

static int
test_single_transform (void)
{
	transform_t transform = Transform_New (scene, (transform_t) {});
	hierarchy_t *h;

	if (!transform.reg || transform.id == nullent) {
		printf ("Transform_New returned null\n");
		return 1;
	}
	if (!Transform_GetRef (transform)) {
		printf ("Transform_GetRef returned null\n");
		return 1;
	}
	if (!(h = Transform_GetRef (transform)->hierarchy)) {
		printf ("New transform has no hierarchy\n");
		return 1;
	}
	mat4f_t    *localMatrix = h->components[transform_type_localMatrix];
	mat4f_t    *localInverse = h->components[transform_type_localInverse];
	mat4f_t    *worldMatrix = h->components[transform_type_worldMatrix];
	mat4f_t    *worldInverse = h->components[transform_type_worldInverse];
	vec4f_t    *localRotation = h->components[transform_type_localRotation];
	vec4f_t    *localScale = h->components[transform_type_localScale];
	if (!check_hierarchy_size (transform, 1)) { return 1; }
	if (!check_indices (transform, 0, nullent, 1, 0)) { return 1; }

	if (!mat4_equal (localMatrix[0], identity)
		|| !mat4_equal (localInverse[0], identity)
		|| !mat4_equal (worldMatrix[0], identity)
		|| !mat4_equal (worldInverse[0], identity)) {
		printf ("New transform matrices not identity\n");
		return 1;
	}

	if (!vec4_equal (localRotation[0], identity[3])
		|| !vec4_equal (localScale[0], one)) {
		printf ("New transform rotation or scale not identity\n");
		return 1;
	}

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (h);

	return 0;
}

static int
test_parent_child_init (void)
{
	transform_t parent = Transform_New (scene, (transform_t) {});
	transform_t child = Transform_New (scene, parent);

	if (Transform_GetRef (parent)->hierarchy
		!= Transform_GetRef (child)->hierarchy) {
		printf ("parent and child transforms have separate hierarchies\n");
		return 1;
	}

	if (!check_hierarchy_size (parent, 2)) { return 1; }

	if (!check_indices (parent, 0, nullent, 1, 1)) { return 1; }
	if (!check_indices (child,  1, 0, 2, 0)) { return 1; }

	hierarchy_t *h = Transform_GetRef (parent)->hierarchy;
	mat4f_t    *localMatrix = h->components[transform_type_localMatrix];
	mat4f_t    *localInverse = h->components[transform_type_localInverse];
	mat4f_t    *worldMatrix = h->components[transform_type_worldMatrix];
	mat4f_t    *worldInverse = h->components[transform_type_worldInverse];
	vec4f_t    *localRotation = h->components[transform_type_localRotation];
	vec4f_t    *localScale = h->components[transform_type_localScale];
	if (!mat4_equal (localMatrix[0], identity)
		|| !mat4_equal (localInverse[0], identity)
		|| !mat4_equal (worldMatrix[0], identity)
		|| !mat4_equal (worldInverse[0], identity)) {
		printf ("Parent transform matrices not identity\n");
		return 1;
	}

	if (!vec4_equal (localRotation[0], identity[3])
		|| !vec4_equal (localScale[0], one)) {
		printf ("Parent transform rotation or scale not identity\n");
		return 1;
	}

	if (!mat4_equal (localMatrix[1], identity)
		|| !mat4_equal (localInverse[1], identity)
		|| !mat4_equal (worldMatrix[1], identity)
		|| !mat4_equal (worldInverse[1], identity)) {
		printf ("Child transform matrices not identity\n");
		return 1;
	}

	if (!vec4_equal (localRotation[1], identity[3])
		|| !vec4_equal (localScale[1], one)) {
		printf ("Child transform rotation or scale not identity\n");
		return 1;
	}

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (Transform_GetRef (parent)->hierarchy);

	return 0;
}

static int
test_parent_child_setparent (void)
{
	transform_t parent = Transform_New (scene, (transform_t) {});
	transform_t child = Transform_New (scene, (transform_t) {});

	Transform_SetName (parent, "parent");
	Transform_SetName (child, "child");

	if (!check_indices (parent, 0, nullent, 1, 0)) { return 1; }
	if (!check_indices (child,  0, nullent, 1, 0)) { return 1; }

	if (Transform_GetRef (parent)->hierarchy
		== Transform_GetRef (child)->hierarchy) {
		printf ("parent and child transforms have same hierarchy before"
				" set paret\n");
		return 1;
	}

	Transform_SetParent (scene, child, parent);

	if (Transform_GetRef (parent)->hierarchy
		!= Transform_GetRef (child)->hierarchy) {
		printf ("parent and child transforms have separate hierarchies\n");
		return 1;
	}

	if (!check_hierarchy_size (parent, 2)) { return 1; }

	if (!check_indices (parent, 0, nullent, 1, 1)) { return 1; }
	if (!check_indices (child,  1, 0, 2, 0)) { return 1; }

	hierarchy_t *h = Transform_GetRef (parent)->hierarchy;
	mat4f_t    *localMatrix = h->components[transform_type_localMatrix];
	mat4f_t    *localInverse = h->components[transform_type_localInverse];
	mat4f_t    *worldMatrix = h->components[transform_type_worldMatrix];
	mat4f_t    *worldInverse = h->components[transform_type_worldInverse];
	vec4f_t    *localRotation = h->components[transform_type_localRotation];
	vec4f_t    *localScale = h->components[transform_type_localScale];
	if (!mat4_equal (localMatrix[0], identity)
		|| !mat4_equal (localInverse[0], identity)
		|| !mat4_equal (worldMatrix[0], identity)
		|| !mat4_equal (worldInverse[0], identity)) {
		printf ("Parent transform matrices not identity\n");
		return 1;
	}

	if (!vec4_equal (localRotation[0], identity[3])
		|| !vec4_equal (localScale[0], one)) {
		printf ("Parent transform rotation or scale not identity\n");
		return 1;
	}

	if (!mat4_equal (localMatrix[1], identity)
		|| !mat4_equal (localInverse[1], identity)
		|| !mat4_equal (worldMatrix[1], identity)
		|| !mat4_equal (worldInverse[1], identity)) {
		printf ("Child transform matrices not identity\n");
		return 1;
	}

	if (!vec4_equal (localRotation[1], identity[3])
		|| !vec4_equal (localScale[1], one)) {
		printf ("Child transform rotation or scale not identity\n");
		return 1;
	}

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (Transform_GetRef (parent)->hierarchy);

	return 0;
}

static int
test_build_hierarchy (void)
{
	printf ("test_build_hierarchy\n");

	transform_t root = Transform_NewNamed (scene, (transform_t) {}, "root");
	transform_t A = Transform_NewNamed (scene, root, "A");
	transform_t B = Transform_NewNamed (scene, root, "B");
	transform_t C = Transform_NewNamed (scene, root, "C");

	if (!check_indices (root, 0, nullent, 1, 3)) { return 1; }
	if (!check_indices (A, 1, 0, 4, 0)) { return 1; }
	if (!check_indices (B, 2, 0, 4, 0)) { return 1; }
	if (!check_indices (C, 3, 0, 4, 0)) { return 1; }

	transform_t B1 = Transform_NewNamed (scene, B, "B1");

	if (!check_indices (root, 0, nullent, 1, 3)) { return 1; }
	if (!check_indices ( A, 1, 0, 4, 0)) { return 1; }
	if (!check_indices ( B, 2, 0, 4, 1)) { return 1; }
	if (!check_indices ( C, 3, 0, 5, 0)) { return 1; }
	if (!check_indices (B1, 4, 2, 5, 0)) { return 1; }

	transform_t A1 = Transform_NewNamed (scene, A, "A1");

	if (!check_indices (root, 0, nullent, 1, 3)) { return 1; }
	if (!check_indices ( A, 1, 0, 4, 1)) { return 1; }
	if (!check_indices ( B, 2, 0, 5, 1)) { return 1; }
	if (!check_indices ( C, 3, 0, 6, 0)) { return 1; }
	if (!check_indices (A1, 4, 1, 6, 0)) { return 1; }
	if (!check_indices (B1, 5, 2, 6, 0)) { return 1; }
	transform_t A1a = Transform_NewNamed (scene, A1, "A1a");
	transform_t B2 = Transform_NewNamed (scene, B, "B2");
	transform_t A2 = Transform_NewNamed (scene, A, "A2");
	transform_t B3 = Transform_NewNamed (scene, B, "B3");
	transform_t B2a = Transform_NewNamed (scene, B2, "B2a");

	if (!check_hierarchy_size (root, 11)) { return 1; }

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

	transform_t D = Transform_NewNamed (scene, root, "D");

	if (!check_hierarchy_size (root, 12)) { return 1; }

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

	dump_hierarchy (root);
	transform_t C1 = Transform_NewNamed (scene, C, "C1");
	dump_hierarchy (root);
	if (!check_hierarchy_size (root, 13)) { return 1; }

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
	Hierarchy_Delete (Transform_GetRef (root)->hierarchy);

	return 0;
}

static int
test_build_hierarchy2 (void)
{
	printf ("test_build_hierarchy2\n");

	transform_t root = Transform_NewNamed (scene, (transform_t) {}, "root");
	transform_t A = Transform_NewNamed (scene, root, "A");
	transform_t B = Transform_NewNamed (scene, root, "B");
	transform_t C = Transform_NewNamed (scene, root, "C");
	transform_t B1 = Transform_NewNamed (scene, B, "B1");
	transform_t A1 = Transform_NewNamed (scene, A, "A1");
	transform_t A1a = Transform_NewNamed (scene, A1, "A1a");
	transform_t B2 = Transform_NewNamed (scene, B, "B2");
	transform_t A2 = Transform_NewNamed (scene, A, "A2");
	transform_t B3 = Transform_NewNamed (scene, B, "B3");
	transform_t B2a = Transform_NewNamed (scene, B2, "B2a");
	transform_t D = Transform_NewNamed (scene, root, "D");
	transform_t C1 = Transform_NewNamed (scene, C, "C1");

	if (!check_hierarchy_size (root, 13)) { return 1; }

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

	transform_t T = Transform_NewNamed (scene, (transform_t) {}, "T");
	transform_t X = Transform_NewNamed (scene, T, "X");
	transform_t Y = Transform_NewNamed (scene, T, "Y");
	transform_t Z = Transform_NewNamed (scene, T, "Z");
	transform_t Y1 = Transform_NewNamed (scene, Y, "Y1");
	transform_t X1 = Transform_NewNamed (scene, X, "X1");
	transform_t X1a = Transform_NewNamed (scene, X1, "X1a");
	transform_t Y2 = Transform_NewNamed (scene, Y, "Y2");
	transform_t X2 = Transform_NewNamed (scene, X, "X2");
	transform_t Y3 = Transform_NewNamed (scene, Y, "Y3");
	transform_t Y2a = Transform_NewNamed (scene, Y2, "Y2a");
	transform_t Z1 = Transform_NewNamed (scene, Z, "Z1");

	dump_hierarchy (T);
	if (!check_hierarchy_size (T, 12)) { return 1; }

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

	Transform_SetParent (scene, T, B);

	dump_hierarchy (root);

	if (!check_hierarchy_size (root, 25)) { return 1; }

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

	Transform_SetParent (scene, Y, (transform_t) {});

	dump_hierarchy (root);
	dump_hierarchy (Y);
	if (!check_hierarchy_size (root, 20)) { return 1; }
	if (!check_hierarchy_size (Y, 5)) { return 1; }

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
	Hierarchy_Delete (Transform_GetRef (root)->hierarchy);
	Hierarchy_Delete (Transform_GetRef (Y)->hierarchy);

	return 0;
}

static int
check_vector (transform_t transform,
			  vec4f_t (*func) (transform_t t),
			  vec4f_t expect, const char *msg)
{
	vec4f_t     res = func(transform);
	if (!vec4_equal (res, expect)) {
		printf ("%s %s: expected "VEC4F_FMT" got "VEC4F_FMT"\n",
				Transform_GetName (transform), msg,
				VEC4_EXP (expect), VEC4_EXP (res));
		return 0;
	}
	return 1;
}

static int
test_frames (void)
{
	transform_t root = Transform_NewNamed (scene, (transform_t) {}, "root");
	transform_t A = Transform_NewNamed (scene, root, "A");
	transform_t B = Transform_NewNamed (scene, root, "B");
	transform_t A1 = Transform_NewNamed (scene, A, "A1");
	transform_t B1 = Transform_NewNamed (scene, B, "B1");

	Transform_SetLocalPosition (root, (vec4f_t) { 0, 0, 1, 1 });
	Transform_SetLocalPosition (A, (vec4f_t) { 1, 0, 0, 1 });
	Transform_SetLocalRotation (A, (vec4f_t) { 0.5, 0.5, 0.5, 0.5 });
	Transform_SetLocalPosition (B, (vec4f_t) { 0, 1, 0, 1 });
	Transform_SetLocalRotation (B, (vec4f_t) { 0.5, -0.5, 0.5, 0.5 });
	Transform_SetLocalPosition (A1, (vec4f_t) { 1, 0, 0, 1 });
	Transform_SetLocalRotation (A1, (vec4f_t) { -0.5, -0.5, -0.5, 0.5 });
	Transform_SetLocalPosition (B1, (vec4f_t) { 0, 1, 0, 1 });
	Transform_SetLocalRotation (B1, (vec4f_t) { -0.5, 0.5, -0.5, 0.5 });

	hierarchy_t *h = Transform_GetRef (root)->hierarchy;
	mat4f_t    *localMatrix = h->components[transform_type_localMatrix];
	mat4f_t    *localInverse = h->components[transform_type_localInverse];
	mat4f_t    *worldMatrix = h->components[transform_type_worldMatrix];
	mat4f_t    *worldInverse = h->components[transform_type_worldInverse];
	char      **name = h->components[transform_type_name];
	for (uint32_t i = 0; i < h->num_objects; i++) {
		mat4f_t     res;
		mmulf (res, localMatrix[i], localInverse[i]);
		if (!mat4_equal (res, identity)) {
			printf ("%s: localInverse not inverse of localMatrix\n",
					name[i]);
			printf ("l: " VEC4F_FMT "\n", MAT4_ROW(localMatrix[i], 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(localMatrix[i], 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(localMatrix[i], 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(localMatrix[i], 3));
			printf ("i: " VEC4F_FMT "\n", MAT4_ROW(localInverse[i], 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(localInverse[i], 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(localInverse[i], 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(localInverse[i], 3));
			printf ("r: " VEC4F_FMT "\n", MAT4_ROW(res, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 3));
			return 1;
		}
		puts (name[i]);
		printf ("l: " VEC4F_FMT "\n", MAT4_ROW(localMatrix[i], 0));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(localMatrix[i], 1));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(localMatrix[i], 2));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(localMatrix[i], 3));
		printf ("i: " VEC4F_FMT "\n", MAT4_ROW(localInverse[i], 0));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(localInverse[i], 1));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(localInverse[i], 2));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(localInverse[i], 3));
	}
	for (uint32_t i = 0; i < h->num_objects; i++) {
		mat4f_t     res;
		mmulf (res, worldMatrix[i], worldInverse[i]);
		if (!mat4_equal (res, identity)) {
			printf ("%s: worldInverse not inverse of worldMatrix\n",
					name[i]);
			printf ("l: " VEC4F_FMT "\n", MAT4_ROW(worldMatrix[i], 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldMatrix[i], 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldMatrix[i], 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldMatrix[i], 3));
			printf ("i: " VEC4F_FMT "\n", MAT4_ROW(worldInverse[i], 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldInverse[i], 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldInverse[i], 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldInverse[i], 3));
			printf ("r: " VEC4F_FMT "\n", MAT4_ROW(res, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 3));
			return 1;
		}
		puts (name[i]);
		printf ("l: " VEC4F_FMT "\n", MAT4_ROW(worldMatrix[i], 0));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldMatrix[i], 1));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldMatrix[i], 2));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldMatrix[i], 3));
		printf ("i: " VEC4F_FMT "\n", MAT4_ROW(worldInverse[i], 0));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldInverse[i], 1));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldInverse[i], 2));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(worldInverse[i], 3));
	}

	if (!check_vector (root, Transform_GetLocalPosition,
					   (vec4f_t) { 0, 0, 1, 1 }, "local position")) {
		return 1;
	}
	if (!check_vector (root, Transform_GetWorldPosition,
					   (vec4f_t) { 0, 0, 1, 1 }, "world position")) {
		return 1;
	}
	if (!check_vector (root, Transform_Forward, (vec4f_t) { 1, 0, 0, 0 },
					   "forward")) {
		return 1;
	}
	if (!check_vector (root, Transform_Right, (vec4f_t) { 0, -1, 0, 0 },
					   "right")) {
		return 1;
	}
	if (!check_vector (root, Transform_Up, (vec4f_t) { 0, 0, 1, 0 },
					   "up")) {
		return 1;
	}

	if (!check_vector (A, Transform_GetLocalPosition, (vec4f_t) { 1, 0, 0, 1 },
					   "local position")) {
		return 1;
	}
	if (!check_vector (A, Transform_GetWorldPosition, (vec4f_t) { 1, 0, 1, 1 },
					   "world position")) {
		return 1;
	}
	if (!check_vector (A, Transform_Forward, (vec4f_t) { 0, 1, 0, 0 },
					   "forward")) {
		return 1;
	}
	if (!check_vector (A, Transform_Right, (vec4f_t) { 0, 0, -1, 0 },
					   "right")) {
		return 1;
	}
	if (!check_vector (A, Transform_Up, (vec4f_t) { 1, 0, 0, 0 },
					   "up")) {
		return 1;
	}
	if (!check_vector (A1, Transform_GetLocalPosition, (vec4f_t) { 1, 0, 0, 1 },
					   "local position")) {
		return 1;
	}
	if (!check_vector (A1, Transform_GetWorldPosition, (vec4f_t) { 1, 1, 1, 1 },
					   "world position")) {
		return 1;
	}
	if (!check_vector (A1, Transform_Forward, (vec4f_t) { 1, 0, 0, 0 },
					   "forward")) {
		return 1;
	}
	if (!check_vector (A1, Transform_Right, (vec4f_t) { 0, -1, 0, 0 },
					   "right")) {
		return 1;
	}
	if (!check_vector (A1, Transform_Up, (vec4f_t) { 0, 0, 1, 0 },
					   "up")) {
		return 1;
	}

	if (!check_vector (B, Transform_GetLocalPosition, (vec4f_t) { 0, 1, 0, 1 },
					   "local position")) {
		return 1;
	}
	if (!check_vector (B, Transform_GetWorldPosition, (vec4f_t) { 0, 1, 1, 1 },
					   "world position")) {
		return 1;
	}
	if (!check_vector (B, Transform_Forward, (vec4f_t) { 0, 0, 1, 0 },
					   "forward")) {
		return 1;
	}
	if (!check_vector (B, Transform_Right, (vec4f_t) { 1, 0, 0, 0 },
					   "right")) {
		return 1;
	}
	if (!check_vector (B, Transform_Up, (vec4f_t) { 0,-1, 0, 0 },
					   "up")) {
		return 1;
	}
	if (!check_vector (B1, Transform_GetLocalPosition, (vec4f_t) { 0, 1, 0, 1 },
					   "local position")) {
		return 1;
	}
	if (!check_vector (B1, Transform_GetWorldPosition, (vec4f_t) {-1, 1, 1, 1 },
					   "world position")) {
		return 1;
	}
	if (!check_vector (B1, Transform_Forward, (vec4f_t) { 1, 0, 0, 0 },
					   "forward")) {
		return 1;
	}
	if (!check_vector (B1, Transform_Right, (vec4f_t) { 0, -1, 0, 0 },
					   "right")) {
		return 1;
	}
	if (!check_vector (B1, Transform_Up, (vec4f_t) { 0, 0, 1, 0 },
					   "up")) {
		return 1;
	}

	Transform_Delete (scene,root);

	return 0;
}

int
main (void)
{
	scene = Scene_NewScene ();

	if (test_single_transform ()) { return 1; }
	if (test_parent_child_init ()) { return 1; }
	if (test_parent_child_setparent ()) { return 1; }
	if (test_build_hierarchy ()) { return 1; }
	if (test_build_hierarchy2 ()) { return 1; }
	if (test_frames ()) { return 1; }

	Scene_DeleteScene (scene);

	return 0;
}
