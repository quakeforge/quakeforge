/*
	zone.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include <stdarg.h>
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "compat.h"

static void Cache_FreeLow (memhunk_t *hunk, int new_low_hunk);
static void Cache_Profile_r (memhunk_t *hunk);
static qboolean Cache_FreeLRU (memhunk_t *hunk);

#define	ZONEID			0x1d4a11
#define	HUNK_SENTINAL	0x1df001ed

#define MINFRAGMENT	64
#define HUNK_ALIGN 64

/*
   						ZONE MEMORY ALLOCATION

	There is never any space between memblocks, and there will never be two
	contiguous free memblocks.

	The rover can be left pointing at a non-empty block

	The zone calls are pretty much used only for small strings and structures,
	all big things are allocated on the hunk.
*/

typedef struct memblock_s {
	size_t      block_size;	// including the header and possibly tiny fragments
	struct memblock_s *next;
	struct memblock_s *prev;
	size_t      size;		// requested size
	int         tag;		// a tag of 0 is a free block
	int         id;			// should be ZONEID
} __attribute__((aligned (64))) memblock_t;

struct memzone_s {
	size_t      size;		// total bytes malloced, including header
	size_t      used;		// ammount used, including header
	size_t      offset;
	size_t      ele_size;
	void        (*error) (void *, const char *);
	void       *data;
	memblock_t *rover;
	memblock_t  blocklist;	// start / end cap for linked list
} __attribute__((aligned (64)));

static int
z_block_size (memblock_t *block)
{
	return block->block_size - sizeof (memblock_t) - 4;
}

static int
z_offset (memzone_t *zone, memblock_t *block)
{
	int         offset = ((byte *) (block + 1) - (byte *) zone);

	return offset / zone->ele_size + zone->offset;
}

VISIBLE void
Z_ClearZone (memzone_t *zone, size_t size, size_t zone_offset, size_t ele_size)
{
	memblock_t	*block;

	// set the entire zone to one free block

	block = (memblock_t *) (zone + 1);
	zone->blocklist.next = block;
	zone->blocklist.prev = block;
	zone->blocklist.tag = 1;	// in use block
	zone->blocklist.id = 0;
	zone->blocklist.block_size = 0;
	zone->blocklist.size = 0;
	zone->offset = zone_offset;
	zone->ele_size = ele_size;
	zone->rover = block;
	zone->size = size;
	zone->used = sizeof (memzone_t);
	zone->error = 0;
	zone->data = 0;

	block->prev = block->next = &zone->blocklist;
	block->tag = 0;			// free block
	block->id = ZONEID;
	//block->id2 = ZONEID;
	block->block_size = size - sizeof (memzone_t);
	block->size = 0;
}

VISIBLE void
Z_Free (memzone_t *zone, void *ptr)
{
	memblock_t	*block, *other;

	if (!ptr) {
		if (zone->error)
			zone->error (zone->data, "Z_Free: NULL pointer");
		Sys_Error ("Z_Free: NULL pointer");
	}

	block = (memblock_t *) ((byte *) ptr - sizeof (memblock_t));
	if (((byte *) block < (byte *) zone)
		|| (((byte *) block) >= (byte *) zone + zone->size)) {
		const char *msg;
		msg = nva ("Z_Free: freed a pointer outside of the zone: %x",
				   z_offset (zone, block));
		if (zone->error)
			zone->error (zone->data, msg);
		Sys_Error ("%s", msg);
	}
	if (block->id != ZONEID/* || block->id2 != ZONEID*/) {
		const char *msg;
		msg = nva ("bad pointer %x", z_offset (zone, block));
		Sys_Printf ("%s\n", msg);
		Z_Print (zone);
		fflush (stdout);
		if (zone->error)
			zone->error (zone->data, msg);
		Sys_Error ("Z_Free: freed a pointer without ZONEID");
	}
	if (block->tag == 0) {
		if (zone->error)
			zone->error (zone->data, "Z_Free: freed a freed pointer");
		Sys_Error ("Z_Free: freed a freed pointer");
	}

	block->tag = 0;		// mark as free
	block->size = 0;
	zone->used -= block->block_size;

	other = block->prev;
	if (!other->tag) {
		// merge with previous free block
		other->block_size += block->block_size;
		other->next = block->next;
		other->next->prev = other;
		if (block == zone->rover)
			zone->rover = other;
		block = other;
	}

	other = block->next;
	if (!other->tag) {
		// merge the next free block onto the end
		block->block_size += other->block_size;
		block->next = other->next;
		block->next->prev = block;
		if (other == zone->rover)
			zone->rover = block;
	}
}

VISIBLE void *
Z_Malloc (memzone_t *zone, size_t size)
{
	void	*buf;

	buf = Z_TagMalloc (zone, size, 1);
	if (!buf) {
		const char *msg;
		msg = nva ("Z_Malloc: failed on allocation of %zd bytes", size);
		if (zone->error)
			zone->error (zone->data, msg);
		Sys_Error ("%s", msg);
	}
	memset (buf, 0, size);

	return buf;
}

