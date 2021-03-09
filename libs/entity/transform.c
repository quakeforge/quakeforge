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

#include "QF/entity.h"
#include "QF/render.h"

transform_t *
Transform_New (transform_t *parent)
{
	transform_t *transform = malloc (sizeof (transform_t));

	if (parent) {
		transform->hierarchy = parent->hierarchy;
		transform->index = Hierarchy_InsertHierarchy (parent->hierarchy, 0,
													  parent->index, 0);
	} else {
		transform->hierarchy = Hierarchy_New (16, 1);//FIXME should be config
		transform->index = 0;
	}
	transform->hierarchy->transform.a[transform->index] = transform;
	Hierarchy_UpdateMatrices (transform->hierarchy);
	return transform;
}

void
Transform_Delete (transform_t *transform)
{
	if (transform->index != 0) {
		// The transform is not the root, so pull it out of its current
		// hierarchy so deleting it is easier
		Transform_SetParent (transform, 0);
	}
	Hierarchy_Delete (transform->hierarchy);
}

transform_t *
Transform_NewNamed (transform_t *parent, const char *name)
{
	transform_t *transform = Transform_New (parent);
	Transform_SetName (transform, name);
	return transform;
}

uint32_t
Transform_ChildCount (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->childCount.a[transform->index];
}

transform_t *
Transform_GetChild (const transform_t *transform, uint32_t childIndex)
{
	hierarchy_t *h = transform->hierarchy;
	if (childIndex >= h->childCount.a[transform->index]) {
		return 0;
	}
	return h->transform.a[h->childIndex.a[transform->index] + childIndex];
}

void
Transform_SetParent (transform_t *transform, transform_t *parent)
{
	if (parent) {
		hierarchy_t *hierarchy = transform->hierarchy;
		uint32_t    index = transform->index;
		Hierarchy_InsertHierarchy (parent->hierarchy, hierarchy,
								   parent->index, index);
		Hierarchy_RemoveHierarchy (hierarchy, index);
		if (!hierarchy->name.size) {
			Hierarchy_Delete (hierarchy);
		}
	} else {
		// null parent -> make transform root
		if (!transform->index) {
			// already root
			return;
		}
		hierarchy_t *hierarchy = transform->hierarchy;
		uint32_t    index = transform->index;

		hierarchy_t *new_hierarchy = Hierarchy_New (16, 0);
		Hierarchy_InsertHierarchy (new_hierarchy, hierarchy, null_transform,
								   index);
		Hierarchy_RemoveHierarchy (hierarchy, index);
	}
}

transform_t *
Transform_GetParent (const transform_t *transform)
{
	if (transform->index == 0) {
		return 0;
	}
	hierarchy_t *h = transform->hierarchy;
	return h->transform.a[h->parentIndex.a[transform->index]];
}

void
Transform_SetName (transform_t *transform, const char *name)
{
	hierarchy_t *h = transform->hierarchy;
	//FIXME create a string pool (similar to qfcc's, or even move that to util)
	if (h->name.a[transform->index]) {
		free (h->name.a[transform->index]);
	}
	h->name.a[transform->index] = strdup (name);
}

const char *
Transform_GetName (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->name.a[transform->index];
}

void
Transform_SetTag (transform_t *transform, uint32_t tag)
{
	hierarchy_t *h = transform->hierarchy;
	h->tag.a[transform->index] = tag;
}

uint32_t
Transform_GetTag (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->tag.a[transform->index];
}

void
Transform_GetLocalMatrix (const transform_t *transform, mat4f_t mat)
{
	hierarchy_t *h = transform->hierarchy;
	memcpy (mat, h->localMatrix.a[transform->index], sizeof (mat4f_t));
}

void
Transform_GetLocalInverse (const transform_t *transform, mat4f_t mat)
{
	hierarchy_t *h = transform->hierarchy;
	memcpy (mat, h->localInverse.a[transform->index], sizeof (mat4f_t));
}

void
Transform_GetWorldMatrix (const transform_t *transform, mat4f_t mat)
{
	hierarchy_t *h = transform->hierarchy;
	memcpy (mat, h->worldMatrix.a[transform->index], sizeof (mat4f_t));
}

void
Transform_GetWorldInverse (const transform_t *transform, mat4f_t mat)
{
	hierarchy_t *h = transform->hierarchy;
	memcpy (mat, h->worldInverse.a[transform->index], sizeof (mat4f_t));
}

vec4f_t
Transform_GetLocalPosition (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->localMatrix.a[transform->index][3];
}

void
Transform_SetLocalPosition (transform_t *transform, vec4f_t position)
{
	hierarchy_t *h = transform->hierarchy;
	h->localMatrix.a[transform->index][3] = position;
	h->modified.a[transform->index] = 1;
	Hierarchy_UpdateMatrices (h);
}

vec4f_t
Transform_GetLocalRotation (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->localRotation.a[transform->index];
}

void
Transform_SetLocalRotation (transform_t *transform, vec4f_t rotation)
{
	hierarchy_t *h = transform->hierarchy;
	vec4f_t     scale = h->localScale.a[transform->index];

	mat4f_t     mat;
	mat4fquat (mat, rotation);

	h->localMatrix.a[transform->index][0] = mat[0] * scale[0];
	h->localMatrix.a[transform->index][1] = mat[1] * scale[1];
	h->localMatrix.a[transform->index][2] = mat[2] * scale[2];
	h->modified.a[transform->index] = 1;
	Hierarchy_UpdateMatrices (h);
}

vec4f_t
Transform_GetLocalScale (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->localScale.a[transform->index];
}

void
Transform_SetLocalScale (transform_t *transform, vec4f_t scale)
{
	hierarchy_t *h = transform->hierarchy;
	vec4f_t     rotation = h->localRotation.a[transform->index];

	mat4f_t     mat;
	mat4fquat (mat, rotation);

	h->localMatrix.a[transform->index][0] = mat[0] * scale[0];
	h->localMatrix.a[transform->index][1] = mat[1] * scale[1];
	h->localMatrix.a[transform->index][2] = mat[2] * scale[2];
	h->modified.a[transform->index] = 1;
	Hierarchy_UpdateMatrices (h);
}

vec4f_t
Transform_GetWorldPosition (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldMatrix.a[transform->index][3];
}

void
Transform_SetWorldPosition (transform_t *transform, vec4f_t position)
{
	if (transform->index) {
		hierarchy_t *h = transform->hierarchy;
		uint32_t    parent = h->parentIndex.a[transform->index];
		position = mvmulf (h->worldInverse.a[parent], position);
	}
	Transform_SetLocalPosition (transform, position);
}

vec4f_t
Transform_GetWorldRotation (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldRotation.a[transform->index];
}

void
Transform_SetWorldRotation (transform_t *transform, vec4f_t rotation)
{
	if (transform->index) {
		hierarchy_t *h = transform->hierarchy;
		uint32_t    parent = h->parentIndex.a[transform->index];
		rotation = qmulf (qconjf (h->worldRotation.a[parent]), rotation);
	}
	Transform_SetLocalRotation (transform, rotation);
}

vec4f_t
Transform_GetWorldScale (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldScale.a[transform->index];
}

vec4f_t
Transform_Forward (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldMatrix.a[transform->index][1];
}

vec4f_t
Transform_Right (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldMatrix.a[transform->index][0];
}

vec4f_t
Transform_Up (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldMatrix.a[transform->index][2];
}
