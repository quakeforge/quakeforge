/*
	cmem.c

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
#include <unistd.h>

#include "QF/alloc.h"
#include "QF/cmem.h"

static size_t __attribute__((const))
ilog2 (size_t x)
{
	size_t      l = 0;
	while (x >>= 1) {
		l++;
	}
	return l;
}

memsuper_t *
new_memsuper (void)
{
	memsuper_t *super = aligned_alloc (MEM_LINE_SIZE, sizeof (*super));
	memset (super, 0, sizeof (*super));
	super->page_size = sysconf (_SC_PAGESIZE);
	super->page_mask = (super->page_size - 1);
	return super;
}

void
delete_memsuper (memsuper_t *super)
{
	while (super->memblocks) {
		memblock_t *t = super->memblocks;
		super->memblocks = super->memblocks->next;
		free (t->mem);
	}
	free (super);
}

static void
link_free_line (memsuper_t *super, memline_t *line)
{
	size_t      ind = ilog2 (line->size) - 6;
	if (super->free_lines[ind]) {
		super->free_lines[ind]->free_prev = &line->free_next;
	}
	line->free_next = super->free_lines[ind];
	line->free_prev = &super->free_lines[ind];
	super->free_lines[ind] = line;
}

static void
unlink_free_line (memline_t *line)
{
	if (line->free_next) {
		line->free_next->free_prev = line->free_prev;
	}
	*line->free_prev = line->free_next;
}

static void
unlink_line (memline_t *line)
{
	if (line->block_next) {
		line->block_next->block_prev = line->block_prev;
	}
	*line->block_prev = line->block_next;

	unlink_free_line (line);
}

static memblock_t *
init_block (memsuper_t *super, void *mem, size_t alloc_size)
{
	size_t      size = super->page_size;
	size_t      mask = super->page_mask;
	size_t      ptr = (size_t) mem;
	memblock_t *block;

	block = (memblock_t *) (((ptr + size) & ~mask) - sizeof (memblock_t));
	memset (block, 0, sizeof (memblock_t));

	if (super->memblocks) {
		super->memblocks->prev = &block->next;
	}
	block->next = super->memblocks;
	block->prev = &super->memblocks;
	super->memblocks = block;

	block->mem = mem;
	block->pre_size = (size_t) block - (size_t) mem;
	block->post_size = alloc_size - block->pre_size - sizeof (memblock_t);
	if (!((size_t) mem & mask) && block->pre_size) {
		// can't use the first cache line of the page as it would be
		// indistinguishable from a large block
		block->pre_size -= MEM_LINE_SIZE;
	}
	if (block->pre_size) {
		memline_t  *line = (memline_t *) ((size_t) block - block->pre_size);

		line->block = block;
		line->size = block->pre_size;

		line->block_next = 0;
		line->block_prev = &block->free_lines;
		block->free_lines = line;

		link_free_line (super, line);
	}
	return block;
}

static memblock_t *
block_alloc (memsuper_t *super, size_t size)
{
	memblock_t *block;
	memblock_t *best = 0;
	size_t      best_size = ~0u;

	for (block = super->memblocks; block; block = block->next) {
		if (block->post_free && block->post_size >= size
			&& block->post_size < best_size) {
			best = block;
			best_size = block->post_size;
		}
	}
	if (best) {
		best->post_free = 0;
		return best;
	}

	size_t      page_size = super->page_size;
	size_t      alloc_size = sizeof (memblock_t) + page_size + size;
	void       *mem = aligned_alloc (MEM_LINE_SIZE, alloc_size);
	block = init_block (super, mem, alloc_size);
	return block;
}

static void *
alloc_line (memline_t *line, size_t size)
{
	void       *mem = line;

	if (line->size > size) {
		// split the line block and insert the new block into the list
		memline_t  *split = (memline_t *)((size_t) line + size);
		split->block = line->block;
		split->size = line->size - size;
		line->size = size;

		split->block_next = line->block_next;
		if (split->block_next) {
			split->block_next->block_prev = &split->block_next;
		}
		line->block_next = split;
		split->block_prev = &line->block_next;

		split->free_next = line->free_next;
		if (split->free_next) {
			split->free_next->free_prev = &split->free_next;
		}
		line->free_next = split;
		split->free_prev = &line->free_next;
	}
	line->block->pre_allocated += line->size;
	unlink_line (line);
	return mem;
}

static void
line_free (memsuper_t *super, memblock_t *block, void *mem)
{
	//FIXME right now, can free only single lines (need allocated lines to
	// have a control block)
	size_t      size = MEM_LINE_SIZE;
	memline_t **l;
	memline_t  *line = 0;

	block->pre_allocated -= size;

	for (l = &block->free_lines; *l; l = &(*l)->block_next) {
		line = *l;
		if (line->block_next && line->block_next < line) {
			*(int *)0 = 0;
		}
		if ((size_t) mem + size < (size_t) line) {
			// line to be freed is below the free line
			break;
		}
		if ((size_t) mem + size == (size_t) line) {
			// line to be freed is immediately below the free line
			// merge with the free line by "allocating" the line and then
			// "freeing" it with the line to be freed
			size += line->size;
			unlink_line (line);	// does not modify line->block_next
			line = line->block_next;
			break;
		}
		if ((size_t) line + line->size == (size_t) mem) {
			// line to be freed is immediately above the free line
			// merge with the free line by growing the line
			line->size += size;
			if (line->block_next
				&& (size_t) line->block_next == (size_t) mem + size) {
				// the line to be freed connects two free lines
				line->size += line->block_next->size;
				unlink_line (line->block_next);
			}
			// the line changed size so needs to be relinked in the super
			unlink_free_line (line);
			link_free_line (super, line);
			return;
		}
		if ((size_t) mem >= (size_t) line
			&& (size_t) mem < (size_t) line + line->size) {
			*(int *) 0 = 0;
		}
		line = 0;
	}
	memline_t  *memline = (memline_t *) mem;
	memline->block_next = line;
	if (memline->block_next) {
		memline->block_next->block_prev = &memline->block_next;
	}
	memline->block_prev = l;
	memline->size = size;
	memline->block = block;
	*l = memline;
	link_free_line (super, memline);
}

static memsline_t *
sline_new (memsuper_t *super, size_t size_ind)
{
	size_t      size = 4 << size_ind;
	size_t      free_loc = (sizeof (memsline_t) + size - 1) & ~(size - 1);
	memsline_t *sline = cmemalloc (super, MEM_LINE_SIZE);
	sline->size = size_ind;
	sline->list = free_loc >> 2;
	while (free_loc + size < MEM_LINE_SIZE) {
		*(uint16_t *)((size_t) sline + free_loc) = free_loc + size;
		free_loc += size;
	}
	*(uint16_t *)((size_t) sline + free_loc) = 0;
	if (super->last_freed[size_ind]) {
		super->last_freed[size_ind]->prev = (size_t) &sline->next >> 6;
	}
	sline->next = super->last_freed[size_ind];
	sline->prev = (size_t) &super->last_freed[size_ind] >> 6;
	super->last_freed[size_ind] = sline;
	return sline;
}

void *
cmemalloc (memsuper_t *super, size_t size)
{
	size_t      ind = 0;
	// allocation sizes start at 4 (sizeof(float)) and go up in powers of two
	while ((4u << ind) < size) {
		ind++;
	}
	// round size up
	if (size > MEM_LINE_SIZE * 8 || size > super->page_size / 8) {
		// the object is large enough it could cause excessive fragmentation,
		memblock_t *block = block_alloc (super, 4 << ind);
		if (!block) {
			return 0;
		}
		return block + 1;
	} else {
		size = 4 << ind;
		if (size >= MEM_LINE_SIZE) {
			// whole cache lines are required for this object
			// convert from byte log2 to cache-line log2
			ind -= 4;
			memline_t  *line = 0;

			while (!line && ind < MAX_CACHE_LINES) {
				line = super->free_lines[ind++];
			}
			while (line && line->size < size) {
				line = line->free_next;
			}
			if (!line) {
				// need a new line, one that doesn't make me fe... wrong song
				void       *mem;
				/* The cache-line pool is page aligned for two reasons:
				 * 1) so it fits exactly within a page
				 * 2) the control block can be found easily
				 * And the reason the pool is exactly one page large is so no
				 * allocated line is ever page-aligned as that would make the
				 * line indistinguishable from a large block.
				 */
				mem = aligned_alloc (super->page_size, super->page_size);
				// sets super->free_lines, the block is guarnateed to be big
				// enough to hold the requested allocation as otherwise a full
				// block allocation would have been used
				memblock_t *block = init_block (super, mem, super->page_size);
				line = block->free_lines;
			}
			return alloc_line (line, size);
		} else {
			void       *mem = 0;
			memsline_t **sline = &super->last_freed[ind];
			if (!*sline) {
				*sline = sline_new (super, ind);
			}
			if (*sline) {
				size_t      list = (*sline)->list << 2;
				mem = (void *) ((size_t) *sline + list);
				(*sline)->list = *(uint16_t *) mem >> 2;
				if (!(*sline)->list) {
					// the sub-line is full, so remove it from the free
					// list. Freeing a block from the line will add it back
					// to the list
					memsline_t *s = *sline;
					if ((*sline)->next) {
						(*sline)->next->prev = (*sline)->prev;
					}
					*sline = (*sline)->next;
					s->next = 0;
					s->prev = 0;
				}
			}
			return mem;
		}
	}
	return 0;
}