void *
Z_TagMalloc (memzone_t *zone, size_t size, int tag)
{
	int         extra;
	int         requested_size = size;
	memblock_t *start, *rover, *new, *base;

	if (developer & SYS_zone)
		Z_CheckHeap (zone);	// DEBUG

	if (!tag) {
		if (zone->error)
			zone->error (zone->data, "Z_TagMalloc: tried to use a 0 tag");
		Sys_Error ("Z_TagMalloc: tried to use a 0 tag");
	}

	// scan through the block list looking for the first free block
	// of sufficient size
	size += sizeof (memblock_t);	// account for size of block header
	size += 4;						// space for memory trash tester
	size = (size + 7) & ~7;			// align to 8-byte boundary

	base = rover = zone->rover;
	start = base->prev;

	do {
		if (rover == start)			// scaned all the way around the list
			return NULL;
		if (rover->tag)
			base = rover = rover->next;
		else
			rover = rover->next;
	} while (base->tag || base->block_size < size);

	// found a block big enough
	extra = base->block_size - size;
	if (extra >  MINFRAGMENT) {
		// there will be a free fragment after the allocated block
		new = (memblock_t *) ((byte *) base + size);
		new->block_size = extra;
		new->tag = 0;			// free block
		new->prev = base;
		new->id = ZONEID;
		//new->id2 = ZONEID;
		new->next = base->next;
		new->next->prev = new;
		base->next = new;
		base->block_size = size;
	}

	base->tag = tag;				// no longer a free block
	base->size = requested_size;

	zone->rover = base->next;	// next allocation will start looking here

	base->id = ZONEID;
	//base->id2 = ZONEID;

	zone->used += base->block_size;

	// marker for memory trash testing
	*(int *) ((byte *) base + base->block_size - 4) = ZONEID;

	return (void *) (base + 1);
}

VISIBLE void *
Z_Realloc (memzone_t *zone, void *ptr, size_t size)
{
	size_t      old_size;
	memblock_t *block;
	void       *old_ptr;

	if (!ptr)
		return Z_Malloc (zone, size);

	if (developer & SYS_zone)
		Z_CheckHeap (zone);	// DEBUG

	block = (memblock_t *) ((byte *) ptr - sizeof (memblock_t));
	if (block->id != ZONEID/* || block->id2 != ZONEID*/) {
		if (zone->error)
			zone->error (zone->data,
						 "Z_Realloc: realloced a pointer without ZONEID");
		Sys_Error ("Z_Realloc: realloced a pointer without ZONEID");
	}
	if (block->tag == 0) {
		if (zone->error)
			zone->error (zone->data, "Z_Realloc: realloced a freed pointer");
		Sys_Error ("Z_Realloc: realloced a freed pointer");
	}

	old_size = block->block_size;
	old_size -= sizeof (memblock_t);	// account for size of block header
	old_size -= 4;						// space for memory trash tester
	old_ptr = ptr;

	Z_Free (zone, ptr);
	ptr = Z_TagMalloc (zone, size, 1);
	if (!ptr) {
		const char *msg;
		msg = nva ("Z_Realloc: failed on allocation of %zd bytes", size);
		if (zone->error)
			zone->error (zone->data, msg);
		Sys_Error ("%s", msg);
	}

	if (ptr != old_ptr)
		memmove (ptr, old_ptr, min (old_size, size));
	if (old_size < size)
		memset ((byte *)ptr + old_size, 0, size - old_size);

	return ptr;
}

void
Z_Print (memzone_t *zone)
{
	memblock_t	*block;

	Sys_Printf ("zone size: %zd  location: %p  used: %zd\n",
				zone->size, zone, zone->used);

	for (block = zone->blocklist.next ; ; block = block->next) {
		Sys_Printf ("block:%p    size:%7i    tag:%5x ofs:%x\n",
					block, z_block_size (block),
					block->tag, z_offset (zone, block));

		if (block->next == &zone->blocklist)
			break;			// all blocks have been hit
		if (block->id != ZONEID/* || block->id2 != ZONEID*/)
			Sys_Printf ("ERROR: block ids incorrect\n");
		if ((byte *) block + block->block_size != (byte *) block->next)
			Sys_Printf ("ERROR: block size does not touch the next block\n");
		if (block->next->prev != block)
			Sys_Printf ("ERROR: next block doesn't have proper back link\n");
		if (!block->tag && !block->next->tag)
			Sys_Printf ("ERROR: two consecutive free blocks\n");
		int     id = *(int *) ((byte *) block + block->block_size - 4);
		if (block->tag && (id != ZONEID))
			Sys_Printf ("ERROR: memory trashed in block %x != %x\n",
						id, ZONEID);
		fflush (stdout);
	}
}

