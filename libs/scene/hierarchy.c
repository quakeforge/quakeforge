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

#include "QF/scene/hierarchy.h"
#include "QF/scene/transform.h"

#include "scn_internal.h"

#if defined(_WIN32) && !defined(_WIN64)
// FIXME (maybe) this is a hack to make DARRAY arrrays 16-byte aligned on
// 32-bit systems (in particular for this case, windows) as the vectors and
// matrices require 16-byte alignment but system malloc (etc) provide only
// 8-byte alignment.
// Really, a custom allocator (maybe using cmem) would be better.
#define free(mem) _aligned_free(mem)
#define malloc(size) _aligned_malloc(size, 16)
#define realloc(mem, size) _aligned_realloc(mem, size, 16)
#endif

static void
hierarchy_UpdateTransformIndices (hierarchy_t *hierarchy, uint32_t start,
								  int offset)
{
	for (size_t i = start; i < hierarchy->transform.size; i++) {
		if (hierarchy->transform.a[i]) {
			hierarchy->transform.a[i]->index += offset;
		}
	}
}

static void
hierarchy_UpdateChildIndices (hierarchy_t *hierarchy, uint32_t start,
							  int offset)
{
	for (size_t i = start; i < hierarchy->childIndex.size; i++) {
		hierarchy->childIndex.a[i] += offset;
	}
}

static void
hierarchy_UpdateParentIndices (hierarchy_t *hierarchy, uint32_t start,
							   int offset)
{
	for (size_t i = start; i < hierarchy->parentIndex.size; i++) {
		hierarchy->parentIndex.a[i] += offset;
	}
}

static void
hierarchy_calcLocalInverse (hierarchy_t *h, uint32_t index)
{
	// This takes advantage of the fact that localMatrix is a simple
	// homogenous scale/rotate/translate matrix with no shear
	vec4f_t     x = h->localMatrix.a[index][0];
	vec4f_t     y = h->localMatrix.a[index][1];
	vec4f_t     z = h->localMatrix.a[index][2];
	vec4f_t     t = h->localMatrix.a[index][3];

	// "one" is to ensure both the scalar and translation have 1 in their
	// forth components
	vec4f_t     one = { 0, 0, 0, 1 };
	vec4f_t     nx = { x[0], y[0], z[0], 0 };
	vec4f_t     ny = { x[1], y[1], z[1], 0 };
	vec4f_t     nz = { x[2], y[2], z[2], 0 };
	vec4f_t     nt = one - t[0] * nx - t[1] * ny - t[2] * nz;
	// vertical dot product!!!
	vec4f_t     s = 1 / (nx * nx + ny * ny + nz * nz + one);
	h->localInverse.a[index][0] = nx * s;
	h->localInverse.a[index][1] = ny * s;
	h->localInverse.a[index][2] = nz * s;
	h->localInverse.a[index][3] = nt * s;
}

void
Hierarchy_UpdateMatrices (hierarchy_t *h)
{
	for (size_t i = 0; i < h->localInverse.size; i++) {
		if (h->modified.a[i]) {
			hierarchy_calcLocalInverse (h, i);
		}
	}
	if (h->modified.a[0]) {
		memcpy (h->worldMatrix.a[0],
				h->localMatrix.a[0], sizeof (mat4_t));
		memcpy (h->worldInverse.a[0],
				h->localInverse.a[0], sizeof (mat4_t));
		h->worldRotation.a[0] = h->localRotation.a[0];
		h->worldScale.a[0] = h->localScale.a[0];
	}
	for (size_t i = 1; i < h->worldMatrix.size; i++) {
		uint32_t    parent = h->parentIndex.a[i];

		if (h->modified.a[i] || h->modified.a[parent]) {
			mmulf (h->worldMatrix.a[i],
				   h->worldMatrix.a[parent], h->localMatrix.a[i]);
			h->modified.a[i] = 1;
		}
	}
	for (size_t i = 1; i < h->worldInverse.size; i++) {
		uint32_t    parent = h->parentIndex.a[i];

		if (h->modified.a[i] || h->modified.a[parent]) {
			mmulf (h->worldInverse.a[i],
				   h->localInverse.a[i], h->worldInverse.a[parent]);
		}
	}
	for (size_t i = 1; i < h->worldRotation.size; i++) {
		uint32_t    parent = h->parentIndex.a[i];
		if (h->modified.a[i] || h->modified.a[parent]) {
			h->worldRotation.a[i] = qmulf (h->worldRotation.a[parent],
										   h->localRotation.a[i]);
		}
	}
	for (size_t i = 1; i < h->worldScale.size; i++) {
		uint32_t    parent = h->parentIndex.a[i];
		if (h->modified.a[i] || h->modified.a[parent]) {
			h->worldScale.a[i] = m3vmulf (h->worldMatrix.a[parent],
										  h->localScale.a[i]);
		}
	}
	memset (h->modified.a, 0, h->modified.size);
}

