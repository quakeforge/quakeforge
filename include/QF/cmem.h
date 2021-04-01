/*
	cmem.h

	Cache-line aligned memory allocator

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>

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
#ifndef __QF_cmem_h
#define __QF_cmem_h

#include "QF/qtypes.h"

#define MEM_LINE_SIZE 64
#define MAX_CACHE_LINES 9

typedef struct memline_s {
	/* chain of free line blocks for fast allocation
	 * chain begins in memsuper_t
	 */
	struct memline_s *free_next;
	struct memline_s **free_prev;
	/* chain of free line blocks within a membock for merging
	 * chain begins in memblock_t
	 */
	struct memline_s *block_next;
	struct memline_s **block_prev;
	size_t      size;
	/* owning block
	 */
	struct memblock_s *block;
	size_t      pad[2];
} memline_t;

typedef struct memsline_s {
	struct memsline_s *next;
	size_t      size:2;
	size_t      list:4;
	size_t      prev:8 * sizeof (void *) - 6;	// memsline_t **
} memsline_t;

typedef struct memblock_s {
	struct memblock_s *next;
	struct memblock_s **prev;
	/* The pointer to pass to free()
	 */
	void       *mem;
	memline_t  *free_lines;
	/* Size of memory region before block "header".
	 *
	 * Since large blocks are allocated with page-size alignment, odds are
	 * high that the there will be many cache lines "wasted" in the space
	 * between the address returned from aligned_alloc (to cache-line
	 * alignment) and the block itself. Setting them up as a pool makes the
	 * lines available for smaller allocations, thus reducing waste.
	 */
	size_t      pre_size;
	/* Size of memory region after block "header".
	 *
	 * Will be 0 for blocks that were allocated exclusively for small
	 * allocations, otherwise indicates the size of the allocated block.
	 */
	size_t      post_size;
	/* True if the post-header block is free to be reused.
	 */
	int         post_free;
	int         pad;
	size_t      pre_allocated;
} memblock_t;

typedef struct memsuper_s {
	size_t      page_size;
	size_t      page_mask;
	memblock_t *memblocks;
	/* Allocated cache lines from which smaller blocks can be allocated.
	 *
	 * The index is the base-2 log minus 2 of the size of the elements in the
	 * cache line from which an element was last freed. Only 4-32 bytes are of
	 * interest because nothing smaller than 4 bytes (int/float) will be
	 * allocated, and 64 bytes and up consume entire cache lines.
	 */
	memsline_t *last_freed[4];
	/* Free chache lines grouped by size.
	 *
	 * The index is the base-2 log of the MINIMUM number of cache lines
	 * available in each block. ie, blocks with 4, 5, 6 and 7 lines will all
	 * be in the third list (index 2). For 4k page sizes, only 6 lists are
	 * needed (32-63 lines) because a page can hold only 62 lines (1 for the
	 * control block and one to avoid a cache-line being on a page boundary).
	 * Having 9 (MAX_CACHE_LINES) lists allows page sizes up to 16kB.
	 */
	memline_t  *free_lines[MAX_CACHE_LINES];
} memsuper_t;

memsuper_t *new_memsuper (void);
void delete_memsuper (memsuper_t *super);
void *cmemalloc (memsuper_t *super, size_t size);
void cmemfree (memsuper_t *super, void *mem);

/**	High-tide structure allocator for use in linked lists.

	Using a free-list with the name of \c NAME_freelist, return a single
	element.
	The type of the element must be a structure with a field named \c next.
	When the free-list is empty, memory is claimed from the system in blocks.
	Elements may be returned to the pool by linking them into the free-list.

	\param s		The number of structures in the block.
	\param t		The structure type.
	\param n		The \c NAME portion of the \c NAME_freelist free-list.
	\param super	The memsuper_t super block from which to allocate memory.

	\hideinitializer
*/
#define CMEMALLOC(s, t, n, super)									\
	({																\
		if (!n##_freelist) {										\
			int         i;											\
			n##_freelist = cmemalloc ((super), (s) * sizeof (t));	\
			for (i = 0; i < (s) - 1; i++)							\
				n##_freelist[i].next = &n##_freelist[i + 1];		\
			n##_freelist[i].next = 0;								\
		}															\
		t *v = n##_freelist;										\
		n##_freelist = n##_freelist->next;							\
		v;															\
	})

/** Free a block allocated by #ALLOC

	\param n		The \c NAME portion of the \c NAME_freelist free-list.
	\param p		The pointer to the block to be freed.

	\hideinitializer
*/
#define CMEMFREE(n, p)			\
	do {						\
		p->next = n##_freelist;	\
		n##_freelist = p;		\
	} while (0)

#endif//__QF_cmem_h
