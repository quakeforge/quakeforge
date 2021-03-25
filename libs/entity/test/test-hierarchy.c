#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/entity.h"

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
check_hierarchy_size (hierarchy_t *h, uint32_t size)
{
	if (h->transform.size != size
		|| h->entity.size != size
		|| h->childCount.size != size
		|| h->childIndex.size != size
		|| h->parentIndex.size != size
		|| h->name.size != size
		|| h->tag.size != size
		|| h->modified.size != size
		|| h->localMatrix.size != size
		|| h->localInverse.size != size
		|| h->worldMatrix.size != size
		|| h->worldInverse.size != size
		|| h->localRotation.size != size
		|| h->localScale.size != size
		|| h->worldRotation.size != size
		|| h->worldScale.size != size) {
		printf ("hierarchy does not have exactly %u"
				" transform or array sizes are inconsistent\n", size);
		return 0;
	}
	for (size_t i = 0; i < h->transform.size; i++) {
		if (h->transform.a[i]->hierarchy != h) {
			printf ("transform %zd (%s) does not point to hierarchy\n",
					i, h->name.a[i]);
		}
	}
	return 1;
}

static void
dump_hierarchy (hierarchy_t *h)
{
	for (size_t i = 0; i < h->transform.size; i++) {
		printf ("%2zd: %5s %2u %2u %2u %2u\n", i, h->name.a[i],
				h->transform.a[i]->index, h->parentIndex.a[i],
				h->childIndex.a[i], h->childCount.a[i]);
	}
	puts ("");
}

static int
check_indices (transform_t *transform, uint32_t index, uint32_t parentIndex,
			   uint32_t childIndex, uint32_t childCount)
{
	hierarchy_t *h = transform->hierarchy;
	if (transform->index != index) {
		printf ("%s/%s index incorrect: expect %u got %u\n",
				h->name.a[transform->index], h->name.a[index],
				index, transform->index);
		return 0;
	}
	if (h->parentIndex.a[index] != parentIndex) {
		printf ("%s parent index incorrect: expect %u got %u\n",
				h->name.a[index], parentIndex, h->parentIndex.a[index]);
		return 0;
	}
	if (h->childIndex.a[index] != childIndex) {
		printf ("%s child index incorrect: expect %u got %u\n",
				h->name.a[index], childIndex, h->childIndex.a[index]);
		return 0;
	}
	if (h->childCount.a[index] != childCount) {
		printf ("%s child count incorrect: expect %u got %u\n",
				h->name.a[index], childCount, h->childCount.a[index]);
		return 0;
	}
	return 1;
}

static int
test_single_transform (void)
{
	transform_t *transform = Transform_New (0);
	hierarchy_t *h;

	if (!transform) {
		printf ("Transform_New returned null\n");
		return 1;
	}
	if (!(h = transform->hierarchy)) {
		printf ("New transform has no hierarchy\n");
		return 1;
	}
	if (!check_hierarchy_size (h, 1)) { return 1; }
	if (!check_indices (transform, 0, null_transform, 1, 0)) { return 1; }

	if (!mat4_equal (h->localMatrix.a[0], identity)
		|| !mat4_equal (h->localInverse.a[0], identity)
		|| !mat4_equal (h->worldMatrix.a[0], identity)
		|| !mat4_equal (h->worldInverse.a[0], identity)) {
		printf ("New transform matrices not identity\n");
		return 1;
	}

	if (!vec4_equal (h->localRotation.a[0], identity[3])
		|| !vec4_equal (h->localScale.a[0], one)) {
		printf ("New transform rotation or scale not identity\n");
		return 1;
	}

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (transform->hierarchy);

	return 0;
}

static int
test_parent_child_init (void)
{
	transform_t *parent = Transform_New (0);
	transform_t *child = Transform_New (parent);

	if (parent->hierarchy != child->hierarchy) {
		printf ("parent and child transforms have separate hierarchies\n");
		return 1;
	}

	if (!check_hierarchy_size (parent->hierarchy, 2)) { return 1; }

	if (!check_indices (parent, 0, null_transform, 1, 1)) { return 1; }
	if (!check_indices (child,  1, 0, 2, 0)) { return 1; }

	hierarchy_t *h = parent->hierarchy;
	if (!mat4_equal (h->localMatrix.a[0], identity)
		|| !mat4_equal (h->localInverse.a[0], identity)
		|| !mat4_equal (h->worldMatrix.a[0], identity)
		|| !mat4_equal (h->worldInverse.a[0], identity)) {
		printf ("Parent transform matrices not identity\n");
		return 1;
	}

	if (!vec4_equal (h->localRotation.a[0], identity[3])
		|| !vec4_equal (h->localScale.a[0], one)) {
		printf ("Parent transform rotation or scale not identity\n");
		return 1;
	}

	if (!mat4_equal (h->localMatrix.a[1], identity)
		|| !mat4_equal (h->localInverse.a[1], identity)
		|| !mat4_equal (h->worldMatrix.a[1], identity)
		|| !mat4_equal (h->worldInverse.a[1], identity)) {
		printf ("Child transform matrices not identity\n");
		return 1;
	}

	if (!vec4_equal (h->localRotation.a[1], identity[3])
		|| !vec4_equal (h->localScale.a[1], one)) {
		printf ("Child transform rotation or scale not identity\n");
		return 1;
	}

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (parent->hierarchy);

	return 0;
}