static void
z_error (memzone_t *zone, const char *msg)
{
	if (zone->error)
		zone->error (zone->data, msg);
	Sys_Error ("%s", msg);
}

void
Z_CheckHeap (memzone_t *zone)
{
	memblock_t	*block;

	for (block = zone->blocklist.next ; ; block = block->next) {
		if (block->next == &zone->blocklist)
			break;			// all blocks have been hit
		if ((byte *) block + block->block_size != (byte *) block->next)
			z_error (zone,
					 "Z_CheckHeap: block size does not touch the next block");
		if (block->next->prev != block)
			z_error (zone,
					 "Z_CheckHeap: next block doesn't have proper back link");
		if (!block->tag && !block->next->tag)
			z_error (zone, "Z_CheckHeap: two consecutive free blocks");
		if (block->id != ZONEID/* || block->id2 != ZONEID*/)
			z_error (zone, "ERROR: block ids incorrect");
		if ((byte *) block + block->block_size != (byte *) block->next)
			z_error (zone, "ERROR: block size does not touch the next block");
		if (block->next->prev != block)
			z_error (zone, "ERROR: next block doesn't have proper back link");
		if (!block->tag && !block->next->tag)
			z_error (zone, "ERROR: two consecutive free blocks");
		if (block->tag
			&& (*(int *) ((byte *) block + block->block_size - 4) != ZONEID))
			z_error (zone, "ERROR: memory trashed in block");
	}
}

VISIBLE void
Z_SetError (memzone_t *zone, void (*err) (void *, const char *), void *data)
{
	zone->error = err;
	zone->data = data;
}

VISIBLE void
Z_CheckPointer (const memzone_t *zone, const void *ptr, size_t size)
{
	const memblock_t *block;
	const char *block_mem;
	const char *check = (char *) ptr;

	for (block = zone->blocklist.next ; ; block = block->next) {
		if (block->next == &zone->blocklist)
			break;			// all blocks have been hit
		if (check < (const char *) block
			|| check >= (const char *) block + block->block_size)
			continue;
		// a block that overlaps with the memory region has been found
		if (!block->tag)
			zone->error (zone->data, "invalid access to unallocated memory");
		block_mem = (char *) &block[1];
		if (check < block_mem || check + size > block_mem + block->size)
			zone->error (zone->data, "invalid access to allocated memory");
		return;		// access ok
	}
}

VISIBLE void
Z_MemInfo (const memzone_t *zone, size_t *used, size_t *size)
{
	*used = zone->used;
	*size = zone->size;
}

//============================================================================

typedef struct cache_system_s cache_system_t;
struct cache_system_s {
	uint32_t    prev;
	uint32_t    next;
	uint32_t    lru_prev;
	uint32_t    lru_next;
	struct memhunk_s *hunk;
	size_t      size;							// including this header
	cache_user_t *user;
	char        name[16];
	int			readlock;
} __attribute__((aligned (64)));

typedef struct {
	int         sentinal1;
	int         sentinal2;
	size_t      size;		// including sizeof(hunkblk_t), -1 = not allocated
	char        name[16];
} __attribute__((aligned (64))) hunkblk_t;

struct memhunk_s {
	byte       *base;
	size_t      size;
	size_t      low_used;
	size_t      high_used;
	size_t      tempmark;
	qboolean    tempactive;
	cache_system_t cache_head[1];
} __attribute__((aligned (64)));

static cache_system_t *
cs_ptr (memhunk_t *hunk, uint32_t cs_ind)
{
	return &hunk->cache_head[cs_ind];
}

static uint32_t
cs_ind (memhunk_t *hunk, cache_system_t *cs_ptr)
{
	return cs_ptr - hunk->cache_head;
}

static memhunk_t *global_hunk;

/*
	Hunk_Check

	Run consistancy and sentinal trahing checks
*/
VISIBLE void
Hunk_Check (memhunk_t *hunk)
{
	if (!hunk) { hunk = global_hunk; } //FIXME clean up callers
	hunkblk_t  *h;
	byte       *hunk_end = hunk->base + hunk->low_used;

	for (h = (hunkblk_t *) hunk->base; (byte *) h < hunk_end; ) {
		if (h->sentinal1 != HUNK_SENTINAL || h->sentinal2 != HUNK_SENTINAL)
			Sys_Error ("Hunk_Check: trashed sentinal");
		if (h->size < sizeof (hunkblk_t)
			|| h->size + (byte *) h > hunk->base + hunk->size)
			Sys_Error ("Hunk_Check: bad size");
		h = (hunkblk_t *) ((byte *) h + h->size);
	}
}

/*
	Hunk_Print

	If "all" is specified, every single allocation is printed.
	Otherwise, allocations with the same name will be totaled up before
	printing.
*/