static void
hierarchy_open (hierarchy_t *hierarchy, uint32_t index, uint32_t count)
{
	DARRAY_OPEN_AT (&hierarchy->transform, index, count);
	DARRAY_OPEN_AT (&hierarchy->entity, index, count);
	DARRAY_OPEN_AT (&hierarchy->childCount, index, count);
	DARRAY_OPEN_AT (&hierarchy->childIndex, index, count);
	DARRAY_OPEN_AT (&hierarchy->parentIndex, index, count);
	DARRAY_OPEN_AT (&hierarchy->name, index, count);
	DARRAY_OPEN_AT (&hierarchy->tag, index, count);
	DARRAY_OPEN_AT (&hierarchy->modified, index, count);
	DARRAY_OPEN_AT (&hierarchy->localMatrix, index, count);
	DARRAY_OPEN_AT (&hierarchy->localInverse, index, count);
	DARRAY_OPEN_AT (&hierarchy->worldMatrix, index, count);
	DARRAY_OPEN_AT (&hierarchy->worldInverse, index, count);
	DARRAY_OPEN_AT (&hierarchy->localRotation, index, count);
	DARRAY_OPEN_AT (&hierarchy->localScale, index, count);
	DARRAY_OPEN_AT (&hierarchy->worldRotation, index, count);
	DARRAY_OPEN_AT (&hierarchy->worldScale, index, count);
}

static void
hierarchy_close (hierarchy_t *hierarchy, uint32_t index, uint32_t count)
{
	if (count) {
		DARRAY_CLOSE_AT (&hierarchy->transform, index, count);
		DARRAY_CLOSE_AT (&hierarchy->entity, index, count);
		DARRAY_CLOSE_AT (&hierarchy->childCount, index, count);
		DARRAY_CLOSE_AT (&hierarchy->childIndex, index, count);
		DARRAY_CLOSE_AT (&hierarchy->parentIndex, index, count);
		DARRAY_CLOSE_AT (&hierarchy->name, index, count);
		DARRAY_CLOSE_AT (&hierarchy->tag, index, count);
		DARRAY_CLOSE_AT (&hierarchy->modified, index, count);
		DARRAY_CLOSE_AT (&hierarchy->localMatrix, index, count);
		DARRAY_CLOSE_AT (&hierarchy->localInverse, index, count);
		DARRAY_CLOSE_AT (&hierarchy->worldMatrix, index, count);
		DARRAY_CLOSE_AT (&hierarchy->worldInverse, index, count);
		DARRAY_CLOSE_AT (&hierarchy->localRotation, index, count);
		DARRAY_CLOSE_AT (&hierarchy->localScale, index, count);
		DARRAY_CLOSE_AT (&hierarchy->worldRotation, index, count);
		DARRAY_CLOSE_AT (&hierarchy->worldScale, index, count);
	}
}

static void
hierarchy_move (hierarchy_t *dst, const hierarchy_t *src,
				uint32_t dstIndex, uint32_t srcIndex, uint32_t count)
{
	memcpy (&dst->transform.a[dstIndex], &src->transform.a[srcIndex],
			count * sizeof(dst->transform.a[0]));
	memset (&src->transform.a[srcIndex], 0,
			count * sizeof(dst->transform.a[0]));
	memcpy (&dst->entity.a[dstIndex], &src->entity.a[srcIndex],
			count * sizeof(dst->entity.a[0]));
	memcpy (&dst->name.a[dstIndex], &src->name.a[srcIndex],
			count * sizeof(dst->name.a[0]));
	memcpy (&dst->tag.a[dstIndex], &src->tag.a[srcIndex],
			count * sizeof(dst->tag.a[0]));
	memset (&dst->modified.a[dstIndex], 1, count * sizeof(dst->modified.a[0]));
	memcpy (&dst->localMatrix.a[dstIndex], &src->localMatrix.a[srcIndex],
			count * sizeof(dst->localMatrix.a[0]));
	memcpy (&dst->localInverse.a[dstIndex], &src->localInverse.a[srcIndex],
			count * sizeof(dst->localInverse.a[0]));
	memcpy (&dst->localRotation.a[dstIndex], &src->localRotation.a[srcIndex],
			count * sizeof(dst->localRotation.a[0]));
	memcpy (&dst->localScale.a[dstIndex], &src->localScale.a[srcIndex],
			count * sizeof(dst->localScale.a[0]));

	for (uint32_t i = 0; i < count; i++) {
		dst->transform.a[dstIndex + i]->hierarchy = dst;
		dst->transform.a[dstIndex + i]->index = dstIndex + i;
	}
}

