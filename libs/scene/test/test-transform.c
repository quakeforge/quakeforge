#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/ecs/component.h"
#include "QF/ecs/hierarchy.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

ecs_system_t ssys;

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
	auto href = Transform_GetRef (t);
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, t.reg);
	if (h->num_objects != size) {
		printf ("hierarchy does not have exactly %u transform\n", size);
		return 0;
	}
	char      **name = h->components[transform_type_name];
	for (uint32_t i = 0; i < h->num_objects; i++) {
		auto ref = *(hierref_t *) Ent_GetComponent (h->ent[i], ssys.base + scene_href, ssys.reg);
		if (ref.id != href.id) {
			printf ("transform %d (%s) does not point to hierarchy\n",
					i, name[i]);
		}
	}
	return 1;
}

static int
check_indices (transform_t transform, uint32_t index, uint32_t parentIndex,
			   uint32_t childIndex, uint32_t childCount)
{
	auto href = Transform_GetRef (transform);
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, ssys.reg);
	char      **name = h->components[transform_type_name];
	if (href.index != index) {
		printf ("%s/%s index incorrect: expect %u got %u\n",
				name[href.index], name[index],
				index, href.index);
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
	transform_t transform = Transform_New (ssys, (transform_t) {});

	if (!transform.reg || transform.id == nullent) {
		printf ("Transform_New returned null\n");
		return 1;
	}
	auto href = Transform_GetRef (transform);
	if (!ECS_EntValid (href.id, ssys.reg)) {
		printf ("New transform has invalid hierarchy reference\n");
		return 1;
	}
	hierarchy_t *h = Ent_GetComponent (href.id, ecs_hierarchy, ssys.reg);

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
	Hierarchy_Delete (href.id, ssys.reg);

	return 0;
}

static int
test_parent_child_init (void)
{
	transform_t parent = Transform_New (ssys, (transform_t) {});
	transform_t child = Transform_New (ssys, parent);

	if (Transform_GetRef (parent).id != Transform_GetRef (child).id) {
		printf ("parent and child transforms have separate hierarchies\n");
		return 1;
	}

	if (!check_hierarchy_size (parent, 2)) { return 1; }

	if (!check_indices (parent, 0, nullent, 1, 1)) { return 1; }
	if (!check_indices (child,  1, 0, 2, 0)) { return 1; }

	auto pref = Transform_GetRef (parent);
	hierarchy_t *h = Ent_GetComponent (pref.id, ecs_hierarchy, ssys.reg);
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
	Hierarchy_Delete (Transform_GetRef (parent).id, ssys.reg);

	return 0;
}

static int
test_parent_child_setparent (void)
{
	transform_t parent = Transform_New (ssys, (transform_t) {});
	transform_t child = Transform_New (ssys, (transform_t) {});

	Transform_SetName (parent, "parent");
	Transform_SetName (child, "child");

	if (!check_indices (parent, 0, nullent, 1, 0)) { return 1; }
	if (!check_indices (child,  0, nullent, 1, 0)) { return 1; }

	if (Transform_GetRef (parent).id == Transform_GetRef (child).id) {
		printf ("parent and child transforms have same hierarchy before"
				" set paret\n");
		return 1;
	}

	Transform_SetParent (child, parent);

	if (Transform_GetRef (parent).id != Transform_GetRef (child).id) {
		printf ("parent and child transforms have separate hierarchies\n");
		return 1;
	}

	if (!check_hierarchy_size (parent, 2)) { return 1; }

	if (!check_indices (parent, 0, nullent, 1, 1)) { return 1; }
	if (!check_indices (child,  1, 0, 2, 0)) { return 1; }

	auto pref = Transform_GetRef (parent);
	hierarchy_t *h = Ent_GetComponent (pref.id, ecs_hierarchy, ssys.reg);
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
	Hierarchy_Delete (Transform_GetRef (parent).id, ssys.reg);

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
	transform_t root = Transform_NewNamed (ssys, (transform_t) {}, "root");
	transform_t A = Transform_NewNamed (ssys, root, "A");
	transform_t B = Transform_NewNamed (ssys, root, "B");
	transform_t A1 = Transform_NewNamed (ssys, A, "A1");
	transform_t B1 = Transform_NewNamed (ssys, B, "B1");

	Transform_SetLocalPosition (root, (vec4f_t) { 0, 0, 1, 1 });
	Transform_SetLocalPosition (A, (vec4f_t) { 1, 0, 0, 1 });
	Transform_SetLocalRotation (A, (vec4f_t) { 0.5, 0.5, 0.5, 0.5 });
	Transform_SetLocalPosition (B, (vec4f_t) { 0, 1, 0, 1 });
	Transform_SetLocalRotation (B, (vec4f_t) { 0.5, -0.5, 0.5, 0.5 });
	Transform_SetLocalPosition (A1, (vec4f_t) { 1, 0, 0, 1 });
	Transform_SetLocalRotation (A1, (vec4f_t) { -0.5, -0.5, -0.5, 0.5 });
	Transform_SetLocalPosition (B1, (vec4f_t) { 0, 1, 0, 1 });
	Transform_SetLocalRotation (B1, (vec4f_t) { -0.5, 0.5, -0.5, 0.5 });

	auto rref = Transform_GetRef (root);
	hierarchy_t *h = Ent_GetComponent (rref.id, ecs_hierarchy, ssys.reg);
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

	Transform_Delete (root);

	return 0;
}

int
main (void)
{
	scene_t    *scene = Scene_NewScene (NULL);
	ssys.reg = scene->reg;
	ssys.base = scene->base;

	if (test_single_transform ()) { return 1; }
	if (test_parent_child_init ()) { return 1; }
	if (test_parent_child_setparent ()) { return 1; }
	if (test_frames ()) { return 1; }

	Scene_DeleteScene (scene);

	return 0;
}