VISIBLE void
Hunk_Print (memhunk_t *hunk, qboolean all)
{
	if (!hunk) { hunk = global_hunk; } //FIXME clean up callers
	hunkblk_t  *h, *next, *endlow, *starthigh, *endhigh;
	int         count, sum, totalblocks;

	count = 0;
	sum = 0;
	totalblocks = 0;

	h = (hunkblk_t *) hunk->base;
	endlow = (hunkblk_t *) (hunk->base + hunk->low_used);
	starthigh = (hunkblk_t *) (hunk->base + hunk->size - hunk->high_used);
	endhigh = (hunkblk_t *) (hunk->base + hunk->size);

	Sys_Printf ("          :%8zd total hunk size\n", hunk->size);
	Sys_Printf ("-------------------------\n");

	while (1) {
		// skip to the high hunk if done with low hunk
		if (h == endlow) {
			Sys_Printf ("-------------------------\n");
			Sys_Printf ("          :%8zd REMAINING\n",
						hunk->size - hunk->low_used - hunk->high_used);
			Sys_Printf ("-------------------------\n");
			h = starthigh;
		}
		// if totally done, break
		if (h == endhigh)
			break;

		// run consistancy checks
		if (h->sentinal1 != HUNK_SENTINAL || h->sentinal2 != HUNK_SENTINAL)
			Sys_Error ("Hunk_Check: trahsed sentinal");
		if (h->size < (int) sizeof (hunkblk_t)
			|| h->size + (byte *) h > hunk->base + hunk->size)
			Sys_Error ("Hunk_Check: bad size");

		next = (hunkblk_t *) ((byte *) h + h->size);
		count++;
		totalblocks++;
		sum += h->size;

		// print the single block
		if (all) {
			const int   sz = sizeof (h->name);
			Sys_Printf ("%8p :%8zd %*.*s\n", h, h->size, sz, sz, h->name);
		}

		// print the total
		if (next == endlow || next == endhigh ||
			strncmp (h->name, next->name, sizeof (h->name))) {
			if (!all) {
				const int   sz = sizeof (h->name);
				Sys_Printf ("          :%8i %*.*s (TOTAL)\n",
							sum, sz, sz, h->name);
			}
			count = 0;
			sum = 0;
		}

		h = next;
	}

	Sys_Printf ("-------------------------\n");
	Sys_Printf ("%8i total blocks\n", totalblocks);
}

static void
Hunk_FreeToHighMark (memhunk_t *hunk, size_t mark)
{
	if (!hunk) { hunk = global_hunk; } //FIXME clean up callers
	if (hunk->tempactive) {
		hunk->tempactive = false;
		Hunk_FreeToHighMark (hunk, hunk->tempmark);
	}
	if (mark == hunk->high_used)
		return;
	if (mark > hunk->high_used)
		Sys_Error ("Hunk_FreeToHighMark: bad mark %zd", mark);
	memset (hunk->base + hunk->size - hunk->high_used, 0,
			hunk->high_used - mark);
	hunk->high_used = mark;
}

static int
Hunk_HighMark (memhunk_t *hunk)
{
	if (!hunk) { hunk = global_hunk; } //FIXME clean up callers
	if (hunk->tempactive) {
		hunk->tempactive = false;
		Hunk_FreeToHighMark (hunk, hunk->tempmark);
	}

	return hunk->high_used;
}

VISIBLE void *
Hunk_RawAllocName (memhunk_t *hunk, size_t size, const char *name)
{
	hunkblk_t  *h;

#ifdef PARANOID
	Hunk_Check ();
#endif

	size = sizeof (hunkblk_t) + ((size + HUNK_ALIGN - 1) & ~(HUNK_ALIGN - 1));

	if (hunk->size - hunk->low_used - hunk->high_used < size) {
		Hunk_HighMark (hunk);
		Cache_FreeLRU (hunk);
	}
	if (hunk->size - hunk->low_used - hunk->high_used < size) {
		int         mem = hunk->size / (1024 * 1024);
		mem += 8;
		mem &= ~7;
		Cache_Profile_r (hunk);
		Sys_Error
			("Not enough RAM allocated.  Try starting using \"-mem %d\" on "
			 "the %s command line. (%zd -  %zd - %zd < %zd)", mem,
			 PACKAGE_NAME, hunk->size, hunk->low_used, hunk->high_used, size);
	}

	h = (hunkblk_t *) (hunk->base + hunk->low_used);
	hunk->low_used += size;

	Cache_FreeLow (hunk, hunk->low_used);

	h->size = size;
	h->sentinal1 = HUNK_SENTINAL;
	h->sentinal2 = HUNK_SENTINAL;
	memccpy (h->name, name, 0, sizeof (h->name));

	return (void *) (h + 1);
}

VISIBLE void *
Hunk_AllocName (memhunk_t *hunk, size_t size, const char *name)
{
	if (!hunk) { hunk = global_hunk; } //FIXME clean up callers
	void       *mem = Hunk_RawAllocName (hunk, size, name);
	memset (mem, 0, size);
	return mem;
}