static void
hierarchy_init (hierarchy_t *dst, uint32_t index,
				uint32_t parentIndex, uint32_t childIndex, uint32_t count)
{
	memset (&dst->transform.a[index], 0,
			count * sizeof(dst->transform.a[0]));
	memset (&dst->entity.a[index], 0, count * sizeof(dst->entity.a[0]));
	memset (&dst->name.a[index], 0, count * sizeof(dst->name.a[0]));
	memset (&dst->tag.a[index], 0, count * sizeof(dst->tag.a[0]));
	memset (&dst->modified.a[index], 1, count * sizeof(dst->modified.a[0]));

	for (uint32_t i = 0; i < count; i++) {
		mat4fidentity (dst->localMatrix.a[index]);
		mat4fidentity (dst->localInverse.a[index]);
		dst->localRotation.a[index] = (vec4f_t) { 0, 0, 0, 1 };
		dst->localScale.a[index] = (vec4f_t) { 1, 1, 1, 1 };

		dst->parentIndex.a[index + i] = parentIndex;
		dst->childCount.a[index + i] = 0;
		dst->childIndex.a[index + i] = childIndex;
	}
}

static uint32_t
hierarchy_insert (hierarchy_t *dst, const hierarchy_t *src,
				  uint32_t dstParent, uint32_t srcRoot, uint32_t count)
{
	uint32_t    insertIndex;	// where the transforms will be inserted
	uint32_t    childIndex;		// where the transforms' children will inserted

	// The newly added transforms are always last children of the parent
	// transform
	insertIndex = dst->childIndex.a[dstParent] + dst->childCount.a[dstParent];

	// By design, all of a transform's children are in one contiguous block,
	// and the blocks of children for each transform are ordered by their
	// parents. Thus the child index of each transform increases monotonically
	// for each child index in the array, regardless of the level of the owning
	// transform (higher levels always come before lower levels).
	uint32_t    neighbor = insertIndex - 1;	// insertIndex never zero
	childIndex = dst->childIndex.a[neighbor] + dst->childCount.a[neighbor];

	// Any transforms that come after the inserted transforms need to have
	// thier indices adjusted.
	hierarchy_UpdateTransformIndices (dst, insertIndex, count);
	// The parent transform's child index is not affected, but the child
	// indices of all transforms immediately after the parent transform are.
	hierarchy_UpdateChildIndices (dst, dstParent + 1, count);
	hierarchy_UpdateParentIndices (dst, childIndex, count);

	// The beginning of the block of children for the new transforms was
	// computed from the pre-insert indices of the related transforms, thus
	// the index must be updated by the number of transforms being inserted
	// (it would have been updated thusly if the insert was done before
	// updating the indices of the other transforms).
	childIndex += count;

	hierarchy_open (dst, insertIndex, count);
	if (src) {
		hierarchy_move (dst, src, insertIndex, srcRoot, count);
	} else {
		hierarchy_init (dst, insertIndex, dstParent, childIndex, count);
	}
	for (uint32_t i = 0; i < count; i++) {
		dst->parentIndex.a[insertIndex + i] = dstParent;
		dst->childIndex.a[insertIndex + i] = childIndex;
		dst->childCount.a[insertIndex + i] = 0;
	}

	dst->childCount.a[dstParent] += count;
	return insertIndex;
}

static void
hierarchy_insert_children (hierarchy_t *dst, const hierarchy_t *src,
						   uint32_t dstParent, uint32_t srcRoot)
{
	uint32_t    insertIndex;
	uint32_t    childIndex = src->childIndex.a[srcRoot];
	uint32_t    childCount = src->childCount.a[srcRoot];

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

	if (dstParent == null_transform) {
		if (dst->transform.size) {
			Sys_Error ("attempt to insert root in non-empty hierarchy");
		}
		hierarchy_open (dst, 0, 1);
		hierarchy_move (dst, src, 0, srcRoot, 1);
		dst->parentIndex.a[0] = null_transform;
		dst->childIndex.a[0] = 1;
		dst->childCount.a[0] = 0;
		insertIndex = 0;
	} else {
		if (!dst->transform.size) {
			Sys_Error ("attempt to insert non-root in empty hierarchy");
		}
		insertIndex = hierarchy_insert (dst, src, dstParent, srcRoot, 1);
	}
	// if src is null, then inserting a new transform which has no children
	if (src) {
		hierarchy_insert_children (dst, src, insertIndex, srcRoot);
	}
	Hierarchy_UpdateMatrices (dst);
	return insertIndex;
}

