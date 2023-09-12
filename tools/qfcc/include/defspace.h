/*
	defspace.h

	management of data segments

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/16

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

#ifndef __defspace_h
#define __defspace_h

#include "QF/progs/pr_comp.h"
#include "QF/progs/pr_debug.h"

/** \defgroup qfcc_defspace Defspace handling
	\ingroup qfcc
*/
///@{

typedef enum {
	ds_backed,		///< data space is globally addressable (near/far/type) and
					///< has backing store
	ds_virtual,		///< data space has no backing store (local vars, entity
					///< fields) defspace_t::max_size reflects the highest
					///< address allocated
} ds_type_t;

/** Represent a block of memory in the progs data space.
*/
typedef struct defspace_s {
	struct defspace_s *next;	///< for ALLOC
	ds_type_t   type;			///< basic type of defspace
	struct locref_s *free_locs;	///< list of free blocks in this space
	struct def_s *defs;			///< list of defs using this space
	struct def_s **def_tail;	///< for appending to \a defs
	pr_type_t  *data;			///< backing memory for this space
	int         alignment;		///< minimum alignment of the whole space
	int         size;			///< current high-water mark for alloced data
	int         max_size;		///< size of backing memory, or highwater mark
								///< for ds_virtual
	/** Grow the backing memory of the defspace.

		This function is called when more memory is needed for the space.
		The default \a grow function for ds_backed defspaces expands the
		backing memory by 1024 pr_type_t words and initializes them to 0.
		Other defspace types do not actually allocate backing memory as it
		is not needed.  If null, more space cannot be allocated and an
		internal error will be generated.

		\param space	This defspace.
		\return			1 for success, 0 for failure. On failure, an internal
						error will be generated.

		\note Setting to null forces failure and thus an internal error.
	*/
	int       (*grow) (struct defspace_s *space);
	int         qfo_space;		///< index to space in qfo spaces
} defspace_t;

/** Create an empty defspace.

	No backing memory is allocated, but the defspace_t::grow function is
	initialized to the default grow function so the backing memory may be
	accessed after space has been allocated via defspace_alloc_loc().

	\param	type	The type of defspace to create. Affects only the default
					defspace_t::grow function: ds_backed uses a grow function
					that allocates backing memory, while ds_virtual uses a
					grow function that only increases defspace_t::max_size
	\return			The new, empty defspace.
*/
defspace_t *defspace_new (ds_type_t type);

void defspace_delete (defspace_t *defspace);

/** Allocate space from the defspace's backing memory.

	If the memory is fragmented, then the first available location at least
	as large as \a size is returned. This means that freeing a location then
	allocating the same amount of space may return a different location.

	If memory cannot be allocated (there is no free space in the currently
	available memory and defspace_t::grow is null), then an internal error
	will be generated.

	\param space	The space from which to allocate data.
	\param size		The amount of pr_type_t words to allocated. int and float
					need 1 word, vector 3 words, and quaternion 4.
	\return			The offset of the first word of the freshly allocated
					space. May be 0 if the allocated space is at the beginning
					of the defspace.
*/
int defspace_alloc_loc (defspace_t *space, int size);

/** Allocate space from the defspace's backing memory.

	If the memory is fragmented, then the first available location at least
	as large as \a size plus padding for alignment is returned. This means
	that freeing a location then allocating the same amount of space may
	return a different location.

	If memory cannot be allocated (there is no free space in the currently
	available memory and defspace_t::grow is null), then an internal error
	will be generated.

	\param space	The space from which to allocate data.
	\param size		The amount of pr_type_t words to allocated. int and float
					need 1 word, vector 3 words, and quaternion 4.
	\param alignment The alignment of the allocated space.
	\return			The offset of the first word of the freshly allocated
					space. May be 0 if the allocated space is at the beginning
					of the defspace.
*/
int defspace_alloc_aligned_loc (defspace_t *space, int size, int alignment);

/** Free a block of contiguous words, returning them to the defspace.

	The block to be freed is specified by \a ofs indicating the offset of the
	first word of the block and \a size indicating the number of words in the
	block.

	If the block to be freed has 0 words, or if the block is partly or fully
	outside the defspace (as defined by defspace_t::size), or if the block
	overlaps any unallocated space in the defspace, then an internal error
	will be generated. However, it is perfectly valid to allocate a large
	block and subsequently free a small block from anywhere within the larger
	block.  This is because when memory is not fragmented, there is no
	difference between allocating one large block and allocating several
	smaller blocks when allocating the same amount of memory.

	\param space	The space to which the freed block will be returned.
	\param ofs		The first word of the block to be freed.
	\param size		The number of words in the block to be freed.
*/
void defspace_free_loc (defspace_t *space, int ofs, int size);

/** Copy a block of data into a defspace.

	Space for the data is allocated from the defspace via
	defspace_alloc_loc().

	If \a data is null, then the copying stage is skipped and this function
	becomes a synonym for defspace_alloc_loc().

	\param space	The space to which the data will be added.
	\param data		The data to be copied into the space.
	\param size		The number of words of data to be copied.
	\return			The offset of the block allocated for the data.
*/
int defspace_add_data (defspace_t *space, pr_type_t *data, int size);

/** Allocate a block of data from the end of the defspace.

	If memory cannot be allocated (there is no free space in the currently
	available memory and defspace_t::grow is null), then an internal error
	will be generated.

	\param space	The space from which to allocate data.
	\param size		The amount of pr_type_t words to allocated. int and float
					need 1 word, vector 3 words, and quaternion 4.
	\return			The offset of the first word of the freshly allocated
					space. May be 0 if the allocated space is at the beginning
					of the defspace.
*/
int defspace_alloc_highwater (defspace_t *space, int size);

/** Allocate an aligned block of data from the end of the defspace.

	Any unallocated holes in the defspace are ignored, even if the hole is
	at the end of the defspace. However, any holes created by the padding
	required for aligning the block will be available to defspace_alloc_loc()
	and defspace_alloc_aligned_loc().

	If memory cannot be allocated (there is no free space in the currently
	available memory and defspace_t::grow is null), then an internal error
	will be generated.

	\param space	The space from which to allocate data.
	\param size		The amount of pr_type_t words to allocated. int and float
					need 1 word, vector 3 words, and quaternion 4.
	\param alignment The alignment of the allocated space.
	\return			The offset of the first word of the freshly allocated
					space. May be 0 if the allocated space is at the beginning
					of the defspace.
*/
int defspace_alloc_aligned_highwater (defspace_t *space, int size,
									  int alignment);
/** Reset a defspace, freeing all allocated memory.

	defspace_t::max_size is not affected. This allows the defspace to be used
	to find the larged block of memory required for a set of operations (eg,
	the largest parameter block required in a function for all its calls
	allowing the stack to remain constant instead of using many push/pop
	operations. Note that this works best with ds_virtual defspaces.

	If the defspace has backing memory (ds_backed), the memory is not freed,
	but it is zeroed so any new allocations will contain zeroed memory.

	\param space	The space to be reset.
*/
void defspace_reset (defspace_t *space);

void defspace_sort_defs (defspace_t *space);

///@}

#endif//__defspace_h