VISIBLE void *
Hunk_RawAlloc (memhunk_t *hunk, size_t size)
{
	return Hunk_RawAllocName (hunk, size, "unknown");
}

VISIBLE void *
Hunk_Alloc (memhunk_t *hunk, size_t size)
{
	if (!hunk) { hunk = global_hunk; } //FIXME clean up callers
	return Hunk_AllocName (hunk, size, "unknown");
}

VISIBLE size_t
Hunk_LowMark (memhunk_t *hunk)
{
	if (!hunk) { hunk = global_hunk; } //FIXME clean up callers
	return hunk->low_used;
}

VISIBLE void
Hunk_RawFreeToLowMark (memhunk_t *hunk, size_t mark)
{
	if (mark == hunk->low_used)
		return;
	if (mark > hunk->low_used)
		Sys_Error ("Hunk_FreeToLowMark: bad mark %zd", mark);
	hunk->low_used = mark;
}

VISIBLE void
Hunk_FreeToLowMark (memhunk_t *hunk, size_t mark)
{
	if (!hunk) { hunk = global_hunk; } //FIXME clean up callers
	if (mark == hunk->low_used)
		return;
	if (mark > hunk->low_used)
		Sys_Error ("Hunk_FreeToLowMark: bad mark %zd", mark);
	memset (hunk->base + mark, 0, hunk->low_used - mark);
	hunk->low_used = mark;
}

static void *
Hunk_HighAlloc (memhunk_t *hunk, size_t size)
{
	hunkblk_t  *h;

	if (hunk->tempactive) {
		Hunk_FreeToHighMark (hunk, hunk->tempmark);
		hunk->tempactive = false;
	}
#ifdef PARANOID
	Hunk_Check (hunk);
#endif

	size = sizeof (hunkblk_t) + ((size + HUNK_ALIGN - 1) & ~(HUNK_ALIGN - 1));

	if (hunk->size - hunk->low_used - hunk->high_used < size) {
		Sys_Printf ("Hunk_HighAlloc: failed on %zd bytes\n", size);
		return NULL;
	}

	hunk->high_used += size;

	h = (void *) (hunk->base + hunk->size - hunk->high_used);
	h->sentinal1 = HUNK_SENTINAL;
	h->sentinal2 = HUNK_SENTINAL;
	h->size = size;
	h->name[0] = 0;
	return h + 1;
}

/*
	Hunk_TempAlloc

	Return space from the top of the hunk
*/
VISIBLE void *
Hunk_TempAlloc (memhunk_t *hunk, size_t size)
{
	if (!hunk) { hunk = global_hunk; } //FIXME clean up callers
	void       *buf;

	size = (size + HUNK_ALIGN - 1) & ~(HUNK_ALIGN - 1);

	if (hunk->tempactive) {
		size_t      temp_free = hunk->high_used - hunk->tempmark;
		if (temp_free >= size + (int) sizeof (hunkblk_t)) {
			byte       *temp_block = hunk->base + hunk->size - hunk->high_used;
			return (hunkblk_t *) temp_block + 1;
		}
		Hunk_FreeToHighMark (hunk, hunk->tempmark);
		hunk->tempactive = false;
	}

	hunk->tempmark = Hunk_HighMark (hunk);

	buf = Hunk_HighAlloc (hunk, size);

	hunk->tempactive = true;

	return buf;
}

VISIBLE int
Hunk_PointerIsValid (memhunk_t *hunk, void *ptr)
{
	if (!hunk) { hunk = global_hunk; } //FIXME clean up callers

	size_t      offset = (byte *) ptr - hunk->base;
	if (offset >= hunk->size) {
		return 0;
	}
	if (offset < hunk->low_used) {
		// the pointer is somewhere in the lower space of the hunk
		// FIXME better checking?
		return 1;
	}
	if (offset >= hunk->size - hunk->high_used + sizeof (hunkblk_t)) {
		// the pointer is somewhere in the upper space of the hunk
		// FIXME better checking?
		return 1;
	}
	// the pointer is somewhere in between the two marks, so it has probably
	// been freed
	return 0;
}

/* CACHE MEMORY */


static cache_system_t *Cache_TryAlloc (memhunk_t *hunk, size_t size,
									   qboolean nobottom);
