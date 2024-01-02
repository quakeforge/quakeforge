/*
	hierarchy.c

	ECS hierarchy handling

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

#include "QF/sys.h"

#include "QF/ecs.h"

static component_t ent_component = { .size = sizeof (uint32_t) };
static component_t childCount_component = { .size = sizeof (uint32_t) };
static component_t childIndex_component = { .size = sizeof (uint32_t) };
static component_t parentIndex_component = { .size = sizeof (uint32_t) };
static component_t nextIndex_component = { .size = sizeof (uint32_t) };
static component_t lastIndex_component = { .size = sizeof (uint32_t) };



static void
hierarchy_UpdateTransformIndices (hierarchy_t *hierarchy, uint32_t start,
								  int offset)
{
	ecs_registry_t *reg = hierarchy->reg;
	uint32_t    href = hierarchy->href_comp;
	for (size_t i = start; i < hierarchy->num_objects; i++) {
		if (ECS_EntValid (hierarchy->ent[i], reg)) {
			hierref_t  *ref = Ent_GetComponent (hierarchy->ent[i], href, reg);
			ref->index += offset;
		}
	}
}

static void
hierarchy_InvalidateReferences (hierarchy_t *hierarchy, uint32_t start,
								uint32_t count)
{
	ecs_registry_t *reg = hierarchy->reg;
	uint32_t    href = hierarchy->href_comp;
	for (size_t i = start; count-- > 0; i++) {
		if (ECS_EntValid (hierarchy->ent[i], reg)) {
			hierref_t  *ref = Ent_GetComponent (hierarchy->ent[i], href, reg);
			ref->id = nullent;
			ref->index = nullindex;
		}
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
		Component_ResizeArray (&nextIndex_component,
							   (void **) &hierarchy->nextIndex, new_max);
		Component_ResizeArray (&lastIndex_component,
							   (void **) &hierarchy->lastIndex, new_max);

		if (hierarchy->type) {
			for (uint32_t i = 0; i < hierarchy->type->num_components; i++) {
				Component_ResizeArray (&hierarchy->type->components[i],
									   &hierarchy->components[i], new_max);
			}
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
	if (!count) {
		return;
	}
	Component_MoveElements (&ent_component,
							hierarchy->ent, dstIndex, index, count);
	Component_MoveElements (&childCount_component,
							hierarchy->childCount, dstIndex, index, count);
	Component_MoveElements (&childIndex_component,
							hierarchy->childIndex, dstIndex, index, count);
	Component_MoveElements (&parentIndex_component,
							hierarchy->parentIndex, dstIndex, index, count);
	Component_MoveElements (&nextIndex_component,
							hierarchy->nextIndex, dstIndex, index, count);
	Component_MoveElements (&lastIndex_component,
							hierarchy->lastIndex, dstIndex, index, count);
	if (hierarchy->type) {
		for (uint32_t i = 0; i < hierarchy->type->num_components; i++) {
			Component_MoveElements (&hierarchy->type->components[i],
									hierarchy->components[i],
									dstIndex, index, count);
		}
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
	Component_MoveElements (&nextIndex_component,
							hierarchy->nextIndex, index, srcIndex, count);
	Component_MoveElements (&lastIndex_component,
							hierarchy->lastIndex, index, srcIndex, count);
	if (hierarchy->type) {
		for (uint32_t i = 0; i < hierarchy->type->num_components; i++) {
			Component_MoveElements (&hierarchy->type->components[i],
									hierarchy->components[i],
									index, srcIndex, count);
		}
	}
}

static void
hierarchy_move (hierarchy_t *dst, uint32_t dstid, const hierarchy_t *src,
				uint32_t dstIndex, uint32_t srcIndex, uint32_t count)
{
	ecs_registry_t *reg = dst->reg;
	uint32_t    href = dst->href_comp;
	Component_CopyElements (&ent_component,
							dst->ent, dstIndex,
							src->ent, srcIndex, count);
	// Actually move (as in C++ move semantics) source hierarchy object
	// references so that their indices do not get updated when the objects
	// are removed from the source hierarchy
	memset (&src->ent[srcIndex], nullent, count * sizeof(dst->ent[0]));

	for (uint32_t i = 0; i < count; i++) {
		if (dst->ent[dstIndex + i] != nullent) {
			uint32_t    ent = dst->ent[dstIndex + i];
			hierref_t  *ref = Ent_GetComponent (ent, href, reg);
			ref->id = dstid;
			ref->index = dstIndex + i;
		}
	}
	if (dst->type) {
		for (uint32_t i = 0; i < dst->type->num_components; i++) {
			Component_CopyElements (&dst->type->components[i],
									dst->components[i], dstIndex,
									src->components[i], srcIndex, count);
		}
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
		dst->lastIndex[index + i] = nullindex;
	}
	if (dst->type) {
		for (uint32_t i = 0; i < dst->type->num_components; i++) {
			Component_CreateElements (&dst->type->components[i],
									  dst->components[i], index, count);
		}
	}
}

static uint32_t
hierarchy_insert_flat (hierarchy_t *dst, uint32_t dstid, const hierarchy_t *src,
					   uint32_t dstParent, uint32_t *srcRoot, uint32_t count)
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
	if (dst == src && insertIndex <= *srcRoot) {
		*srcRoot += count;
	}
	if (src) {
		hierarchy_move (dst, dstid, src, insertIndex, *srcRoot, count);
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

static uint32_t
hierarchy_insert_tree (hierarchy_t *dst, uint32_t dstid, const hierarchy_t *src,
					   uint32_t dstParent, uint32_t *srcRoot, uint32_t count)
{
	uint32_t    insertIndex;
	if (dst == src) {
		// reparenting within the hierarchy, so need only to update indices
		// of course, easier said than done
		insertIndex = *srcRoot;
		uint32_t    srcParent = dst->parentIndex[insertIndex];
		uint32_t   *next = &dst->nextIndex[srcParent];
		while (*next != insertIndex) {
			next = &dst->nextIndex[*next];
		}
		if (dst->lastIndex[srcParent] == insertIndex) {
			// removing src from the end of srcParent's child chain
			dst->lastIndex[srcParent] = next - dst->nextIndex;
		}
		*next = dst->nextIndex[insertIndex];
		dst->nextIndex[insertIndex] = nullindex;

		dst->nextIndex[dst->lastIndex[dstParent]] = insertIndex;
		dst->lastIndex[dstParent] = insertIndex;
	} else {
		// new objecs are always appended
		insertIndex = dst->num_objects;
		hierarchy_open (dst, insertIndex, count);
		if (dst->childCount[dstParent]) {
			dst->nextIndex[dst->lastIndex[dstParent]] = insertIndex;
		} else {
			dst->childIndex[dstParent] = insertIndex;
		}
		dst->childCount[dstParent] += count;
		dst->lastIndex[dstParent] = insertIndex;
		dst->nextIndex[insertIndex] = nullindex;

		if (src) {
			hierarchy_move (dst, dstid, src, insertIndex, *srcRoot, count);
		} else {
			hierarchy_init (dst, insertIndex, dstParent, nullindex, count);
		}
	}
	return insertIndex;
}

static uint32_t
hierarchy_insert (hierarchy_t *dst, uint32_t dstid, const hierarchy_t *src,
				  uint32_t dstParent, uint32_t *srcRoot, uint32_t count)
{
	if (dst->tree_mode) {
		return hierarchy_insert_tree (dst, dstid, src, dstParent, srcRoot,
									  count);
	} else {
		return hierarchy_insert_flat (dst, dstid, src, dstParent, srcRoot,
									  count);
	}
}

static void
hierarchy_insert_children (hierarchy_t *dst, uint32_t dstid,
						   const hierarchy_t *src,
						   uint32_t dstParent, uint32_t *srcRoot)
{
	uint32_t    insertIndex;
	uint32_t    childIndex = src->childIndex[*srcRoot];
	uint32_t    childCount = src->childCount[*srcRoot];

	if (childCount) {
		insertIndex = hierarchy_insert (dst, dstid, src, dstParent,
										&childIndex, childCount);
		if (dst == src && insertIndex <= *srcRoot) {
			*srcRoot += childCount;
		}
		for (uint32_t i = 0; i < childCount; i++, childIndex++) {
			hierarchy_insert_children (dst, dstid, src, insertIndex + i,
									   &childIndex);
		}
	}
}

static hierref_t
hierarchy_insertHierarchy (hierarchy_t *dst, uint32_t dstid,
						   const hierarchy_t *src,
						   uint32_t dstParent, uint32_t *srcRoot)
{
	uint32_t    insertIndex;

	if (dstParent == nullindex) {
		if (dst->num_objects) {
			Sys_Error ("attempt to insert root in non-empty hierarchy");
		}
		hierarchy_open (dst, 0, 1);
		if (src) {
			hierarchy_move (dst, dstid, src, 0, *srcRoot, 1);
		}
		dst->parentIndex[0] = nullindex;
		dst->childIndex[0] = 1;
		dst->childCount[0] = 0;
		insertIndex = 0;
	} else {
		if (!dst->num_objects) {
			Sys_Error ("attempt to insert non-root in empty hierarchy");
		}
		insertIndex = hierarchy_insert (dst, dstid, src, dstParent, srcRoot, 1);
	}
	// if src is null, then inserting a new object which has no children
	if (src) {
		hierarchy_insert_children (dst, dstid, src, insertIndex, srcRoot);
	}
	return (hierref_t) { .id = dstid, .index = insertIndex };
}

hierref_t
Hierarchy_InsertHierarchy (hierref_t dref, hierref_t sref, ecs_registry_t *reg)
{
	hierarchy_t *dst = Ent_GetComponent (dref.id, ecs_hierarchy, reg);
	hierarchy_t *src = 0;
	if (ECS_EntValid (sref.id, reg)) {
		src = Ent_GetComponent (sref.id, ecs_hierarchy, reg);
	}
	return hierarchy_insertHierarchy (dst, dref.id, src,
									  dref.index, &sref.index);
}

static void
hierarchy_remove_children (hierarchy_t *hierarchy, uint32_t index,
						   int delEntities)
{
	uint32_t    childIndex = hierarchy->childIndex[index];
	uint32_t    childCount = hierarchy->childCount[index];

	for (uint32_t i = childCount; i-- > 0; ) {
		hierarchy_remove_children (hierarchy, childIndex + i, delEntities);
	}
	if (delEntities) {
		hierarchy_InvalidateReferences (hierarchy, childIndex, childCount);
		for (uint32_t i = 0; i < childCount; i++) {
			ECS_DelEntity (hierarchy->reg, hierarchy->ent[childIndex + i]);
		}
	}
	hierarchy_close (hierarchy, childIndex, childCount);
	hierarchy->childCount[index] = 0;

	if (childCount) {
		hierarchy_UpdateTransformIndices (hierarchy, childIndex, -childCount);
		hierarchy_UpdateChildIndices (hierarchy, index, -childCount);
	}
	if (childIndex < hierarchy->num_objects) {
		hierarchy_UpdateParentIndices (hierarchy, childIndex, -1);
	}
}

void
Hierarchy_RemoveHierarchy (hierarchy_t *hierarchy, uint32_t index,
						   int delEntities)
{
	if (hierarchy->tree_mode) {
		Sys_Error ("Hierarchy_RemoveHierarchy tree mode not implemented");
	}
	uint32_t    parentIndex = hierarchy->parentIndex[index];

	hierarchy_remove_children (hierarchy, index, delEntities);
	if (delEntities) {
		hierarchy_InvalidateReferences (hierarchy, index, 1);
		ECS_DelEntity (hierarchy->reg, hierarchy->ent[index]);
	}
	hierarchy_close (hierarchy, index, 1);

	hierarchy_UpdateTransformIndices (hierarchy, index, -1);
	if (parentIndex != nullindex) {
		hierarchy_UpdateChildIndices (hierarchy, parentIndex + 1, -1);
		hierarchy->childCount[parentIndex] -= 1;
	}
}

void
Hierarchy_Create (hierarchy_t *hierarchy)
{
	if (hierarchy) {
		Sys_Error ("Hierarchy_Create");
	}
}

static void
hierarchy_destroy (hierarchy_t *hierarchy)
{
	free (hierarchy->ent);
	free (hierarchy->childCount);
	free (hierarchy->childIndex);
	free (hierarchy->parentIndex);
	free (hierarchy->nextIndex);
	free (hierarchy->lastIndex);
	if (hierarchy->type) {
		for (uint32_t i = 0; i < hierarchy->type->num_components; i++) {
			free (hierarchy->components[i]);
		}
		free (hierarchy->components);
	}
}

void
Hierarchy_Destroy (hierarchy_t *hierarchy)
{
	hierarchy_InvalidateReferences (hierarchy, 0, hierarchy->num_objects);
	for (uint32_t i = 0; i < hierarchy->num_objects; i++) {
		ECS_DelEntity (hierarchy->reg, hierarchy->ent[i]);
	}
	hierarchy_destroy (hierarchy);
}

uint32_t
Hierarchy_New (ecs_registry_t *reg, uint32_t href_comp,
			   const hierarchy_type_t *type, bool createRoot)
{
	hierarchy_t hierarchy = {
		.reg = reg,
		.href_comp = href_comp,
		.type = type,
		.components = type ? calloc (type->num_components, sizeof (void *)) : 0,
	};

	if (createRoot) {
		hierarchy_open (&hierarchy, 0, 1);
		hierarchy_init (&hierarchy, 0, nullindex, 1, 1);
	}
	uint32_t hent = ECS_NewEntity (reg);
	Ent_SetComponent (hent, ecs_hierarchy, reg, &hierarchy);

	return hent;
}

void
Hierarchy_Delete (uint32_t hierarchy, ecs_registry_t *reg)
{
	hierarchy_t *h = Ent_GetComponent (hierarchy, ecs_hierarchy, reg);
	hierarchy_InvalidateReferences (h, 0, h->num_objects);
	ECS_DelEntity (reg, hierarchy);
}

static uint32_t
copy_one_node (hierarchy_t *dst, const hierarchy_t *src,
			   uint32_t dstIndex, uint32_t childIndex)
{
	uint32_t    srcIndex = dst->parentIndex[dstIndex];
	uint32_t    childCount = src->childCount[srcIndex];

	dst->childIndex[dstIndex] = childIndex;
	dst->childCount[dstIndex] = childCount;
	dst->ent[dstIndex] = src->ent[srcIndex];
	if (dst->type) {
		for (uint32_t i = 0; i < dst->type->num_components; i++) {
			Component_CopyElements (&dst->type->components[i],
									dst->components[i], dstIndex,
									src->components[i], srcIndex, 1);
		}
	}
	return srcIndex;
}

static uint32_t
queue_tree_nodes (hierarchy_t *dst, const hierarchy_t *src,
				  uint32_t queueIndex, uint32_t srcIndex)
{
	uint32_t    srcChild = src->childIndex[srcIndex];
	uint32_t    childCount = src->childCount[srcIndex];

	for (uint32_t i = 0; i < childCount; i++) {
		dst->parentIndex[queueIndex + i] = srcChild;
		srcChild = src->nextIndex[srcChild];
	}
	return childCount;
}

static void
copy_tree_nodes (hierarchy_t *dst, const hierarchy_t *src,
				 uint32_t dstIndex, uint32_t *queueIndex)
{
	auto ind = copy_one_node (dst, src, dstIndex, *queueIndex);
	auto count = queue_tree_nodes (dst, src, *queueIndex, ind);
	*queueIndex += count;
}

static void
swap_pointers (void *a, void *b)
{
	void *t = *(void **)a;
	*(void **)a = *(void **) b;
	*(void **)b = t;
}

void
Hierarchy_SetTreeMode (hierarchy_t *hierarchy, bool tree_mode)
{
	if (!hierarchy->tree_mode == !tree_mode) {
		// no change
		return;
	}
	hierarchy->tree_mode = tree_mode;
	if (tree_mode) {
		// switching from a cononical hierarchy to tree mode, noed only to
		// ensure next/last indices are correct

		// root node has no siblings
		hierarchy->nextIndex[0] = nullindex;
		for (uint32_t i = 0; i < hierarchy->num_objects; i++) {
			uint32_t    count = hierarchy->childCount[i];
			uint32_t    child = hierarchy->childIndex[i];
			for (uint32_t j = 0; count && j < count - 1; j++) {
				hierarchy->nextIndex[child + j] = child + j + 1;
			}
			hierarchy->lastIndex[i] = count ? child + count - 1 : nullindex;
			if (count) {
				hierarchy->nextIndex[hierarchy->lastIndex[i]] = nullindex;
			} else {
				hierarchy->childIndex[i] = nullindex;
			}
		}
		return;
	}

	auto src = hierarchy;
	hierarchy_t tmp = {
		.reg = src->reg,
		.href_comp = src->href_comp,
		.type = src->type,
		.components = src->type
					? calloc (src->type->num_components, sizeof (void *)) : 0,
	};
	Hierarchy_Reserve (&tmp, src->num_objects);
	tmp.num_objects = src->num_objects;

	// treat parentIndex as a queue for breadth-first traversal
	tmp.parentIndex[0] = 0;	// start at root of src
	uint32_t    queueIndex = 1;
	for (uint32_t i = 0; i < src->num_objects; i++) {
		copy_tree_nodes (&tmp, src, i, &queueIndex);
	}
	tmp.parentIndex[0] = nullindex;
	for (uint32_t i = 0; i < src->num_objects; i++) {
		for (uint32_t j = 0; j < tmp.childCount[i]; j++) {
			tmp.parentIndex[tmp.childIndex[i] + j] = i;
		}
	}
	auto href_comp = src->href_comp;
	for (uint32_t i = 0; i < src->num_objects; i++) {
		hierref_t  *ref = Ent_GetComponent (tmp.ent[i], href_comp, src->reg);
		ref->index = i;
	}

	swap_pointers (&tmp.ent, &src->ent);
	swap_pointers (&tmp.childCount, &src->childCount);
	swap_pointers (&tmp.childIndex, &src->childIndex);
	swap_pointers (&tmp.parentIndex, &src->parentIndex);
	swap_pointers (&tmp.nextIndex, &src->nextIndex);
	swap_pointers (&tmp.lastIndex, &src->lastIndex);
	if (src->type) {
		for (uint32_t i = 0; i < src->type->num_components; i++) {
			swap_pointers (&tmp.components[i], &src->components[i]);
		}
	}
	hierarchy_destroy (&tmp);
}

uint32_t
Hierarchy_Copy (ecs_registry_t *dstReg, uint32_t href_comp,
				const hierarchy_t *src)
{
	if (src->tree_mode) {
		Sys_Error ("Hierarchy_Copy tree mode not implemented");
	}
	uint32_t    copy = Hierarchy_New (dstReg, href_comp, src->type, 0);
	hierarchy_t *dst = Ent_GetComponent (copy, ecs_hierarchy, dstReg);
	size_t      count = src->num_objects;

	Hierarchy_Reserve (dst, count);

	for (size_t i = 0; i < count; i++) {
		dst->ent[i] = ECS_NewEntity (dstReg);
		hierref_t   ref = {
			.id = copy,
			.index = i,
		};
		Ent_SetComponent (dst->ent[i], href_comp, dstReg, &ref);
	}

	Component_CopyElements (&childCount_component,
							dst->childCount, 0, src->childCount, 0, count);
	Component_CopyElements (&childIndex_component,
							dst->childIndex, 0, src->childIndex, 0, count);
	Component_CopyElements (&parentIndex_component,
							dst->parentIndex, 0, src->parentIndex, 0, count);
	Component_CopyElements (&nextIndex_component,
							dst->nextIndex, 0, src->nextIndex, 0, count);
	Component_CopyElements (&lastIndex_component,
							dst->lastIndex, 0, src->lastIndex, 0, count);
	if (dst->type) {
		for (uint32_t i = 0; i < dst->type->num_components; i++) {
			Component_CopyElements (&dst->type->components[i],
									dst->components[i], 0,
									src->components[i], 0, count);
		}
	}
	return copy;
}

hierref_t
Hierarchy_SetParent (hierref_t dref, hierref_t sref, ecs_registry_t *reg)
{
	hierarchy_t *src = Ent_GetComponent (sref.id, ecs_hierarchy, reg);
	if (src->tree_mode) {
		Sys_Error ("Hierarchy_SetParent tree mode not implemented");
	}
	if (ECS_EntValid (dref.id, reg)) {
		hierarchy_t *dst = Ent_GetComponent (dref.id, ecs_hierarchy, reg);
		if (dst->type != src->type) {
			Sys_Error ("Can't set parent in hierarchy of different type");
		}
	} else {
		if (!sref.index) {
			return sref;
		}
		dref.id = Hierarchy_New (src->reg, src->href_comp, src->type, 0);
		dref.index = nullindex;
	}
	hierarchy_t *dst = Ent_GetComponent (dref.id, ecs_hierarchy, reg);
	dref = hierarchy_insertHierarchy (dst, dref.id, src,
									  dref.index, &sref.index);
	Hierarchy_RemoveHierarchy (src, sref.index, 0);
	if (!src->num_objects) {
		Hierarchy_Delete (sref.id, reg);
	}
	return dref;
}

void
Hierref_DestroyComponent (void *href, ecs_registry_t *reg)
{
	hierref_t   ref = *(hierref_t *) href;
	if (ECS_EntValid (ref.id, reg)) {
		hierarchy_t *h = Ent_GetComponent (ref.id, ecs_hierarchy, reg);
		h->ent[ref.index] = -1;
		Hierarchy_RemoveHierarchy (h, ref.index, 1);
		if (!h->num_objects) {
			Hierarchy_Delete (ref.id, reg);
		}
	}
}
