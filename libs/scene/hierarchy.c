/*
	hierarchy.c

	General hierarchy handling

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

#include "QF/scene/component.h"
#include "QF/scene/hierarchy.h"
#include "QF/scene/transform.h"

#include "scn_internal.h"

static component_t ent_component = { .size = sizeof (uint32_t) };
static component_t childCount_component = { .size = sizeof (uint32_t) };
static component_t childIndex_component = { .size = sizeof (uint32_t) };
static component_t parentIndex_component = { .size = sizeof (uint32_t) };

static void
hierarchy_UpdateTransformIndices (hierarchy_t *hierarchy, uint32_t start,
								  int offset)
{
	ecs_registry_t *reg = hierarchy->scene->reg;
	for (size_t i = start; i < hierarchy->num_objects; i++) {
		hierref_t  *ref = Ent_GetComponent (hierarchy->ent[i], scene_href, reg);
		ref->index += offset;
	}
}

static void
hierarchy_UpdateChildIndices (hierarchy_t *hierarchy, uint32_t start,
							  int offset)
{
	for (size_t i = start; i < hierarchy->num_objects; i++) {
		hierarchy->childIndex[i] += offset;
	}
}

static void
hierarchy_UpdateParentIndices (hierarchy_t *hierarchy, uint32_t start,
							   int offset)
{
	for (size_t i = start; i < hierarchy->num_objects; i++) {
		hierarchy->parentIndex[i] += offset;
	}
}

void
Hierarchy_Reserve (hierarchy_t *hierarchy, uint32_t count)
{
	if (hierarchy->num_objects + count > hierarchy->max_objects) {
		uint32_t    new_max = hierarchy->num_objects + count;
		new_max += 15;
		new_max &= ~15;

		Component_ResizeArray (&ent_component,
							   (void **) &hierarchy->ent, new_max);
		Component_ResizeArray (&childCount_component,
							   (void **) &hierarchy->childCount, new_max);
		Component_ResizeArray (&childIndex_component,
							   (void **) &hierarchy->childIndex, new_max);
		Component_ResizeArray (&parentIndex_component,
							   (void **) &hierarchy->parentIndex, new_max);

		for (uint32_t i = 0; i < hierarchy->type->num_components; i++) {
			Component_ResizeArray (&hierarchy->type->components[i],
								   &hierarchy->components[i], new_max);
		}
		hierarchy->max_objects = new_max;
	}
}

static void
hierarchy_open (hierarchy_t *hierarchy, uint32_t index, uint32_t count)
{
	Hierarchy_Reserve (hierarchy, count);

	hierarchy->num_objects += count;
	uint32_t    dstIndex = index + count;
	count = hierarchy->num_objects - index - count;
	Component_MoveElements (&ent_component,
							hierarchy->ent, dstIndex, index, count);
	Component_MoveElements (&childCount_component,
							hierarchy->childCount, dstIndex, index, count);
	Component_MoveElements (&childIndex_component,
							hierarchy->childIndex, dstIndex, index, count);
	Component_MoveElements (&parentIndex_component,
							hierarchy->parentIndex, dstIndex, index, count);
	for (uint32_t i = 0; i < hierarchy->type->num_components; i++) {
		Component_MoveElements (&hierarchy->type->components[i],
							    hierarchy->components[i],
								dstIndex, index, count);
	}
}

static void
hierarchy_close (hierarchy_t *hierarchy, uint32_t index, uint32_t count)
{
	if (!count) {
		return;
	}
	hierarchy->num_objects -= count;
	uint32_t    srcIndex = index + count;
	count = hierarchy->num_objects - index;
	Component_MoveElements (&ent_component,
							hierarchy->ent, index, srcIndex, count);
	Component_MoveElements (&childCount_component,
							hierarchy->childCount, index, srcIndex, count);
	Component_MoveElements (&childIndex_component,
							hierarchy->childIndex, index, srcIndex, count);
	Component_MoveElements (&parentIndex_component,
							hierarchy->parentIndex, index, srcIndex, count);
	for (uint32_t i = 0; i < hierarchy->type->num_components; i++) {
		Component_MoveElements (&hierarchy->type->components[i],
							    hierarchy->components[i],
								index, srcIndex, count);
	}
}

static void
hierarchy_move (hierarchy_t *dst, const hierarchy_t *src,
				uint32_t dstIndex, uint32_t srcIndex, uint32_t count)
{
	ecs_registry_t *reg = dst->scene->reg;
	Component_CopyElements (&ent_component,
							dst->ent, dstIndex,
							src->ent, srcIndex, count);
	// Actually move (as in C++ move semantics) source hierarchy object
	// references so that their indices do not get updated when the objects
	// are removed from the source hierarcy
	memset (&src->ent[srcIndex], 0, count * sizeof(dst->ent[0]));

	for (uint32_t i = 0; i < count; i++) {
		uint32_t    ent = dst->ent[dstIndex + i];
		hierref_t  *ref = Ent_GetComponent (ent, scene_href, reg);
		ref->hierarchy = dst;
		ref->index = dstIndex + i;
	}
	for (uint32_t i = 0; i < dst->type->num_components; i++) {
		Component_CopyElements (&dst->type->components[i],
							    dst->components[i], dstIndex,
								src->components[i], srcIndex, count);
	}
}

static void
hierarchy_init (hierarchy_t *dst, uint32_t index,
				uint32_t parentIndex, uint32_t childIndex, uint32_t count)
{
	memset (&dst->ent[index], nullent, count * sizeof(uint32_t));

	for (uint32_t i = 0; i < count; i++) {
		dst->parentIndex[index + i] = parentIndex;
		dst->childCount[index + i] = 0;
		dst->childIndex[index + i] = childIndex;
	}
	for (uint32_t i = 0; i < dst->type->num_components; i++) {
		Component_CreateElements (&dst->type->components[i],
								  dst->components[i], index, count);
	}
}

static uint32_t
hierarchy_insert (hierarchy_t *dst, const hierarchy_t *src,
				  uint32_t dstParent, uint32_t srcRoot, uint32_t count)
{
	uint32_t    insertIndex;	// where the objects will be inserted
	uint32_t    childIndex;		// where the objects' children will inserted

	// The newly added objects are always last children of the parent
	// object
	insertIndex = dst->childIndex[dstParent] + dst->childCount[dstParent];
	// By design, all of an object's children are in one contiguous block,
	// and the blocks of children for each object are ordered by their
	// parents. Thus the child index of each object increases monotonically
	// for each child index in the array, regardless of the level of the owning
	// object (higher levels always come before lower levels).
	uint32_t    neighbor = insertIndex - 1;	// insertIndex never zero
	childIndex = dst->childIndex[neighbor] + dst->childCount[neighbor];

	// Any objects that come after the inserted objects need to have
	// thier indices adjusted.
	hierarchy_UpdateTransformIndices (dst, insertIndex, count);
	// The parent object's child index is not affected, but the child
	// indices of all objects immediately after the parent object are.
	hierarchy_UpdateChildIndices (dst, dstParent + 1, count);
	hierarchy_UpdateParentIndices (dst, childIndex, count);

	// The beginning of the block of children for the new objects was
	// computed from the pre-insert indices of the related objects, thus
	// the index must be updated by the number of objects being inserted
	// (it would have been updated thusly if the insert was done before
	// updating the indices of the other objects).
	childIndex += count;

	hierarchy_open (dst, insertIndex, count);
	if (src) {
		hierarchy_move (dst, src, insertIndex, srcRoot, count);
	} else {
		hierarchy_init (dst, insertIndex, dstParent, childIndex, count);
	}
	for (uint32_t i = 0; i < count; i++) {
		dst->parentIndex[insertIndex + i] = dstParent;
		dst->childIndex[insertIndex + i] = childIndex;
		dst->childCount[insertIndex + i] = 0;
	}

	dst->childCount[dstParent] += count;
	return insertIndex;
}

static void
hierarchy_insert_children (hierarchy_t *dst, const hierarchy_t *src,
						   uint32_t dstParent, uint32_t srcRoot)
{
	uint32_t    insertIndex;
	uint32_t    childIndex = src->childIndex[srcRoot];
	uint32_t    childCount = src->childCount[srcRoot];

	if (childCount) {
		insertIndex = hierarchy_insert (dst, src, dstParent,
										childIndex, childCount);
		for (uint32_t i = 0; i < childCount; i++) {
			hierarchy_insert_children (dst, src, insertIndex + i,
									   childIndex + i);
		}
	}
}

uint32_t
Hierarchy_InsertHierarchy (hierarchy_t *dst, const hierarchy_t *src,
						   uint32_t dstParent, uint32_t srcRoot)
{
	uint32_t    insertIndex;

	if (dstParent == nullent) {
		if (dst->num_objects) {
			Sys_Error ("attempt to insert root in non-empty hierarchy");
		}
		hierarchy_open (dst, 0, 1);
		hierarchy_move (dst, src, 0, srcRoot, 1);
		dst->parentIndex[0] = nullent;
		dst->childIndex[0] = 1;
		dst->childCount[0] = 0;
		insertIndex = 0;
	} else {
		if (!dst->num_objects) {
			Sys_Error ("attempt to insert non-root in empty hierarchy");
		}
		insertIndex = hierarchy_insert (dst, src, dstParent, srcRoot, 1);
	}
	// if src is null, then inserting a new object which has no children
	if (src) {
		hierarchy_insert_children (dst, src, insertIndex, srcRoot);
	}
	return insertIndex;
}

static void
hierarchy_remove_children (hierarchy_t *hierarchy, uint32_t index)
{
	uint32_t    childIndex = hierarchy->childIndex[index];
	uint32_t    childCount = hierarchy->childCount[index];
	uint32_t    parentIndex = hierarchy->parentIndex[index];
	uint32_t    nieceIndex = nullent;

	if (parentIndex != nullent) {
		uint32_t    siblingIndex = hierarchy->childIndex[parentIndex];
		siblingIndex += hierarchy->childCount[parentIndex] - 1;
		nieceIndex = hierarchy->childIndex[siblingIndex];
	}
	for (uint32_t i = childCount; i-- > 0; ) {
		hierarchy_remove_children (hierarchy, childIndex + i);
	}
	hierarchy_close (hierarchy, childIndex, childCount);
	hierarchy->childCount[index] = 0;

	if (childCount) {
		hierarchy_UpdateTransformIndices (hierarchy, childIndex, -childCount);
		hierarchy_UpdateChildIndices (hierarchy, index, -childCount);
		if (nieceIndex != nullent) {
			hierarchy_UpdateParentIndices (hierarchy, nieceIndex, -childCount);
		}
	}
}

void
Hierarchy_RemoveHierarchy (hierarchy_t *hierarchy, uint32_t index)
{
	uint32_t    parentIndex = hierarchy->parentIndex[index];
	uint32_t    childIndex = hierarchy->childIndex[index];
	uint32_t    siblingIndex = nullent;
	if (parentIndex != nullent) {
		siblingIndex = hierarchy->childIndex[parentIndex];
	}
	hierarchy_remove_children (hierarchy, index);
	hierarchy_close (hierarchy, index, 1);
	if (siblingIndex != nullent) {
		hierarchy_UpdateTransformIndices (hierarchy, index, -1);
		hierarchy_UpdateChildIndices (hierarchy, siblingIndex, -1);
		hierarchy_UpdateParentIndices (hierarchy, childIndex - 1, -1);
	}
}

hierarchy_t *
Hierarchy_New (scene_t *scene, const hierarchy_type_t *type, int createRoot)
{
	scene_resources_t *res = scene->resources;
	hierarchy_t *hierarchy = PR_RESNEW (res->hierarchies);
	hierarchy->scene = scene;

	hierarchy->prev = &scene->hierarchies;
	hierarchy->next = scene->hierarchies;
	if (scene->hierarchies) {
		scene->hierarchies->prev = &hierarchy->next;
	}
	scene->hierarchies = hierarchy;

	hierarchy->type = type;
	hierarchy->components = calloc (hierarchy->type->num_components,
									sizeof (void *));

	if (createRoot) {
		hierarchy_open (hierarchy, 0, 1);
		hierarchy_init (hierarchy, 0, nullent, 1, 1);
	}

	return hierarchy;
}

void
Hierarchy_Delete (hierarchy_t *hierarchy)
{
	if (hierarchy->next) {
		hierarchy->next->prev = hierarchy->prev;
	}
	*hierarchy->prev = hierarchy->next;

	free (hierarchy->ent);
	free (hierarchy->childCount);
	free (hierarchy->childIndex);
	free (hierarchy->parentIndex);
	for (uint32_t i = 0; i < hierarchy->type->num_components; i++) {
		free (hierarchy->components[i]);
	}
	free (hierarchy->components);

	scene_resources_t *res = hierarchy->scene->resources;
	PR_RESFREE (res->hierarchies, hierarchy);
}

hierarchy_t *
Hierarchy_Copy (scene_t *scene, const hierarchy_t *src)
{
	ecs_registry_t *dstReg = scene->reg;
	//ecs_registry_t *srcReg = src->scene->reg;
	hierarchy_t *dst = Hierarchy_New (scene, src->type, 0);
	size_t      count = src->num_objects;

	Hierarchy_Reserve (dst, count);

	for (size_t i = 0; i < count; i++) {
		dst->ent[i] = ECS_NewEntity (dstReg);
		hierref_t  *ref = Ent_AddComponent (dst->ent[i], scene_href, dstReg);
		ref->hierarchy = dst;
		ref->index = i;
	}

	Component_CopyElements (&childCount_component,
							dst->childCount, 0, src->childCount, 0, count);
	Component_CopyElements (&childIndex_component,
							dst->childIndex, 0, src->childIndex, 0, count);
	Component_CopyElements (&parentIndex_component,
							dst->parentIndex, 0, src->parentIndex, 0, count);
	for (uint32_t i = 0; i < dst->type->num_components; i++) {
		Component_CopyElements (&dst->type->components[i],
							    dst->components[i], 0,
								src->components[i], 0, count);
	}
	// Just in case the source hierarchy has modified objects
	//Hierarchy_UpdateMatrices (dst);
	return dst;
}