#if 0
static void
check_cache (void)
{
	cache_system_t *cs;
	int used = hunk_tempactive ? hunk_tempmark : hunk_high_used;

	for (cs = cache_head.prev; cs != &cache_head; cs = cs->prev)
		if (cs->prev != &cache_head) {
			if ((byte *) cs + cs->size != (byte *) cs->prev)
				Sys_Error ("inconsistent cache %p %p %d %d", cs, cs->prev,
						   (int)cs->size,
						   (int) ((char *)cs->prev - (char *)cs));
			if (hunk_size - ((byte*)cs - hunk_base) > used)
				Sys_Error ("cache block out of high hunk");
		}
	if (cache_head.prev != &cache_head &&
		hunk_size - ((byte*) cache_head.prev - hunk_base) != used)
		Sys_Error ("cache bottom not at bottom of high hunk");
}
#endif
static void
Cache_Move (cache_system_t *c)
{
	memhunk_t  *hunk = c->hunk;
	cache_system_t *new;

	// we are clearing up space at the bottom, so allocate it late
	new = Cache_TryAlloc (hunk, c->size, true);
	if (new) {
		Sys_MaskPrintf (SYS_cache, "cache_move ok\n");

		memcpy (new + 1, c + 1, c->size - sizeof (cache_system_t));
		new->user = c->user;
		memccpy (new->name, c->name, 0, sizeof (new->name));
		Cache_Free (c->user);
		new->user->data = (void *) (new + 1);
	} else {
		Sys_MaskPrintf (SYS_cache, "cache_move failed\n");

		Cache_Free (c->user);			// tough luck...
	}
}

/*
	Cache_FreeLow

	Throw things out until the hunk can be expanded to the given point
*/
static void
Cache_FreeLow (memhunk_t *hunk, int new_low_hunk)
{
	cache_system_t *c;

	while (1) {
		c = cs_ptr (hunk, hunk->cache_head[0].prev);
		if (c == hunk->cache_head)
			return;						// nothing in cache at all
		if ((byte *) c >= hunk->base + new_low_hunk)
			return;						// there is space to grow the hunk
		Sys_Error ("FIXME: Cache_FreeLow: not enough memory");
		Cache_Move (c);					// reclaim the space
	}
}

static inline void
Cache_UnlinkLRU (cache_system_t * cs)
{
	memhunk_t  *hunk = cs->hunk;
	cs_ptr (hunk, cs->lru_next)->lru_prev = cs->lru_prev;
	cs_ptr (hunk, cs->lru_prev)->lru_next = cs->lru_next;

	cs->lru_prev = cs->lru_next = 0;
}

static void
Cache_MakeLRU (cache_system_t * cs)
{
	memhunk_t  *hunk = cs->hunk;
	__auto_type nx = cs_ptr (hunk, hunk->cache_head[0].lru_next);
	nx->lru_prev = cs_ind (hunk, cs);
	cs->lru_next = cs_ind (hunk, nx);
	cs->lru_prev = 0;
	hunk->cache_head[0].lru_next = cs_ind (hunk, cs);
}

static qboolean
Cache_FreeLRU (memhunk_t *hunk)
{
	cache_system_t *cs;

	//check_cache ();
	for (cs = cs_ptr (hunk, hunk->cache_head[0].lru_prev);
		 cs != hunk->cache_head && cs->readlock;
		 cs = cs_ptr (hunk, cs->lru_prev)) {
	}
	if (cs == hunk->cache_head)
		return 0;
	Cache_Free (cs->user);
	return 1;
}

static void
link_cache_system (cache_system_t *new, cache_system_t *cs)
{
	memhunk_t  *hunk = cs->hunk;
	new->next = cs_ind (hunk, cs);
	new->prev = cs->prev;
	cs_ptr (hunk, cs->prev)->next = cs_ind (hunk, new);
	cs->prev = cs_ind (hunk, new);

}

/*
	Cache_TryAlloc

	Looks for a free block of memory between the high and low hunk marks
	Size should already include the header and padding
*/
static cache_system_t *
Cache_TryAlloc (memhunk_t *hunk, size_t size, qboolean nobottom)
{
	cache_system_t *cs, *new;

	//check_cache ();
	// is the cache completely empty?
	if (!nobottom && hunk->cache_head[0].prev == 0) {
		new = (cache_system_t *) Hunk_HighAlloc (hunk, size);
		if (!new)
			return 0;
		new->size = size;
		new->hunk = hunk;
		hunk->cache_head[0].prev = cs_ind (hunk, new);
		hunk->cache_head[0].next = cs_ind (hunk, new);
		new->prev = new->next = 0;
		new->readlock = 0;
		new->name[0] = 0;
		Cache_MakeLRU (new);
		//check_cache ();
		return new;
	}

	// search for space in existing cache
	for (cs = cs_ptr (hunk, hunk->cache_head[0].next);
		 cs != hunk->cache_head;
		 cs = cs_ptr (hunk, cs->next)) {
		if (cs->user)
			continue;					// block isn't free
		if (cs->size >= size) {
			// found a big enough free block. If possible, carve it up for
			// later reuse, using the upper portion of the block for the
			// newly allocated block.
			new = cs;
			if (size - cs->size >= sizeof (cache_system_t)) {
				new = (cache_system_t *) ((char *) cs + cs->size - size);
				new->readlock = 0;
				new->name[0] = 0;
				new->size = size;
				new->hunk = hunk;
				cs->size -= size;
				link_cache_system (new, cs);
				//check_cache ();
			}
			Cache_MakeLRU (new);
			return new;
		}
	}

	if (nobottom)
		return 0;

	// didn't find a free block, so make a new one.
	new = Hunk_HighAlloc (hunk, size);
	if (new) {
		new->readlock = 0;
		new->name[0] = 0;
		new->size = size;
		new->hunk = hunk;
		link_cache_system (new, hunk->cache_head);
		Cache_MakeLRU (new);
		//check_cache ();
		return new;
	}

	return 0;							// couldn't allocate
}