static void
unlink_block (memblock_t *block)
{
	if (block->pre_size) {
		if (!block->free_lines || block->free_lines->block_next) {
			*(int *) 0 = 0;
		}
		unlink_line (block->free_lines);
	}
	if (block->next) {
		block->next->prev = block->prev;
	}
	*block->prev = block->next;
}

void
cmemfree (memsuper_t *super, void *mem)
{
	memsline_t **super_sline;
	memsline_t *sline;
	memblock_t *block;

	if ((size_t) mem & (MEM_LINE_SIZE - 1)) {
		// sub line block
		sline = (memsline_t *) ((size_t) mem & ~(MEM_LINE_SIZE - 1));
		*(uint16_t *) mem = sline->list << 2;
		sline->list = ((size_t) mem & (MEM_LINE_SIZE - 1)) >> 2;
		super_sline = &super->last_freed[sline->size];
		if (*super_sline != sline) {
			if (sline->next) {
				sline->next->prev = sline->prev;
			}
			if (sline->prev) {
				*(memsline_t **) (size_t)(sline->prev << 6) = sline->next;
			}

			if (*super_sline) {
				(*super_sline)->prev = (size_t) &sline->next >> 6;
			}
			sline->next = *super_sline;
			sline->prev = (size_t) super_sline >> 6;
			(*super_sline) = sline;
		}
		return;
	} else if ((size_t) mem & super->page_mask) {
		// cache line
		size_t      page_size = super->page_size;
		size_t      page_mask = super->page_mask;
		block = (memblock_t *) (((size_t) mem + page_size) & ~page_mask) - 1;
		line_free (super, block, mem);
	} else {
		// large block
		block = (memblock_t *) mem - 1;
		block->post_free = 1;
	}
	if (!block->pre_allocated && (!block->post_size || block->post_free)) {
		unlink_block (block);
		free (block->mem);
	}
}
