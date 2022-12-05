/*
	transform.h

	Transform management

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/02/26

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

#ifndef __QF_scene_transform_h
#define __QF_scene_transform_h

#include "QF/darray.h"
#include "QF/qtypes.h"
#include "QF/ecs/component.h"
#include "QF/ecs/hierarchy.h"
#include "QF/ecs/hierarchy.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

/** \defgroup transform Transform management
	\ingroup utils
*/
///@{

enum {
	transform_type_name,
	transform_type_tag,
	transform_type_modified,
	transform_type_localMatrix,
	transform_type_localInverse,
	transform_type_worldMatrix,
	transform_type_worldInverse,
	transform_type_localRotation,
	transform_type_localScale,
	transform_type_worldRotation,

	transform_type_count
};

typedef struct transform_s {
	ecs_registry_t *reg;
	uint32_t    id;
	uint32_t    comp;
} transform_t;

#define nulltransform ((transform_t) {})

#define XFORMINLINE GNU89INLINE inline __attribute__((pure))

XFORMINLINE int Transform_Valid (transform_t transform);

transform_t Transform_New (ecs_registry_t *reg, transform_t parent);
/* Deletes all child transforms, and transform names */
void Transform_Delete (transform_t transform);
transform_t Transform_NewNamed (ecs_registry_t *reg, transform_t parent,
								const char *name);
XFORMINLINE hierref_t *Transform_GetRef (transform_t transform);
XFORMINLINE uint32_t Transform_ChildCount (transform_t transform);
XFORMINLINE transform_t Transform_GetChild (transform_t transform,
										 uint32_t childIndex);
void Transform_SetParent (transform_t transform, transform_t parent);
XFORMINLINE transform_t Transform_GetParent (transform_t transform);
void Transform_SetName (transform_t transform, const char *name);
XFORMINLINE const char *Transform_GetName (transform_t transform);
void Transform_SetTag (transform_t transform, uint32_t tag);
XFORMINLINE uint32_t Transform_GetTag (transform_t transform);
GNU89INLINE inline void Transform_GetLocalMatrix (transform_t transform,
												  mat4f_t mat);
GNU89INLINE inline void Transform_GetLocalInverse (transform_t transform,
												   mat4f_t mat);
GNU89INLINE inline void Transform_GetWorldMatrix (transform_t transform,
												  mat4f_t mat);
// XXX the pointer may be invalidated by hierarchy updates
XFORMINLINE const vec4f_t *Transform_GetWorldMatrixPtr (transform_t transform);
GNU89INLINE inline void Transform_GetWorldInverse (transform_t transform,
												   mat4f_t mat);
XFORMINLINE vec4f_t Transform_GetLocalPosition (transform_t transform);
void Transform_SetLocalPosition (transform_t transform, vec4f_t position);
XFORMINLINE vec4f_t Transform_GetLocalRotation (transform_t transform);
void Transform_SetLocalRotation (transform_t transform, vec4f_t rotation);
XFORMINLINE vec4f_t Transform_GetLocalScale (transform_t transform);
void Transform_SetLocalScale (transform_t transform, vec4f_t scale);
XFORMINLINE vec4f_t Transform_GetWorldPosition (transform_t transform);
void Transform_SetWorldPosition (transform_t transform, vec4f_t position);
XFORMINLINE vec4f_t Transform_GetWorldRotation (transform_t transform);
void Transform_SetWorldRotation (transform_t transform, vec4f_t rotation);
XFORMINLINE vec4f_t Transform_GetWorldScale (transform_t transform);
void Transform_SetLocalTransform (transform_t transform, vec4f_t scale,
								  vec4f_t rotation, vec4f_t position);
// NOTE: these use X: forward, -Y: right, Z:up
// aslo, not guaranteed to be normalized or even orthogonal
XFORMINLINE vec4f_t Transform_Forward (transform_t transform);
XFORMINLINE vec4f_t Transform_Right (transform_t transform);
XFORMINLINE vec4f_t Transform_Up (transform_t transform);
// no SetWorldScale because after rotations, non uniform scale becomes shear

#undef XFORMINLINE
#ifndef IMPLEMENT_TRANSFORM_Funcs
#define XFORMINLINE GNU89INLINE inline
#else
#define XFORMINLINE VISIBLE
#endif

XFORMINLINE
int
Transform_Valid (transform_t transform)
{
	return transform.reg && ECS_EntValid (transform.id, transform.reg);
}

XFORMINLINE
hierref_t *
Transform_GetRef (transform_t transform)
{
	return Ent_GetComponent (transform.id, transform.comp, transform.reg);
}

XFORMINLINE
uint32_t
Transform_ChildCount (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	return h->childCount[ref->index];
}