static void
Cache_Profile_r (memhunk_t *hunk)
{
	unsigned int i;
	unsigned int items[31] = {0}, sizes[31] = {0};
	int count = 0, total = 0;
	cache_system_t *cs;

	for (uint32_t ind = hunk->cache_head[0].next; ind; ind = cs->next) {
		cs = cs_ptr (hunk, ind);
		for (i = 0; (cs->size >> (i + 1)) && i < 30; i++) {
		}
		items[i]++;
		sizes[i] += cs->size;
		total += cs->size;
		count++;
		ind = cs->next;
	}
	Sys_Printf ("Cache Profile:\n");
	Sys_Printf ("%8s  %8s  %8s  %8s  %8s\n",
				"count", "min", "max", "average", "percent");
	for (i = 0; i < 31; i++) {
		if (!items[i])
			continue;
		Sys_Printf ("%8d  %8d  %8d  %8d  %7d%%\n",
					items[i], 1 << i, (1 << (i + 1)) - 1,
					sizes[i] / items[i],
					(sizes[i] * 100) / total);
	}
	Sys_Printf ("Total allocations: %d in %d allocations, average of"
				" %d per allocation\n", total, count,
				count ? total / count : -1);
}

static void
Cache_Profile (void)
{
	Cache_Profile_r (global_hunk);
}

static void
Cache_Print_r (memhunk_t *hunk)
{
	cache_system_t *cs;
	for (uint32_t ind = hunk->cache_head[0].next; ind; ind = cs->next) {
		const int   sz = sizeof (cs->name);
		cs = cs_ptr (hunk, ind);
		Sys_Printf ("%8zd : %.*s\n", cs->size, sz, cs->name);
	}
}

static void
Cache_Print (void)
{
	Cache_Print_r (global_hunk);
}

static void
init_cache (memhunk_t *hunk)
{
	hunk->cache_head[0].hunk = hunk;
	hunk->cache_head[0].size = 0;
	hunk->cache_head[0].next = hunk->cache_head[0].prev = 0;
	hunk->cache_head[0].lru_next = hunk->cache_head[0].lru_prev = 0;
	hunk->cache_head[0].user = (cache_user_t *) 1;	// make it look allocated
	hunk->cache_head[0].readlock = 1; // don't try to free or move it
}

static void
Cache_Init (void)
{
	Cmd_AddCommand ("cache_flush", Cache_Flush, "Clears the current game "
					"cache");
	Cmd_AddCommand ("cache_profile", Cache_Profile, "Prints a profile of "
					"the current cache");
	Cmd_AddCommand ("cache_print", Cache_Print, "Prints out items in the "
					"cache");
}

/*
	Cache_Flush

	Throw everything out, so new data will be demand cached
*/
static void
Cache_Flush_r (memhunk_t *hunk)
{
	// cache_head.prev is guaranteed to not be free because it's the bottom
	// one and Cache_Free actually properly releases it
	while (hunk->cache_head[0].prev) {
		__auto_type cs = cs_ptr (hunk, hunk->cache_head[0].prev);
		if (!cs->user->data) {
			const int   sz = sizeof (cs->name);
			Sys_Error ("Cache_Flush: user/system out of sync for "
					   "'%.*s' with %zd size",
					   sz, cs->name, cs->size);
		}
		Cache_Free (cs->user);	// reclaim the space
	}
}

VISIBLE void
Cache_Flush (void)
{
	// cache_head.prev is guaranteed to not be free because it's the bottom
	Cache_Flush_r (global_hunk);
}

VISIBLE void *
Cache_Check (cache_user_t *c)
{
	cache_system_t *cs;

	if (!c->data)
		return NULL;

	cs = ((cache_system_t *) c->data) - 1;

	// move to head of LRU
	Cache_UnlinkLRU (cs);
	Cache_MakeLRU (cs);

	return c->data;
}

