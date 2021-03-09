/*
	entity.h

	Entity management

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

#ifndef __QF_entity_h
#define __QF_entity_h

#include "QF/darray.h"
#include "QF/mathlib.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

/** \defgroup entity Entity management
	\ingroup utils
*/
///@{

typedef struct mat4fset_s DARRAY_TYPE (mat4f_t) mat4fset_t;
typedef struct vec4fset_s DARRAY_TYPE (vec4f_t) vec4fset_t;
typedef struct uint32set_s DARRAY_TYPE (uint32_t) uint32set_t;
typedef struct byteset_s DARRAY_TYPE (byte) byteset_t;
typedef struct stringset_s DARRAY_TYPE (char *) stringset_t;
typedef struct xformset_s DARRAY_TYPE (struct transform_s *) xformset_t;
typedef struct entityset_s DARRAY_TYPE (struct entity_s *) entityset_t;

#define null_transform (~0u)

typedef struct hierarchy_s {
	xformset_t  transform;
	entityset_t entity;
	uint32set_t childCount;
	uint32set_t childIndex;
	uint32set_t parentIndex;
	stringset_t name;
	uint32set_t tag;
	byteset_t   modified;
	mat4fset_t  localMatrix;
	mat4fset_t  localInverse;
	mat4fset_t  worldMatrix;
	mat4fset_t  worldInverse;
	vec4fset_t  localRotation;
	vec4fset_t  localScale;
	vec4fset_t  worldRotation;
	vec4fset_t  worldScale;
} hierarchy_t;

typedef struct transform_s {
	hierarchy_t *hierarchy;
	uint32_t    index;
} transform_t;

transform_t *Transform_New (transform_t *parent);
void Transform_Delete (transform_t *transform);
transform_t *Transform_NewNamed (transform_t *parent, const char *name);
uint32_t Transform_ChildCount (const transform_t *transform) __attribute__((pure));
transform_t *Transform_GetChild (const transform_t *transform,
								 uint32_t childIndex) __attribute__((pure));
void Transform_SetParent (transform_t *transform, transform_t *parent);
transform_t *Transform_GetParent (const transform_t *transform) __attribute__((pure));
void Transform_SetName (transform_t *transform, const char *name);
const char *Transform_GetName (const transform_t *transform) __attribute__((pure));
void Transform_SetTag (transform_t *transform, uint32_t tag);
uint32_t Transform_GetTag (const transform_t *transform) __attribute__((pure));
void Transform_GetLocalMatrix (const transform_t *transform, mat4f_t mat);
void Transform_GetLocalInverse (const transform_t *transform, mat4f_t mat);
void Transform_GetWorldMatrix (const transform_t *transform, mat4f_t mat);
void Transform_GetWorldInverse (const transform_t *transform, mat4f_t mat);
vec4f_t Transform_GetLocalPosition (const transform_t *transform) __attribute__((pure));
void Transform_SetLocalPosition (transform_t *transform_t, vec4f_t position);
vec4f_t Transform_GetLocalRotation (const transform_t *transform) __attribute__((pure));
void Transform_SetLocalRotation (transform_t *transform_t, vec4f_t rotation);
vec4f_t Transform_GetLocalScale (const transform_t *transform) __attribute__((pure));
void Transform_SetLocalScale (transform_t *transform_t, vec4f_t scale);
vec4f_t Transform_GetWorldPosition (const transform_t *transform) __attribute__((pure));
void Transform_SetWorldPosition (transform_t *transform_t, vec4f_t position);
vec4f_t Transform_GetWorldRotation (const transform_t *transform) __attribute__((pure));
void Transform_SetWorldRotation (transform_t *transform_t, vec4f_t rotation);
vec4f_t Transform_GetWorldScale (const transform_t *transform) __attribute__((pure));
// NOTE: these use X: right, Y: forward, Z:up
// aslo, not guaranteed to be normalized or even orthogonal
vec4f_t Transform_Forward (const transform_t *transform) __attribute__((pure));
vec4f_t Transform_Right (const transform_t *transform) __attribute__((pure));
vec4f_t Transform_Up (const transform_t *transform) __attribute__((pure));
// no SetWorldScale because after rotations, non uniform scale becomes shear

hierarchy_t *Hierarchy_New (size_t grow, int createRoot);
hierarchy_t *Hierarchy_Copy (hierarchy_t *src);
void Hierarchy_Delete (hierarchy_t *hierarchy);

void Hierarchy_UpdateMatrices (hierarchy_t *hierarchy);
uint32_t Hierarchy_InsertHierarchy (hierarchy_t *dst, const hierarchy_t *src,
									uint32_t dstParent, uint32_t srcRoot);
void Hierarchy_RemoveHierarchy (hierarchy_t *hierarchy, uint32_t index);
///@}

#endif//__QF_entity_h