static void
hierarchy_remove_children (hierarchy_t *hierarchy, uint32_t index)
{
	uint32_t    childIndex = hierarchy->childIndex.a[index];
	uint32_t    childCount = hierarchy->childCount.a[index];
	uint32_t    parentIndex = hierarchy->parentIndex.a[index];
	uint32_t    nieceIndex = null_transform;

	if (parentIndex != null_transform) {
		uint32_t    siblingIndex = hierarchy->childIndex.a[parentIndex];
		siblingIndex += hierarchy->childCount.a[parentIndex] - 1;
		nieceIndex = hierarchy->childIndex.a[siblingIndex];
	}
	for (uint32_t i = childCount; i-- > 0; ) {
		hierarchy_remove_children (hierarchy, childIndex + i);
	}
	hierarchy_close (hierarchy, childIndex, childCount);
	hierarchy->childCount.a[index] = 0;

	if (childCount) {
		hierarchy_UpdateTransformIndices (hierarchy, childIndex, -childCount);
		hierarchy_UpdateChildIndices (hierarchy, index, -childCount);
		if (nieceIndex != null_transform) {
			hierarchy_UpdateParentIndices (hierarchy, nieceIndex, -childCount);
		}
	}
}

void
Hierarchy_RemoveHierarchy (hierarchy_t *hierarchy, uint32_t index)
{
	uint32_t    parentIndex = hierarchy->parentIndex.a[index];
	uint32_t    childIndex = hierarchy->childIndex.a[index];
	uint32_t    siblingIndex = null_transform;
	if (parentIndex != null_transform) {
		siblingIndex = hierarchy->childIndex.a[parentIndex];
	}
	hierarchy_remove_children (hierarchy, index);
	hierarchy_close (hierarchy, index, 1);
	if (siblingIndex != null_transform) {
		hierarchy_UpdateTransformIndices (hierarchy, index, -1);
		hierarchy_UpdateChildIndices (hierarchy, siblingIndex, -1);
		hierarchy_UpdateParentIndices (hierarchy, childIndex - 1, -1);
	}
}

hierarchy_t *
Hierarchy_New (size_t grow, int createRoot)
{
	if (!grow) {
		grow = 16;
	}
	hierarchy_t *hierarchy = malloc (sizeof (hierarchy_t));

	DARRAY_INIT (&hierarchy->transform, grow);
	DARRAY_INIT (&hierarchy->entity, grow);
	DARRAY_INIT (&hierarchy->childCount, grow);
	DARRAY_INIT (&hierarchy->childIndex, grow);
	DARRAY_INIT (&hierarchy->parentIndex, grow);
	DARRAY_INIT (&hierarchy->name, grow);
	DARRAY_INIT (&hierarchy->tag, grow);
	DARRAY_INIT (&hierarchy->modified, grow);
	DARRAY_INIT (&hierarchy->localMatrix, grow);
	DARRAY_INIT (&hierarchy->localInverse, grow);
	DARRAY_INIT (&hierarchy->worldMatrix, grow);
	DARRAY_INIT (&hierarchy->worldInverse, grow);
	DARRAY_INIT (&hierarchy->localRotation, grow);
	DARRAY_INIT (&hierarchy->localScale, grow);
	DARRAY_INIT (&hierarchy->worldRotation, grow);
	DARRAY_INIT (&hierarchy->worldScale, grow);

	if (createRoot) {
		hierarchy_open (hierarchy, 0, 1);
		hierarchy_init (hierarchy, 0, null_transform, 1, 1);
	}

	return hierarchy;
}

void
Hierarchy_Delete (hierarchy_t *hierarchy)
{
	if (hierarchy->transform.size) {
		scene_resources_t *res = hierarchy->transform.a[0]->scene->resources;

		for (size_t i = 0; i < hierarchy->transform.size; i++) {
			PR_RESFREE (res->transforms, hierarchy->transform.a[i]);
		}
	}
	DARRAY_CLEAR (&hierarchy->transform);
	DARRAY_CLEAR (&hierarchy->entity);
	DARRAY_CLEAR (&hierarchy->childCount);
	DARRAY_CLEAR (&hierarchy->childIndex);
	DARRAY_CLEAR (&hierarchy->parentIndex);
	DARRAY_CLEAR (&hierarchy->name);
	DARRAY_CLEAR (&hierarchy->tag);
	DARRAY_CLEAR (&hierarchy->modified);
	DARRAY_CLEAR (&hierarchy->localMatrix);
	DARRAY_CLEAR (&hierarchy->localInverse);
	DARRAY_CLEAR (&hierarchy->worldMatrix);
	DARRAY_CLEAR (&hierarchy->worldInverse);
	DARRAY_CLEAR (&hierarchy->localRotation);
	DARRAY_CLEAR (&hierarchy->localScale);
	DARRAY_CLEAR (&hierarchy->worldRotation);
	DARRAY_CLEAR (&hierarchy->worldScale);
	free (hierarchy);
}