/*
	Cache_Free

	Frees the memory and removes it from the LRU list
*/
VISIBLE void
Cache_Free (cache_user_t *c)
{
	cache_system_t *cs;

	if (!c->data)
		Sys_Error ("Cache_Free: not allocated");

	cs = ((cache_system_t *) c->data) - 1;

	if (cs->readlock)
		Sys_Error ("Cache_Free: attempt to free locked block");

	const int   sz = sizeof (cs->name);
	Sys_MaskPrintf (SYS_cache, "Cache_Free: freeing '%.*s' %p\n",
					sz, cs->name, cs);

	Cache_UnlinkLRU (cs);

	memhunk_t  *h = cs->hunk;
	//check_cache ();
	cs->user = 0;
	if (!cs_ptr (h, cs->prev)->user) {
		cs->size += cs_ptr (h, cs->prev)->size;
		cs_ptr (h, cs_ptr (h, cs->prev)->prev)->next = cs_ind (h, cs);
		cs->prev = cs_ptr (h, cs->prev)->prev;
	}
	if (!cs_ptr (h, cs->next)->user) {
		cs = cs_ptr (h, cs->next);
		cs->size += cs_ptr (h, cs->prev)->size;
		cs_ptr (h, cs_ptr (h, cs->prev)->prev)->next = cs_ind (h, cs);
		cs->prev = cs_ptr (h, cs->prev)->prev;
	}
	if (!cs->next) {
		cs_ptr (h, cs->next)->prev = cs->prev;
		cs_ptr (h, cs->prev)->next = cs->next;
		if (cs->prev) {
			__auto_type ptr = (byte *) cs_ptr (h, cs->prev);
			Hunk_FreeToHighMark (h, h->size - (ptr - h->base));
		} else {
			Hunk_FreeToHighMark (h, 0);
		}
	}
	//check_cache ();

	c->data = NULL;
}

static void *
Cache_Alloc_r (memhunk_t *hunk, cache_user_t *c, size_t size, const char *name)
{
	cache_system_t *cs;

	if (c->data)
		Sys_Error ("Cache_Alloc_r: already allocated");

	if (size <= 0)
		Sys_Error ("Cache_Alloc_r: size %zd", size);

	size = (size + sizeof (cache_system_t) + HUNK_ALIGN - 1) & ~(HUNK_ALIGN-1);

	// find memory for it
	while (1) {
		cs = Cache_TryAlloc (hunk, size, false);
		if (cs) {
			memccpy (cs->name, name, 0, sizeof (cs->name));
			c->data = (void *) (cs + 1);
			cs->user = c;
			break;
		}
		// free the least recently used cachedat
		if (!Cache_FreeLRU (hunk))
			Sys_Error ("Cache_Alloc: out of memory");
	}

	return Cache_Check (c);
}

VISIBLE void *
Cache_Alloc (cache_user_t *c, size_t size, const char *name)
{
	return Cache_Alloc_r (global_hunk, c, size, name);
}

static void
Cache_Report_r (memhunk_t *hunk)
{
	Sys_Printf ("%4.1f megabyte data cache\n",
				(hunk->size - hunk->high_used -
				 hunk->low_used) / (float) (1024 * 1024));
}

VISIBLE void
Cache_Report (void)
{
	if (developer & SYS_cache) {
		Cache_Report_r (global_hunk);
	}
}

VISIBLE void
Cache_Add (cache_user_t *c, void *object, cache_loader_t loader)
{
	if (c->data || c->object || c->loader)
		Sys_Error ("Cache_Add: cache item already exists!");

	c->object = object;
	c->loader = loader;

//	c->loader (c, Cache_Alloc); // for debugging
}

VISIBLE void
Cache_Remove (cache_user_t *c)
{
	if (!c->object || !c->loader)
		Sys_Error ("Cache_Remove: already removed!");

	if (Cache_Check (c))
		Cache_Free (c);

	c->object = 0;
	c->loader = 0;
}

VISIBLE void *
Cache_TryGet (cache_user_t *c)
{
	void *mem;

	mem = Cache_Check (c);
	if (!mem) {
		c->loader (c->object, Cache_Alloc);
		mem = Cache_Check (c);
	}
	if (mem)
		(((cache_system_t *)c->data) - 1)->readlock++;

	return mem;
}

VISIBLE void *
Cache_Get (cache_user_t *c)
{
	void *mem = Cache_TryGet (c);

	if (!mem)
		Sys_Error ("Cache_Get: couldn't get cache!");
	return mem;
}

VISIBLE void
Cache_Release (cache_user_t *c)
{
	int *readlock;

	readlock = &(((cache_system_t *)c->data) - 1)->readlock;

	if (!*readlock)
		Sys_Error ("Cache_Release: already released!");

	(*readlock)--;

//	if (!*readlock)
//		Cache_Free (c); // for debugging
}

VISIBLE int
Cache_ReadLock (cache_user_t *c)
{
	return (((cache_system_t *)c->data) - 1)->readlock;
}


//============================================================================

VISIBLE memhunk_t *
Hunk_Init (void *buf, size_t size)
{
	memhunk_t  *hunk = buf;
	hunk->base = (byte *) (hunk + 1);
	hunk->size = size - sizeof (memhunk_t);
	hunk->low_used = 0;
	hunk->high_used = 0;

	init_cache (hunk);
	return hunk;
}

VISIBLE memhunk_t *
Memory_Init (void *buf, size_t size)
{
	global_hunk = Hunk_Init (buf, size);
	Cache_Init ();
	return global_hunk;
}
