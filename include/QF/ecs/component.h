/*
	component.h

	ECS Component management

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/10/07

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

#ifndef __QF_ecs_component_h
#define __QF_ecs_component_h

#include <string.h>
#include <stdlib.h>

#include "QF/qtypes.h"

/** \defgroup ecs_component Entity Component System components
	\ingroup utils
*/
///@{

struct imui_ctx_s;
struct ecs_registry_s;
typedef struct component_s {
	size_t      size;
	void      (*create) (void *);
	void      (*destroy) (void *, struct ecs_registry_s *reg);
	// comp is the registry component id (base + system component id)
	uint32_t  (*rangeid) (struct ecs_registry_s *reg, uint32_t ent,
						  uint32_t comp);
	const char *name;
	void      (*ui) (void *comp, struct imui_ctx_s *imui_ctx,
					 struct ecs_registry_s *reg, uint32_t ent,
					 void *data);
} component_t;

#define COMPINLINE GNU89INLINE inline

COMPINLINE void Component_ResizeArray (const component_t *component,
									   void **array, uint32_t count);
COMPINLINE void *Component_MoveElements (const component_t *component,
										 void *array, uint32_t dstIndex,
										 uint32_t srcIndex, uint32_t count);
COMPINLINE void Component_RotateElements (const component_t *component,
										  void *array, uint32_t dstIndex,
										  uint32_t srcIndex, uint32_t count);
COMPINLINE void Component_SwapElements (const component_t *component,
										void *a, void *b);
COMPINLINE void *Component_CopyElements (const component_t *component,
										 void *dstArray, uint32_t dstIndex,
										 const void *srcArray,
										 uint32_t srcIndex,
										 uint32_t count);
COMPINLINE void *Component_CreateElements (const component_t *component,
										   void *array,
										   uint32_t index, uint32_t count);
COMPINLINE void Component_DestroyElements (const component_t *component,
										   void *array,
										   uint32_t index, uint32_t count,
										   struct ecs_registry_s *reg);

#undef COMPINLINE
#ifndef IMPLEMENT_ECS_COMPONENT_Funcs
#define COMPINLINE GNU89INLINE inline
#else
#define COMPINLINE VISIBLE
#endif

COMPINLINE void
Component_ResizeArray (const component_t *component,
					   void **array, uint32_t count)
{
	*array = realloc (*array, count * component->size);
}

COMPINLINE void *
Component_MoveElements (const component_t *component,
						void *array, uint32_t dstIndex, uint32_t srcIndex,
						uint32_t count)
{
	__auto_type dst = (byte *) array + dstIndex * component->size;
	__auto_type src = (byte *) array + srcIndex * component->size;
	return memmove (dst, src, count * component->size);
}

COMPINLINE void
Component_RotateElements (const component_t *component,
						  void *array, uint32_t dstIndex, uint32_t srcIndex,
						  uint32_t count)
{
	if (dstIndex == srcIndex) {
		puts ("a");
		return;
	}
	auto dst = (byte *) array + dstIndex * component->size;
	auto src = (byte *) array + srcIndex * component->size;
	size_t countSize = count * component->size;
	if (dstIndex < srcIndex) {
		uint32_t bcount = srcIndex - dstIndex;
		size_t bcountSize = bcount * component->size;
		if (count < bcount) {
			byte tmp[countSize];
			memcpy (tmp, src, countSize);
			memmove (dst + countSize, dst, bcountSize);
			memcpy (dst, tmp, countSize);
		} else {
			byte tmp[bcountSize];
			memcpy (tmp, dst, bcountSize);
			memmove (dst, src, countSize);
			memcpy (dst + countSize, tmp, bcountSize);
		}
	} else if (dstIndex < srcIndex + count) {
		uint32_t bcount = dstIndex - srcIndex;
		size_t bcountSize = bcount * component->size;
		byte tmp[bcountSize];
		memcpy (tmp, src + countSize, bcountSize);
		memmove (dst, src, countSize);
		memcpy (src, tmp, bcountSize);
	} else {
		uint32_t bcount = dstIndex - srcIndex;
		size_t bcountSize = bcount * component->size;
		byte tmp[countSize];
		memcpy (tmp, src, countSize);
		memmove (src, src + countSize, bcountSize);
		memcpy (dst, tmp, countSize);
	}
}

COMPINLINE void
Component_SwapElements (const component_t *component, void *a, void *b)
{
	size_t      size = component->size;
	byte        tmp[size];
	memcpy (tmp, a, size);
	memcpy (a, b, size);
	memcpy (b, tmp, size);
}

COMPINLINE void *
Component_CopyElements (const component_t *component,
						void *dstArray, uint32_t dstIndex,
						const void *srcArray, uint32_t srcIndex,
						uint32_t count)
{
	__auto_type dst = (byte *) dstArray + dstIndex * component->size;
	__auto_type src = (byte *) srcArray + srcIndex * component->size;
	return memcpy (dst, src, count * component->size);
}

COMPINLINE void *
Component_CreateElements (const component_t *component, void *array,
						  uint32_t index, uint32_t count)
{
	if (component->create) {
		for (uint32_t i = index; count-- > 0; i++) {
			__auto_type dst = (byte *) array + i * component->size;
			component->create (dst);
		}
	} else {
		__auto_type dst = (byte *) array + index * component->size;
		memset (dst, 0, count * component->size);
	}
	return (byte *) array + index * component->size;
}

COMPINLINE void
Component_DestroyElements (const component_t *component, void *array,
						   uint32_t index, uint32_t count,
						   struct ecs_registry_s *reg)
{
	if (component->destroy) {
		for (uint32_t i = index; count-- > 0; i++) {
			__auto_type dst = (byte *) array + i * component->size;
			component->destroy (dst, reg);
		}
	}
}

#undef COMPINLINE

///@}

#endif//__QF_ecs_component_h