static int
test_parent_child_setparent (void)
{
	transform_t *parent = Transform_New (0);
	transform_t *child = Transform_New (0);

	Transform_SetName (parent, "parent");
	Transform_SetName (child, "child");

	if (!check_indices (parent, 0, null_transform, 1, 0)) { return 1; }
	if (!check_indices (child,  0, null_transform, 1, 0)) { return 1; }

	if (parent->hierarchy == child->hierarchy) {
		printf ("parent and child transforms have same hierarchy before"
				" set paret\n");
		return 1;
	}

	Transform_SetParent (child, parent);

	if (parent->hierarchy != child->hierarchy) {
		printf ("parent and child transforms have separate hierarchies\n");
		return 1;
	}

	if (!check_hierarchy_size (parent->hierarchy, 2)) { return 1; }

	if (!check_indices (parent, 0, null_transform, 1, 1)) { return 1; }
	if (!check_indices (child,  1, 0, 2, 0)) { return 1; }

	hierarchy_t *h = parent->hierarchy;
	if (!mat4_equal (h->localMatrix.a[0], identity)
		|| !mat4_equal (h->localInverse.a[0], identity)
		|| !mat4_equal (h->worldMatrix.a[0], identity)
		|| !mat4_equal (h->worldInverse.a[0], identity)) {
		printf ("Parent transform matrices not identity\n");
		return 1;
	}

	if (!vec4_equal (h->localRotation.a[0], identity[3])
		|| !vec4_equal (h->localScale.a[0], one)) {
		printf ("Parent transform rotation or scale not identity\n");
		return 1;
	}

	if (!mat4_equal (h->localMatrix.a[1], identity)
		|| !mat4_equal (h->localInverse.a[1], identity)
		|| !mat4_equal (h->worldMatrix.a[1], identity)
		|| !mat4_equal (h->worldInverse.a[1], identity)) {
		printf ("Child transform matrices not identity\n");
		return 1;
	}

	if (!vec4_equal (h->localRotation.a[1], identity[3])
		|| !vec4_equal (h->localScale.a[1], one)) {
		printf ("Child transform rotation or scale not identity\n");
		return 1;
	}

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (parent->hierarchy);

	return 0;
}

static int
test_build_hierarchy (void)
{
	printf ("test_build_hierarchy\n");

	transform_t *root = Transform_NewNamed (0, "root");
	transform_t *A = Transform_NewNamed (root, "A");
	transform_t *B = Transform_NewNamed (root, "B");
	transform_t *C = Transform_NewNamed (root, "C");

	if (!check_indices (root, 0, null_transform, 1, 3)) { return 1; }
	if (!check_indices (A, 1, 0, 4, 0)) { return 1; }
	if (!check_indices (B, 2, 0, 4, 0)) { return 1; }
	if (!check_indices (C, 3, 0, 4, 0)) { return 1; }

	transform_t *B1 = Transform_NewNamed (B, "B1");

	if (!check_indices (root, 0, null_transform, 1, 3)) { return 1; }
	if (!check_indices ( A, 1, 0, 4, 0)) { return 1; }
	if (!check_indices ( B, 2, 0, 4, 1)) { return 1; }
	if (!check_indices ( C, 3, 0, 5, 0)) { return 1; }
	if (!check_indices (B1, 4, 2, 5, 0)) { return 1; }

	transform_t *A1 = Transform_NewNamed (A, "A1");

	if (!check_indices (root, 0, null_transform, 1, 3)) { return 1; }
	if (!check_indices ( A, 1, 0, 4, 1)) { return 1; }
	if (!check_indices ( B, 2, 0, 5, 1)) { return 1; }
	if (!check_indices ( C, 3, 0, 6, 0)) { return 1; }
	if (!check_indices (A1, 4, 1, 6, 0)) { return 1; }
	if (!check_indices (B1, 5, 2, 6, 0)) { return 1; }
	transform_t *A1a = Transform_NewNamed (A1, "A1a");
	transform_t *B2 = Transform_NewNamed (B, "B2");
	transform_t *A2 = Transform_NewNamed (A, "A2");
	transform_t *B3 = Transform_NewNamed (B, "B3");
	transform_t *B2a = Transform_NewNamed (B2, "B2a");

	if (!check_hierarchy_size (root->hierarchy, 11)) { return 1; }

	if (!check_indices (root, 0, null_transform, 1, 3)) { return 1; }
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

	transform_t *D = Transform_NewNamed (root, "D");

	if (!check_hierarchy_size (root->hierarchy, 12)) { return 1; }

	if (!check_indices (root, 0, null_transform, 1, 4)) { return 1; }
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

	dump_hierarchy (root->hierarchy);
	transform_t *C1 = Transform_NewNamed (C, "C1");
	dump_hierarchy (root->hierarchy);
	if (!check_hierarchy_size (root->hierarchy, 13)) { return 1; }

	if (!check_indices (root, 0, null_transform, 1, 4)) { return 1; }
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
	Hierarchy_Delete (root->hierarchy);

	return 0;
}

