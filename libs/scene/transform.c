/*
	transform.c

	General transform handling

	Copyright (C) 2021 Bill Currke

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#define IMPLEMENT_TRANSFORM_Funcs

#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

#include "scn_internal.h"

static void
transform_mat4f_identity (void *_mat)
{
	vec4f_t    *mat = _mat;
	mat4fidentity (mat);
}

static void
transform_rotation_identity (void *_rot)
{
	vec4f_t    *rot = _rot;
	*rot = (vec4f_t) { 0, 0, 0, 1 };
}

static void
transform_scale_identity (void *_scale)
{
	vec4f_t    *scale = _scale;
	*scale = (vec4f_t) { 1, 1, 1, 1 };
}

static void
transform_modified_init (void *_modified)
{
	byte       *modified = _modified;
	*modified = 1;
}

static const component_t transform_components[transform_type_count] = {
	[transform_type_name] = {
		.size = sizeof (char *),
		.name = "Name",
	},
	[transform_type_tag] = {
		.size = sizeof (uint32_t),
		.name = "Tag",
	},
	[transform_type_modified] = {
		.size = sizeof (byte),
		.create = transform_modified_init,
		.name = "Modified",
	},
	[transform_type_localMatrix] = {
		.size = sizeof (mat4f_t),
		.create = transform_mat4f_identity,
		.name = "Local Matrix",
	},
	[transform_type_localInverse] = {
		.size = sizeof (mat4f_t),
		.create = transform_mat4f_identity,
		.name = "Local Inverse",
	},
	[transform_type_worldMatrix] = {
		.size = sizeof (mat4f_t),
		.create = transform_mat4f_identity,
		.name = "World Matrix",
	},
	[transform_type_worldInverse] = {
		.size = sizeof (mat4f_t),
		.create = transform_mat4f_identity,
		.name = "World Inverse",
	},
	[transform_type_localRotation] = {
		.size = sizeof (vec4f_t),
		.create = transform_rotation_identity,
		.name = "Local Rotation",
	},
	[transform_type_localScale] = {
		.size = sizeof (vec4f_t),
		.create = transform_scale_identity,
		.name = "Local Scale",
	},
	[transform_type_worldRotation] = {
		.size = sizeof (vec4f_t),
		.create = transform_rotation_identity,
		.name = "World Rotation",
	},
};

static const hierarchy_type_t transform_type = {
	.num_components = transform_type_count,
	.components = transform_components,
};

static void
transform_calcLocalInverse (hierarchy_t *h, uint32_t index)
{
	mat4f_t    *localMatrix = h->components[transform_type_localMatrix];
	mat4f_t    *localInverse = h->components[transform_type_localInverse];
	// This takes advantage of the fact that localMatrix is a simple
	// homogenous scale/rotate/translate matrix with no shear
	vec4f_t     x = localMatrix[index][0];
	vec4f_t     y = localMatrix[index][1];
	vec4f_t     z = localMatrix[index][2];
	vec4f_t     t = localMatrix[index][3];

	// "one" is to ensure both the scalar and translation have 1 in their
	// fourth components
	vec4f_t     one = { 0, 0, 0, 1 };
	vec4f_t     nx = { x[0], y[0], z[0], 0 };
	vec4f_t     ny = { x[1], y[1], z[1], 0 };
	vec4f_t     nz = { x[2], y[2], z[2], 0 };
	vec4f_t     nt = one - t[0] * nx - t[1] * ny - t[2] * nz;
	// vertical dot product!!!
	vec4f_t     s = 1 / (nx * nx + ny * ny + nz * nz + one);
	localInverse[index][0] = nx * s;
	localInverse[index][1] = ny * s;
	localInverse[index][2] = nz * s;
	localInverse[index][3] = nt * s;
}

static void
Transform_UpdateMatrices (hierarchy_t *h)
{
	mat4f_t    *localMatrix = h->components[transform_type_localMatrix];
	mat4f_t    *localInverse = h->components[transform_type_localInverse];
	mat4f_t    *worldMatrix = h->components[transform_type_worldMatrix];
	mat4f_t    *worldInverse = h->components[transform_type_worldInverse];
	vec4f_t    *localRotation = h->components[transform_type_localRotation];
	vec4f_t    *worldRotation = h->components[transform_type_worldRotation];
	byte       *modified = h->components[transform_type_modified];

	for (uint32_t i = 0; i < h->num_objects; i++) {
		if (modified[i]) {
			transform_calcLocalInverse (h, i);
		}
	}
	if (modified[0]) {
		memcpy (worldMatrix[0],
				localMatrix[0], sizeof (mat4_t));
		memcpy (worldInverse[0],
				localInverse[0], sizeof (mat4_t));
		worldRotation[0] = localRotation[0];
	}
	for (size_t i = 1; i < h->num_objects; i++) {
		uint32_t    parent = h->parentIndex[i];

		if (modified[i] || modified[parent]) {
			mmulf (worldMatrix[i],
				   worldMatrix[parent], localMatrix[i]);
			modified[i] = 1;
		}
	}
	for (size_t i = 1; i < h->num_objects; i++) {
		uint32_t    parent = h->parentIndex[i];

		if (modified[i] || modified[parent]) {
			mmulf (worldInverse[i],
				   localInverse[i], worldInverse[parent]);
		}
	}
	for (size_t i = 1; i < h->num_objects; i++) {
		uint32_t    parent = h->parentIndex[i];
		if (modified[i] || modified[parent]) {
			worldRotation[i] = qmulf (worldRotation[parent],
										   localRotation[i]);
		}
	}
	memset (modified, 0, h->num_objects);
}

transform_t
Transform_New (ecs_registry_t *reg, transform_t parent)
{
	uint32_t    transform = ECS_NewEntity (reg);
	hierref_t  *ref = Ent_AddComponent (transform, scene_href, reg);

	if (parent.reg && parent.id != nullent) {
		hierref_t  *pref = Transform_GetRef (parent);
		ref->hierarchy = pref->hierarchy;
		ref->index = Hierarchy_InsertHierarchy (pref->hierarchy, 0,
												pref->index, 0);
	} else {
		ref->hierarchy = Hierarchy_New (reg, &transform_type, 1);
		ref->index = 0;
	}
	ref->hierarchy->ent[ref->index] = transform;
	Transform_UpdateMatrices (ref->hierarchy);
	return (transform_t) { .reg = reg, .id = transform, .comp = scene_href };
}

void
Transform_Delete (transform_t transform)
{
	hierref_t  *ref = Transform_GetRef (transform);
	if (ref->index != 0) {
		// The transform is not the root, so pull it out of its current
		// hierarchy so deleting it is easier
		Transform_SetParent (transform, (transform_t) {});
	}
	// Takes care of freeing the transforms
	Hierarchy_Delete (ref->hierarchy);
}

transform_t
Transform_NewNamed (ecs_registry_t *reg, transform_t parent, const char *name)
{
	transform_t transform = Transform_New (reg, parent);
	Transform_SetName (transform, name);
	return transform;
}

void
Transform_SetParent (transform_t transform, transform_t parent)
{
	hierarchy_t *dst = 0;
	uint32_t    dstParent = nullent;
	hierarchy_t *src = 0;
	uint32_t    srcIndex = 0;
	if (Transform_Valid (parent)) {
		__auto_type ref = Transform_GetRef (parent);
		dst = ref->hierarchy;
		dstParent = ref->index;
	}
	{
		__auto_type ref = Transform_GetRef (transform);
		src = ref->hierarchy;
		srcIndex = ref->index;
	}
	Hierarchy_SetParent (dst, dstParent, src, srcIndex);

	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	byte       *modified = h->components[transform_type_modified];
	modified[ref->index] = 1;
	Transform_UpdateMatrices (h);
}

void
Transform_SetName (transform_t transform, const char *_name)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	char      **name = h->components[transform_type_name];
	//FIXME create a string pool (similar to qfcc's, or even move that to util)
	if (name[ref->index]) {
		free (name[ref->index]);
	}
	name[ref->index] = strdup (_name);
}

void
Transform_SetTag (transform_t transform, uint32_t _tag)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	uint32_t   *tag = h->components[transform_type_tag];
	tag[ref->index] = _tag;
}

void
Transform_SetLocalPosition (transform_t transform, vec4f_t position)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t    *localMatrix = h->components[transform_type_localMatrix];
	byte       *modified = h->components[transform_type_modified];
	localMatrix[ref->index][3] = position;
	modified[ref->index] = 1;
	Transform_UpdateMatrices (h);
}

void
Transform_SetLocalRotation (transform_t transform, vec4f_t rotation)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t    *localMatrix = h->components[transform_type_localMatrix];
	vec4f_t    *localRotation = h->components[transform_type_localRotation];
	vec4f_t    *localScale = h->components[transform_type_localScale];
	byte       *modified = h->components[transform_type_modified];
	vec4f_t     scale = localScale[ref->index];

	mat4f_t     mat;
	mat4fquat (mat, rotation);

	localRotation[ref->index] = rotation;
	localMatrix[ref->index][0] = mat[0] * scale[0];
	localMatrix[ref->index][1] = mat[1] * scale[1];
	localMatrix[ref->index][2] = mat[2] * scale[2];
	modified[ref->index] = 1;
	Transform_UpdateMatrices (h);
}

void
Transform_SetLocalScale (transform_t transform, vec4f_t scale)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t    *localMatrix = h->components[transform_type_localMatrix];
	vec4f_t    *localRotation = h->components[transform_type_localRotation];
	vec4f_t    *localScale = h->components[transform_type_localScale];
	byte       *modified = h->components[transform_type_modified];
	vec4f_t     rotation = localRotation[ref->index];

	mat4f_t     mat;
	mat4fquat (mat, rotation);

	localScale[ref->index] = scale;
	localMatrix[ref->index][0] = mat[0] * scale[0];
	localMatrix[ref->index][1] = mat[1] * scale[1];
	localMatrix[ref->index][2] = mat[2] * scale[2];
	modified[ref->index] = 1;
	Transform_UpdateMatrices (h);
}

void
Transform_SetWorldPosition (transform_t transform, vec4f_t position)
{
	__auto_type ref = Transform_GetRef (transform);
	if (ref->index) {
		hierarchy_t *h = ref->hierarchy;
		mat4f_t    *worldInverse = h->components[transform_type_worldInverse];
		uint32_t    parent = h->parentIndex[ref->index];
		position = mvmulf (worldInverse[parent], position);
	}
	Transform_SetLocalPosition (transform, position);
}

void
Transform_SetWorldRotation (transform_t transform, vec4f_t rotation)
{
	__auto_type ref = Transform_GetRef (transform);
	if (ref->index) {
		hierarchy_t *h = ref->hierarchy;
		vec4f_t    *worldRotation = h->components[transform_type_worldRotation];
		uint32_t    parent = h->parentIndex[ref->index];
		rotation = qmulf (qconjf (worldRotation[parent]), rotation);
	}
	Transform_SetLocalRotation (transform, rotation);
}

void
Transform_SetLocalTransform (transform_t transform, vec4f_t scale,
							 vec4f_t rotation, vec4f_t position)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t    *localMatrix = h->components[transform_type_localMatrix];
	vec4f_t    *localRotation = h->components[transform_type_localRotation];
	vec4f_t    *localScale = h->components[transform_type_localScale];
	byte       *modified = h->components[transform_type_modified];
	mat4f_t     mat;
	mat4fquat (mat, rotation);

	position[3] = 1;
	localRotation[ref->index] = rotation;
	localScale[ref->index] = scale;
	localMatrix[ref->index][0] = mat[0] * scale[0];
	localMatrix[ref->index][1] = mat[1] * scale[1];
	localMatrix[ref->index][2] = mat[2] * scale[2];
	localMatrix[ref->index][3] = position;
	modified[ref->index] = 1;
	Transform_UpdateMatrices (h);
}
