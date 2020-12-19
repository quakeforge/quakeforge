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
#ifndef __cmem_h
#define __cmem_h

#include "QF/qtypes.h"

#define MEM_LINE_SIZE 64

typedef struct memline_s {
	struct memline_s *next;
	size_t      size;
	size_t      pad[6];
} memline_t;

typedef struct memsline_s {
	struct memsline_s *next;
	size_t      size:2;
	size_t      list:4;
	size_t      prev:58;	// memsline_t **
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
	size_t      pad;
} memsuper_t;

memsuper_t *new_memsuper (void);
void delete_memsuper (memsuper_t *super);
void *cmemalloc (memsuper_t *super, size_t size);
void cmemfree (memsuper_t *super, void *mem);

#endif//__cmem_h