static int
test_build_hierarchy2 (void)
{
	printf ("test_build_hierarchy2\n");

	transform_t *root = Transform_NewNamed (0, "root");
	transform_t *A = Transform_NewNamed (root, "A");
	transform_t *B = Transform_NewNamed (root, "B");
	transform_t *C = Transform_NewNamed (root, "C");
	transform_t *B1 = Transform_NewNamed (B, "B1");
	transform_t *A1 = Transform_NewNamed (A, "A1");
	transform_t *A1a = Transform_NewNamed (A1, "A1a");
	transform_t *B2 = Transform_NewNamed (B, "B2");
	transform_t *A2 = Transform_NewNamed (A, "A2");
	transform_t *B3 = Transform_NewNamed (B, "B3");
	transform_t *B2a = Transform_NewNamed (B2, "B2a");
	transform_t *D = Transform_NewNamed (root, "D");
	transform_t *C1 = Transform_NewNamed (C, "C1");

	if (!check_hierarchy_size (root->hierarchy, 13)) { return 1; }

	if (!check_indices (root, 0, null_transform, 1, 4)) { return 1; }
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

	transform_t *T = Transform_NewNamed (0, "T");
	transform_t *X = Transform_NewNamed (T, "X");
	transform_t *Y = Transform_NewNamed (T, "Y");
	transform_t *Z = Transform_NewNamed (T, "Z");
	transform_t *Y1 = Transform_NewNamed (Y, "Y1");
	transform_t *X1 = Transform_NewNamed (X, "X1");
	transform_t *X1a = Transform_NewNamed (X1, "X1a");
	transform_t *Y2 = Transform_NewNamed (Y, "Y2");
	transform_t *X2 = Transform_NewNamed (X, "X2");
	transform_t *Y3 = Transform_NewNamed (Y, "Y3");
	transform_t *Y2a = Transform_NewNamed (Y2, "Y2a");
	transform_t *Z1 = Transform_NewNamed (Z, "Z1");

	dump_hierarchy (T->hierarchy);
	if (!check_hierarchy_size (T->hierarchy, 12)) { return 1; }

	if (!check_indices (  T,  0, null_transform, 1, 3)) { return 1; }
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

	Transform_SetParent (T, B);

	dump_hierarchy (root->hierarchy);

	if (!check_hierarchy_size (root->hierarchy, 25)) { return 1; }

	if (!check_indices (root, 0, null_transform, 1, 4)) { return 1; }
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

	Transform_SetParent (Y, 0);

	dump_hierarchy (root->hierarchy);
	dump_hierarchy (Y->hierarchy);
	if (!check_hierarchy_size (root->hierarchy, 20)) { return 1; }
	if (!check_hierarchy_size (Y->hierarchy, 5)) { return 1; }

	if (!check_indices (root, 0, null_transform, 1, 4)) { return 1; }
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

	if (!check_indices (  Y, 0, null_transform, 1, 3)) { return 1; }
	if (!check_indices ( Y1, 1, 0, 4, 0)) { return 1; }
	if (!check_indices ( Y2, 2, 0, 4, 1)) { return 1; }
	if (!check_indices ( Y3, 3, 0, 5, 0)) { return 1; }
	if (!check_indices (Y2a, 4, 2, 5, 0)) { return 1; }

	// Delete the hierarchy directly as setparent isn't fully tested
	Hierarchy_Delete (root->hierarchy);
	Hierarchy_Delete (Y->hierarchy);

	return 0;
}

