/*
	defspace.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/alloc.h"
#include "QF/hash.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"

typedef struct locref_s {
	struct locref_s *next;
	int         ofs;
	int         size;
} locref_t;

static defspace_t *spaces_freelist;
static locref_t *locrefs_freelist;

static locref_t *
new_locref (int ofs, int size, locref_t *next)
{
	locref_t   *loc;

	ALLOC (1024, locref_t, locrefs, loc);
	loc->ofs = ofs;
	loc->size = size;
	loc->next = next;
	return loc;
}

static void
del_locref (locref_t *loc)
{
	FREE (locrefs, loc);
}

static defspace_t *
new_defspace (void)
{
	defspace_t *space;

	ALLOC (1024, defspace_t, spaces, space);
	return space;
}

#define GROW 1024

static int
grow_space_global (defspace_t *space)
{
	int         size;

	if (space->size <= space->max_size)
		return 1;

	size = space->size + GROW;
	size -= size % GROW;
	space->data = realloc (space->data, size * sizeof (pr_type_t));
	memset (space->data + space->max_size, 0, GROW * sizeof (pr_type_t));
	space->max_size = size;
	return 1;
}

static int
grow_space_virtual (defspace_t *space)
{
	if (space->size > space->max_size) {
		space->max_size = space->size;
	}
	return 1;
}

defspace_t *
defspace_new (ds_type_t type)
{
	defspace_t *space = new_defspace ();

	space->def_tail = &space->defs;
	space->type = type;
	if (type == ds_backed) {
		space->grow = grow_space_global;
	} else if (type == ds_virtual) {
		space->grow = grow_space_virtual;
	} else {
		internal_error (0, "unknown defspace type");
	}
	return space;
}

void
defspace_delete (defspace_t *space)
{
	locref_t  **lr;

	for (lr = &space->free_locs; *lr; lr = &(*lr)->next) {
	}
	*lr = locrefs_freelist;
	locrefs_freelist = space->free_locs;

	if (space->data) {
		free (space->data);
	}

	while (space->defs) {
		def_t      *def = space->defs;
		space->defs = def->next;
		def->space = 0;
		free_def (def);
	}
}

int
defspace_alloc_loc (defspace_t *space, int size)
{
	return defspace_alloc_aligned_loc (space, size, 1);
}

int
defspace_alloc_aligned_loc (defspace_t *space, int size, int alignment)
{
	int         ofs, pad;
	locref_t   *loc;
	locref_t  **l = &space->free_locs;

	if (size <= 0)
		internal_error (0, "invalid number of words requested: %d", size);
	if (alignment <= 0)
		internal_error (0, "invalid alignment requested: %d", alignment);
	while ((loc = *l)) {
		ofs = loc->ofs;
		pad = alignment * ((ofs + alignment - 1) / alignment) - ofs;
		// exact fit, so just shrink the block or remove it if there is no
		// padding (any padding remains free)
		if (size + pad == loc->size) {
			loc->size -= size;
			if (!loc->size) {
				*l = loc->next;
				del_locref (loc);
			}
			return ofs + pad;
		}
		// there's excess space in the block. If there's no padding, then
		// just shrink it, otherwise split it into two, one on either side
		// of the allocated block, such that the padding remains free
		if (size + pad < loc->size) {
			if (!pad) {
				loc->ofs += size;
				loc->size -= size;
			} else {
				loc->next = new_locref (ofs + pad + size,
										loc->size - ofs - pad, loc->next);
				loc->size = pad;
			}
			return ofs + pad;
		}
		l = &(*l)->next;
	}
	ofs = space->size;
	pad = alignment * ((ofs + alignment - 1) / alignment) - ofs;
	space->size += size + pad;
	if (space->size > space->max_size) {
		if (!space->grow || !space->grow (space))
			internal_error (0, "unable to allocate %d words", size);
	}
	if (pad) {
		// mark the padding as free
		*l = new_locref (ofs, pad, 0);
	}
	return ofs + pad;
}

void
defspace_free_loc (defspace_t *space, int ofs, int size)
{
	locref_t  **l;
	locref_t   *loc;

	if (size <= 0)
		internal_error (0, "defspace: freeing size %d location", size);
	if (ofs < 0 || ofs >= space->size || ofs + size > space->size)
		internal_error (0, "defspace: freeing bogus location %d:%d",
						ofs, size);
	for (l = &space->free_locs; *l; l = &(*l)->next) {
		loc = *l;
		// location to be freed is below the free block
		// need to create a new free block
		if (ofs + size < loc->ofs)
			break;
		// location to be freed is immediately below the free block
		// merge with the free block
		if (ofs + size == loc->ofs) {
			loc->size += size;
			loc->ofs = ofs;
			return;
		}
		// location to be freed overlaps the free block
		// this is an error
		if (ofs + size > loc->ofs && ofs < loc->ofs + loc->size)
			internal_error (0, "defspace: freeing bogus location %d:%d",
							ofs, size);
		// location to be freed is immediately above the free block
		// merge with the free block
		if (ofs == loc->ofs + loc->size) {
			loc->size += size;
			// location to be freed is immediately below the next free block
			// merge with the next block too, and unlink that block
			if (loc->next && loc->next->ofs == loc->ofs + loc->size) {
				loc->size += loc->next->size;
				loc = loc->next;
				*l = loc->next;
				del_locref (loc);
			}
			return;
		}
	}
	// insert a new free block for the location to be freed
	*l = new_locref (ofs, size, *l);
}

int
defspace_add_data (defspace_t *space, pr_type_t *data, int size)
{
	int         loc;

	loc = defspace_alloc_loc (space, size);
	if (data)
		memcpy (space->data + loc, data, size * sizeof (pr_type_t));
	return loc;
}

int
defspace_alloc_highwater (defspace_t *space, int size)
{
	return defspace_alloc_aligned_highwater (space, size, 1);
}

int
defspace_alloc_aligned_highwater (defspace_t *space, int size, int alignment)
{
	if (size < 0)
		internal_error (0, "invalid number of words requested: %d", size);
	if (alignment <= 0)
		internal_error (0, "invalid alignment requested: %d", alignment);

	int         ofs = space->size;
	int         pad = alignment * ((ofs + alignment - 1) / alignment) - ofs;
	space->size += size + pad;
	if (space->size > space->max_size) {
		if (!space->grow || !space->grow (space))
			internal_error (0, "unable to allocate %d words", size);
	}
	locref_t  **l = &space->free_locs;
	if (pad) {
		// mark the padding as free
		*l = new_locref (ofs, pad, 0);
	}
	return ofs + pad;
}

void
defspace_reset (defspace_t *space)
{
	space->size = 0;
	while (space->free_locs) {
		locref_t   *l = space->free_locs;
		space->free_locs = l->next;
		del_locref (l);
	}
	if (space->data) {
		memset (space->data, 0, space->max_size * sizeof (pr_type_t));
	}
}