XFORMINLINE
transform_t
Transform_GetChild (transform_t transform, uint32_t childIndex)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	if (childIndex >= h->childCount[ref->index]) {
		return nulltransform;
	}
	return (transform_t) {
		.reg = transform.reg,
		.id = h->ent[h->childIndex[ref->index] + childIndex],
		.comp = transform.comp,
	};
}

XFORMINLINE
transform_t
Transform_GetParent (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	if (ref->index == 0) {
		return nulltransform;
	}
	hierarchy_t *h = ref->hierarchy;
	return (transform_t) {
		.reg = transform.reg,
		.id = h->ent[h->parentIndex[ref->index]],
		.comp = transform.comp,
	};
}

XFORMINLINE
const char *
Transform_GetName (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	char      **name = h->components[transform_type_name];
	return name[ref->index];
}

XFORMINLINE
uint32_t
Transform_GetTag (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	uint32_t   *tag = h->components[transform_type_tag];
	return tag[ref->index];
}

XFORMINLINE
void
Transform_GetLocalMatrix (transform_t transform, mat4f_t mat)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t     *localMatrix = h->components[transform_type_localMatrix];
	vec4f_t     *src = localMatrix[ref->index];
	mat[0] = src[0];
	mat[1] = src[1];
	mat[2] = src[2];
	mat[3] = src[3];
}

XFORMINLINE
void
Transform_GetLocalInverse (transform_t transform, mat4f_t mat)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t     *localInverse = h->components[transform_type_localInverse];
	vec4f_t     *src = localInverse[ref->index];
	mat[0] = src[0];
	mat[1] = src[1];
	mat[2] = src[2];
	mat[3] = src[3];
}

XFORMINLINE
void
Transform_GetWorldMatrix (transform_t transform, mat4f_t mat)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t     *worldMatrix = h->components[transform_type_worldMatrix];
	vec4f_t     *src = worldMatrix[ref->index];
	mat[0] = src[0];
	mat[1] = src[1];
	mat[2] = src[2];
	mat[3] = src[3];
}

XFORMINLINE
const vec4f_t *
Transform_GetWorldMatrixPtr (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t     *worldMatrix = h->components[transform_type_worldMatrix];
	return worldMatrix[ref->index];
}

XFORMINLINE
void
Transform_GetWorldInverse (transform_t transform, mat4f_t mat)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t     *worldInverse = h->components[transform_type_worldInverse];
	vec4f_t     *src = worldInverse[ref->index];
	mat[0] = src[0];
	mat[1] = src[1];
	mat[2] = src[2];
	mat[3] = src[3];
}

XFORMINLINE
vec4f_t
Transform_GetLocalPosition (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t     *localMatrix = h->components[transform_type_localMatrix];
	return localMatrix[ref->index][3];
}

XFORMINLINE
vec4f_t
Transform_GetLocalRotation (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	vec4f_t     *localRotation = h->components[transform_type_localRotation];
	return localRotation[ref->index];
}

XFORMINLINE
vec4f_t
Transform_GetLocalScale (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	vec4f_t     *localScale = h->components[transform_type_localScale];
	return localScale[ref->index];
}

XFORMINLINE
vec4f_t
Transform_GetWorldPosition (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t     *worldMatrix = h->components[transform_type_worldMatrix];
	return worldMatrix[ref->index][3];
}

XFORMINLINE
vec4f_t
Transform_GetWorldRotation (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	vec4f_t     *worldRotation = h->components[transform_type_worldRotation];
	return worldRotation[ref->index];
}

XFORMINLINE
vec4f_t
Transform_GetWorldScale (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t     *worldMatrix = h->components[transform_type_worldMatrix];
	vec4f_t     *m = worldMatrix[ref->index];
	vec4f_t     s = {
		dotf (m[0], m[0])[0],
		dotf (m[1], m[1])[0],
		dotf (m[2], m[2])[0],
		0,
	};
	return vsqrt4f (s);
}

XFORMINLINE
vec4f_t
Transform_Forward (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t     *worldMatrix = h->components[transform_type_worldMatrix];
	return worldMatrix[ref->index][0];
}

XFORMINLINE
vec4f_t
Transform_Right (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t     *worldMatrix = h->components[transform_type_worldMatrix];
	return -worldMatrix[ref->index][1];
}

XFORMINLINE
vec4f_t
Transform_Up (transform_t transform)
{
	__auto_type ref = Transform_GetRef (transform);
	hierarchy_t *h = ref->hierarchy;
	mat4f_t     *worldMatrix = h->components[transform_type_worldMatrix];
	return worldMatrix[ref->index][2];
}

///@}

#endif//__QF_scene_transform_h