static int
check_vector (const transform_t *transform,
			  vec4f_t (*func) (const transform_t *t),
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
	transform_t *root = Transform_NewNamed (0, "root");
	transform_t *A = Transform_NewNamed (root, "A");
	transform_t *B = Transform_NewNamed (root, "B");
	transform_t *A1 = Transform_NewNamed (A, "A1");
	transform_t *B1 = Transform_NewNamed (B, "B1");

	Transform_SetLocalPosition (root, (vec4f_t) { 0, 0, 1, 1 });
	Transform_SetLocalPosition (A, (vec4f_t) { 1, 0, 0, 1 });
	Transform_SetLocalRotation (A, (vec4f_t) { 0.5, 0.5, 0.5, 0.5 });
	Transform_SetLocalPosition (B, (vec4f_t) { 0, 1, 0, 1 });
	Transform_SetLocalRotation (B, (vec4f_t) { 0.5, -0.5, 0.5, 0.5 });
	Transform_SetLocalPosition (A1, (vec4f_t) { 1, 0, 0, 1 });
	Transform_SetLocalRotation (A1, (vec4f_t) { -0.5, -0.5, -0.5, 0.5 });
	Transform_SetLocalPosition (B1, (vec4f_t) { 0, 1, 0, 1 });
	Transform_SetLocalRotation (B1, (vec4f_t) { -0.5, 0.5, -0.5, 0.5 });

	hierarchy_t *h = root->hierarchy;
	for (size_t i = 0; i < h->transform.size; i++) {
		mat4f_t     res;
		mmulf (res, h->localMatrix.a[i], h->localInverse.a[i]);
		if (!mat4_equal (res, identity)) {
			printf ("%s: localInverse not inverse of localMatrix\n",
					h->name.a[i]);
			printf ("l: " VEC4F_FMT "\n", MAT4_ROW(h->localMatrix.a[i], 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localMatrix.a[i], 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localMatrix.a[i], 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localMatrix.a[i], 3));
			printf ("i: " VEC4F_FMT "\n", MAT4_ROW(h->localInverse.a[i], 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localInverse.a[i], 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localInverse.a[i], 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localInverse.a[i], 3));
			printf ("r: " VEC4F_FMT "\n", MAT4_ROW(res, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 3));
			return 1;
		}
		puts (h->name.a[i]);
		printf ("l: " VEC4F_FMT "\n", MAT4_ROW(h->localMatrix.a[i], 0));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localMatrix.a[i], 1));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localMatrix.a[i], 2));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localMatrix.a[i], 3));
		printf ("i: " VEC4F_FMT "\n", MAT4_ROW(h->localInverse.a[i], 0));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localInverse.a[i], 1));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localInverse.a[i], 2));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->localInverse.a[i], 3));
	}
	for (size_t i = 0; i < h->transform.size; i++) {
		mat4f_t     res;
		mmulf (res, h->worldMatrix.a[i], h->worldInverse.a[i]);
		if (!mat4_equal (res, identity)) {
			printf ("%s: worldInverse not inverse of worldMatrix\n",
					h->name.a[i]);
			printf ("l: " VEC4F_FMT "\n", MAT4_ROW(h->worldMatrix.a[i], 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldMatrix.a[i], 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldMatrix.a[i], 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldMatrix.a[i], 3));
			printf ("i: " VEC4F_FMT "\n", MAT4_ROW(h->worldInverse.a[i], 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldInverse.a[i], 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldInverse.a[i], 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldInverse.a[i], 3));
			printf ("r: " VEC4F_FMT "\n", MAT4_ROW(res, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(res, 3));
			return 1;
		}
		puts (h->name.a[i]);
		printf ("l: " VEC4F_FMT "\n", MAT4_ROW(h->worldMatrix.a[i], 0));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldMatrix.a[i], 1));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldMatrix.a[i], 2));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldMatrix.a[i], 3));
		printf ("i: " VEC4F_FMT "\n", MAT4_ROW(h->worldInverse.a[i], 0));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldInverse.a[i], 1));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldInverse.a[i], 2));
		printf ("   " VEC4F_FMT "\n", MAT4_ROW(h->worldInverse.a[i], 3));
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

	Transform_Delete (root);

	return 0;
}

int
main (void)
{
	if (test_single_transform ()) { return 1; }
	if (test_parent_child_init ()) { return 1; }
	if (test_parent_child_setparent ()) { return 1; }
	if (test_build_hierarchy ()) { return 1; }
	if (test_build_hierarchy2 ()) { return 1; }
	if (test_frames ()) { return 1; }

	return 0;
}
