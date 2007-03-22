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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdarg.h>
#include <stdlib.h>

#undef MMAPPED_CACHE
#ifdef MMAPPED_CACHE
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
# include <fcntl.h>
# include <sys/mman.h>
# include <sys/types.h>
# include <sys/stat.h>
# ifndef _POSIX_MAPPED_FILES
#  error No _POSIX_MAPPED_FILES?  erk!
# endif
#endif

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "compat.h"

#define	ZONEID			0x1d4a11
#define	HUNK_SENTINAL	0x1df001ed

#define MINFRAGMENT	64

/*
   						ZONE MEMORY ALLOCATION

	There is never any space between memblocks, and there will never be two
	contiguous free memblocks.

	The rover can be left pointing at a non-empty block

	The zone calls are pretty much only used for small strings and structures,
	all big things are allocated on the hunk.
*/

typedef struct memblock_s
{
	int         size;		// including the header and possibly tiny fragments
	int         tag;		// a tag of 0 is a free block
	int         id;			// should be ZONEID
	struct memblock_s *next, *prev;
	int         pad;		// pad to 64 bit boundary
} memblock_t;

struct memzone_s
{
	int         size;		// total bytes malloced, including header
	int         used;		// ammount used, including header
	memblock_t  blocklist;	// start / end cap for linked list
	memblock_t  *rover;
};


VISIBLE void
Z_ClearZone (memzone_t *zone, int size)
{
	memblock_t	*block;
	
	// set the entire zone to one free block

	block = (memblock_t *) (zone + 1);
	zone->blocklist.next = block;
	zone->blocklist.prev = block;
	zone->blocklist.tag = 1;	// in use block
	zone->blocklist.id = 0;
	zone->blocklist.size = 0;
	zone->rover = block;
	zone->size = size;
	zone->used = sizeof (memzone_t);
	
	block->prev = block->next = &zone->blocklist;
	block->tag = 0;			// free block
	block->id = ZONEID;
	block->size = size - sizeof (memzone_t);
}

VISIBLE void
Z_Free (memzone_t *zone, void *ptr)
{
	memblock_t	*block, *other;
	
	if (!ptr)
		Sys_Error ("Z_Free: NULL pointer");

	block = (memblock_t *) ((byte *) ptr - sizeof (memblock_t));
	if (block->id != ZONEID)
		Sys_Error ("Z_Free: freed a pointer without ZONEID");
	if (block->tag == 0)
		Sys_Error ("Z_Free: freed a freed pointer");

	block->tag = 0;		// mark as free
	zone->used -= block->size;
	
	other = block->prev;
	if (!other->tag) {
		// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if (block == zone->rover)
			zone->rover = other;
		block = other;
	}
	
	other = block->next;
	if (!other->tag) {
		// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if (other == zone->rover)
			zone->rover = block;
	}
}

VISIBLE void *
Z_Malloc (memzone_t *zone, int size)
{
	void	*buf;

	if (!developer || developer->int_val)
		Z_CheckHeap (zone);	// DEBUG
	buf = Z_TagMalloc (zone, size, 1);
	if (!buf)
		Sys_Error ("Z_Malloc: failed on allocation of %i bytes",size);
	memset (buf, 0, size);

	return buf;
}

void *
Z_TagMalloc (memzone_t *zone, int size, int tag)
{
	int			 extra;
	memblock_t	*start, *rover, *new, *base;

	if (!tag)
		Sys_Error ("Z_TagMalloc: tried to use a 0 tag");

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
	} while (base->tag || base->size < size);
	
	// found a block big enough
	extra = base->size - size;
	if (extra >  MINFRAGMENT) {
		// there will be a free fragment after the allocated block
		new = (memblock_t *) ((byte *) base + size);
		new->size = extra;
		new->tag = 0;			// free block
		new->prev = base;
		new->id = ZONEID;
		new->next = base->next;
		new->next->prev = new;
		base->next = new;
		base->size = size;
	}
	
	base->tag = tag;				// no longer a free block
	
	zone->rover = base->next;	// next allocation will start looking here
	
	base->id = ZONEID;

	zone->used += base->size;

	// marker for memory trash testing
	*(int *) ((byte *) base + base->size - 4) = ZONEID;

	return (void *) (base + 1);
}

VISIBLE void *
Z_Realloc (memzone_t *zone, void *ptr, int size)
{
	int         old_size;
	memblock_t *block;
	void       *old_ptr;
	
	if (!ptr)
		return Z_Malloc (zone, size);

	block = (memblock_t *) ((byte *) ptr - sizeof (memblock_t));
	if (block->id != ZONEID)
		Sys_Error ("Z_Realloc: realloced a pointer without ZONEID");
	if (block->tag == 0)
		Sys_Error ("Z_Realloc: realloced a freed pointer");

	old_size = block->size;
	old_ptr = ptr;

	Z_Free (zone, ptr);
	ptr = Z_TagMalloc (zone, size, 1);
	if (!ptr)
		Sys_Error ("Z_Realloc: failed on allocation of %i bytes", size);

	if (ptr != old_ptr)
		memmove (ptr, old_ptr, min (old_size, size));

	return ptr;
}

void
Z_Print (memzone_t *zone)
{
	memblock_t	*block;

	Sys_Printf ("zone size: %i  location: %p  used: %i\n",
				zone->size, zone, zone->used);

	for (block = zone->blocklist.next ; ; block = block->next) {
		Sys_Printf ("block:%p    size:%7i    tag:%3i ofs:%d\n",
					block, block->size, block->tag,
					(int) ((byte *) block - (byte *) zone));

		if (block->next == &zone->blocklist)
			break;			// all blocks have been hit	
		if ( (byte *)block + block->size != (byte *)block->next)
			Sys_Printf ("ERROR: block size does not touch the next block\n");
		if ( block->next->prev != block)
			Sys_Printf ("ERROR: next block doesn't have proper back link\n");
		if (!block->tag && !block->next->tag)
			Sys_Printf ("ERROR: two consecutive free blocks\n");
		if (block->tag
			&& (*(int *) ((byte *) block + block->size - 4) != ZONEID))
			Sys_Printf ("ERROR: memory trashed in block\n");
		fflush (stdout);
	}
}

void
Z_CheckHeap (memzone_t *zone)
{
	memblock_t	*block;

	for (block = zone->blocklist.next ; ; block = block->next) {
		if (block->next == &zone->blocklist)
			break;			// all blocks have been hit	
		if ( (byte *)block + block->size != (byte *)block->next)
			Sys_Error ("Z_CheckHeap: block size does not touch the next "
					   "block\n");
		if ( block->next->prev != block)
			Sys_Error ("Z_CheckHeap: next block doesn't have proper back "
					   "link\n");
		if (!block->tag && !block->next->tag)
			Sys_Error ("Z_CheckHeap: two consecutive free blocks\n");
	}
}

//============================================================================

typedef struct {
	int         sentinal;
	int         size;			// including sizeof(hunk_t), -1 = not allocated
	char        name[8];
} hunk_t;

byte       *hunk_base;
int         hunk_size;
int         hunk_low_used;
int         hunk_high_used;
int         hunk_tempmark;

qboolean    hunk_tempactive;

/*
	Hunk_Check

	Run consistancy and sentinal trahing checks
*/
VISIBLE void
Hunk_Check (void)
{
	hunk_t     *h;

	for (h = (hunk_t *) hunk_base; (byte *) h != hunk_base + hunk_low_used;) {
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error ("Hunk_Check: trashed sentinal");
		if (h->size < 16 || h->size + (byte *) h - hunk_base > hunk_size)
			Sys_Error ("Hunk_Check: bad size");
		h = (hunk_t *) ((byte *) h + h->size);
	}
}

/*
	Hunk_Print

	If "all" is specified, every single allocation is printed.
	Otherwise, allocations with the same name will be totaled up before
	printing.
*/
/*
static void
Hunk_Print (qboolean all)
{
	char        name[9];
	hunk_t     *h, *next, *endlow, *starthigh, *endhigh;
	int         count, sum, totalblocks;

	name[8] = 0;
	count = 0;
	sum = 0;
	totalblocks = 0;

	h = (hunk_t *) hunk_base;
	endlow = (hunk_t *) (hunk_base + hunk_low_used);
	starthigh = (hunk_t *) (hunk_base + hunk_size - hunk_high_used);
	endhigh = (hunk_t *) (hunk_base + hunk_size);

	Sys_Printf ("          :%8i total hunk size\n", hunk_size);
	Sys_Printf ("-------------------------\n");

	while (1) {
		// skip to the high hunk if done with low hunk
		if (h == endlow) {
			Sys_Printf ("-------------------------\n");
			Sys_Printf ("          :%8i REMAINING\n",
						hunk_size - hunk_low_used - hunk_high_used);
			Sys_Printf ("-------------------------\n");
			h = starthigh;
		}
		// if totally done, break
		if (h == endhigh)
			break;

		// run consistancy checks
		if (h->sentinal != HUNK_SENTINAL)
			Sys_Error ("Hunk_Check: trahsed sentinal");
		if (h->size < 16 || h->size + (byte *) h - hunk_base > hunk_size)
			Sys_Error ("Hunk_Check: bad size");

		next = (hunk_t *) ((byte *) h + h->size);
		count++;
		totalblocks++;
		sum += h->size;

		// print the single block
		memcpy (name, h->name, 8);
		if (all)
			Sys_Printf ("%8p :%8i %8s\n", h, h->size, name);

		// print the total
		if (next == endlow || next == endhigh ||
			strncmp (h->name, next->name, 8)) {
			if (!all)
				Sys_Printf ("          :%8i %8s (TOTAL)\n", sum, name);
			count = 0;
			sum = 0;
		}

		h = next;
	}

	Sys_Printf ("-------------------------\n");
	Sys_Printf ("%8i total blocks\n", totalblocks);
}
*/
VISIBLE void *
Hunk_AllocName (int size, const char *name)
{
	hunk_t     *h;

#ifdef PARANOID
	Hunk_Check ();
#endif

	if (size < 0)
		Sys_Error ("Hunk_Alloc: bad size: %i", size);

	size = sizeof (hunk_t) + ((size + 15) & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size)
		Sys_Error
			("Not enough RAM allocated.  Try starting using \"-mem 16\" on "
			 "the %s command line. (%d -  %d - %d < %d)",
			 PROGRAM, hunk_size, hunk_low_used, hunk_high_used, size);

	h = (hunk_t *) (hunk_base + hunk_low_used);
	hunk_low_used += size;

	Cache_FreeLow (hunk_low_used);

	memset (h, 0, size);

	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	strncpy (h->name, name, 8);

	return (void *) (h + 1);
}

VISIBLE void *
Hunk_Alloc (int size)
{
	return Hunk_AllocName (size, "unknown");
}

VISIBLE int
Hunk_LowMark (void)
{
	return hunk_low_used;
}

VISIBLE void
Hunk_FreeToLowMark (int mark)
{
	if (mark < 0 || mark > hunk_low_used)
		Sys_Error ("Hunk_FreeToLowMark: bad mark %i", mark);
	memset (hunk_base + mark, 0, hunk_low_used - mark);
	hunk_low_used = mark;
}

VISIBLE int
Hunk_HighMark (void)
{
	if (hunk_tempactive) {
		hunk_tempactive = false;
		Hunk_FreeToHighMark (hunk_tempmark);
	}

	return hunk_high_used;
}

VISIBLE void
Hunk_FreeToHighMark (int mark)
{
	if (hunk_tempactive) {
		hunk_tempactive = false;
		Hunk_FreeToHighMark (hunk_tempmark);
	}
	if (mark < 0 || mark > hunk_high_used)
		Sys_Error ("Hunk_FreeToHighMark: bad mark %i", mark);
	memset (hunk_base + hunk_size - hunk_high_used, 0, hunk_high_used - mark);
	hunk_high_used = mark;
}

VISIBLE void *
Hunk_HighAllocName (int size, const char *name)
{
	hunk_t     *h;

	if (size < 0)
		Sys_Error ("Hunk_HighAllocName: bad size: %i", size);

	if (hunk_tempactive) {
		Hunk_FreeToHighMark (hunk_tempmark);
		hunk_tempactive = false;
	}
#ifdef PARANOID
	Hunk_Check ();
#endif

	size = sizeof (hunk_t) + ((size + 15) & ~15);

	if (hunk_size - hunk_low_used - hunk_high_used < size) {
		Sys_Printf ("Hunk_HighAlloc: failed on %i bytes\n", size);
		return NULL;
	}

	hunk_high_used += size;
	Cache_FreeHigh (hunk_high_used);

	h = (hunk_t *) (hunk_base + hunk_size - hunk_high_used);

	h->size = size;
	h->sentinal = HUNK_SENTINAL;
	strncpy (h->name, name, 8);

	return (void *) (h + 1);
}

/*
	Hunk_TempAlloc

	Return space from the top of the hunk
*/
VISIBLE void *
Hunk_TempAlloc (int size)
{
	void       *buf;

	size = (size + 15) & ~15;

	if (hunk_tempactive) {
		if (hunk_high_used - hunk_tempmark >= size + (int) sizeof (hunk_t)) {
			return (hunk_t *) (hunk_base + hunk_size - hunk_high_used) + 1;
		}
		Hunk_FreeToHighMark (hunk_tempmark);
		hunk_tempactive = false;
	}

	hunk_tempmark = Hunk_HighMark ();

	buf = Hunk_HighAllocName (size, "temp");

	hunk_tempactive = true;

	return buf;
}

/* CACHE MEMORY */

typedef struct cache_system_s {
	cache_user_t *user;
	char        name[16];
	int         size;							// including this header
	int			readlock;
	struct cache_system_s *prev, *next;
	struct cache_system_s *lru_prev, *lru_next;	// for LRU flushing 
} cache_system_t;

cache_system_t cache_head;
int cache_writelock;

static cache_system_t *Cache_TryAlloc (int size, qboolean nobottom);
static void Cache_RealFree (cache_user_t *c);
static void Cache_Profile (void);
static void *Cache_RealAlloc (cache_user_t *c, int size, const char *name);

#define CACHE_WRITE_LOCK	{ if (cache_writelock) \
								Sys_Error ("Cache double-locked!"); \
							  else \
								cache_writelock++; }
#define CACHE_WRITE_UNLOCK	{ if (!cache_writelock) \
								Sys_Error ("Cache already unlocked!"); \
							  else \
								cache_writelock--; }

#ifndef MMAPPED_CACHE
static void
Cache_Move (cache_system_t * c)
{
	cache_system_t *new;

	// we are clearing up space at the bottom, so only allocate it late
	new = Cache_TryAlloc (c->size, true);
	if (new) {
		Sys_DPrintf ("cache_move ok\n");

		memcpy (new + 1, c + 1, c->size - sizeof (cache_system_t));
		new->user = c->user;
		memcpy (new->name, c->name, sizeof (new->name));
		Cache_RealFree (c->user);
		new->user->data = (void *) (new + 1);
	} else {
		Sys_DPrintf ("cache_move failed\n");

		Cache_RealFree (c->user);			// tough luck...
	}
}
#endif

/*
	Cache_FreeLow

	Throw things out until the hunk can be expanded to the given point
*/
void
Cache_FreeLow (int new_low_hunk)
{
#ifndef MMAPPED_CACHE
	cache_system_t *c;

	while (1) {
		c = cache_head.next;
		if (c == &cache_head)
			return;						// nothing in cache at all
		if ((byte *) c >= hunk_base + new_low_hunk)
			return;						// there is space to grow the hunk
		Cache_Move (c);					// reclaim the space
	}
#endif
}

/*
	Cache_FreeHigh

	Throw things out until the hunk can be expanded to the given point
*/
void
Cache_FreeHigh (int new_high_hunk)
{
#ifndef MMAPPED_CACHE
	cache_system_t *c, *prev;

	prev = NULL;
	while (1) {
		c = cache_head.prev;
		if (c == &cache_head)
			return;						// nothing in cache at all
		if ((byte *) c + c->size <= hunk_base + hunk_size - new_high_hunk)
			return;						// there is space to grow the hunk
		if (c == prev)
			Cache_RealFree (c->user);	// didn't move out of the way
		else {
			Cache_Move (c);				// try to move it
			prev = c;
		}
	}
#endif
}

static inline void
Cache_UnlinkLRU (cache_system_t * cs)
{
	if (!cs->lru_next || !cs->lru_prev)
		Sys_Error ("Cache_UnlinkLRU: NULL link");

	cs->lru_next->lru_prev = cs->lru_prev;
	cs->lru_prev->lru_next = cs->lru_next;

	cs->lru_prev = cs->lru_next = NULL;
}

static inline void
Cache_MakeLRU (cache_system_t * cs)
{
	if (cs->lru_next || cs->lru_prev)
		Sys_Error ("Cache_MakeLRU: active link");

	cache_head.lru_next->lru_prev = cs;
	cs->lru_next = cache_head.lru_next;
	cs->lru_prev = &cache_head;
	cache_head.lru_next = cs;
}

static qboolean
Cache_FreeLRU (void)
{
	cache_system_t *cs;

	for (cs = cache_head.lru_prev; cs->readlock; cs = cs->lru_prev)
		;
	if (cs == &cache_head)
		return 0;
	Cache_RealFree (cs->user);
	return 1;
}

/*
	Cache_TryAlloc

	Looks for a free block of memory between the high and low hunk marks
	Size should already include the header and padding
*/
static cache_system_t *
Cache_TryAlloc (int size, qboolean nobottom)
{
#ifndef MMAPPED_CACHE
	cache_system_t *cs, *new;

	// is the cache completely empty?

	if (!nobottom && cache_head.prev == &cache_head) {
		if (hunk_size - hunk_high_used - hunk_low_used < size) {
			Sys_Printf ("Cache_TryAlloc: %i is greater then free hunk", size);
			return NULL;
		}

		new = (cache_system_t *) (hunk_base + hunk_low_used);
		memset (new, 0, sizeof (*new));
		new->size = size;

		cache_head.prev = cache_head.next = new;
		new->prev = new->next = &cache_head;

		Cache_MakeLRU (new);
		return new;
	}

	// search from the bottom up for space

	new = (cache_system_t *) (hunk_base + hunk_low_used);
	cs = cache_head.next;

	do {
		if (!nobottom || cs != cache_head.next) {
			if ((byte *) cs - (byte *) new >= size) {	// found space
				memset (new, 0, sizeof (*new));
				new->size = size;

				new->next = cs;
				new->prev = cs->prev;
				cs->prev->next = new;
				cs->prev = new;

				Cache_MakeLRU (new);

				return new;
			}
		}
		// continue looking     
		new = (cache_system_t *) ((byte *) cs + cs->size);
		cs = cs->next;

	} while (cs != &cache_head);

	// try to allocate one at the very end
	if (hunk_base + hunk_size - hunk_high_used - (byte *) new >= size) {
		memset (new, 0, sizeof (*new));
		new->size = size;

		new->next = &cache_head;
		new->prev = cache_head.prev;
		cache_head.prev->next = new;
		cache_head.prev = new;

		Cache_MakeLRU (new);

		return new;
	}

	return NULL;						// couldn't allocate
#else
	cache_system_t *new;
	int fd;

	fd = open ("/dev/zero", O_RDWR);
	if (fd < 0)
		return NULL;
	new = mmap (0, size, PROT_READ | PROT_WRITE,
				MAP_PRIVATE, fd, 0);
	close (fd);
	if (new == MAP_FAILED)
		return NULL;

	new->size = size;
	new->next = &cache_head;
	new->prev = cache_head.prev;
	cache_head.prev->next = new;
	cache_head.prev = new;

	Cache_MakeLRU (new);

	return new;
#endif
}

/*
	Cache_Flush

	Throw everything out, so new data will be demand cached
*/
void
Cache_Flush (void)
{
	CACHE_WRITE_LOCK;
	while (cache_head.next != &cache_head) {
		if (!cache_head.next->user->data)
			Sys_Error ("Cache_Flush: user/system out of sync for "
					   "'%s' with %d size",
					   cache_head.next->name, cache_head.next->size);
		Cache_RealFree (cache_head.next->user);	// reclaim the space
	}
	CACHE_WRITE_UNLOCK;
}

static void
Cache_Print (void)
{
	cache_system_t *cd;

	CACHE_WRITE_LOCK;
	for (cd = cache_head.next; cd != &cache_head; cd = cd->next) {
		Sys_Printf ("%8i : %s\n", cd->size, cd->name);
	}
	CACHE_WRITE_UNLOCK;
}

VISIBLE void
Cache_Report (void)
{
	CACHE_WRITE_LOCK;
	Sys_DPrintf ("%4.1f megabyte data cache\n",
				 (hunk_size - hunk_high_used -
				  hunk_low_used) / (float) (1024 * 1024));
	CACHE_WRITE_UNLOCK;
}

static void
Cache_Init (void)
{
	cache_head.next = cache_head.prev = &cache_head;
	cache_head.lru_next = cache_head.lru_prev = &cache_head;

	Cmd_AddCommand ("cache_flush", Cache_Flush, "Clears the current game "
					"cache");
	Cmd_AddCommand ("cache_profile", Cache_Profile, "Prints a profile of "
					"the current cache");
	Cmd_AddCommand ("cache_print", Cache_Print, "Prints out items in the "
					"cache");
}

/*
	Cache_Free

	Frees the memory and removes it from the LRU list
*/
VISIBLE void
Cache_Free (cache_user_t *c)
{
	CACHE_WRITE_LOCK;
	Cache_RealFree (c);
	CACHE_WRITE_UNLOCK;
}

static void
Cache_RealFree (cache_user_t *c)
{
	cache_system_t *cs;

	if (!c->data)
		Sys_Error ("Cache_Free: not allocated");

	cs = ((cache_system_t *) c->data) - 1;

	Sys_DPrintf ("Cache_Free: freeing '%s'\n", cs->name);

	cs->prev->next = cs->next;
	cs->next->prev = cs->prev;
	cs->next = cs->prev = NULL;

	c->data = NULL;

	Cache_UnlinkLRU (cs);

#ifdef MMAPPED_CACHE
	if (munmap (cs, cs->size))
		Sys_Error ("Cache_Free: munmap failed!");
#endif
}

static inline void *
Cache_RealCheck (cache_user_t *c)
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

VISIBLE void *
Cache_Check (cache_user_t *c)
{
	void *mem;

	CACHE_WRITE_LOCK;
	mem = Cache_RealCheck (c);
	CACHE_WRITE_UNLOCK;
	return mem;
}

VISIBLE void *
Cache_Alloc (cache_user_t *c, int size, const char *name)
{
	void *mem;

	CACHE_WRITE_LOCK;
	mem = Cache_RealAlloc (c, size, name);
	CACHE_WRITE_UNLOCK;
	return mem;
}

static void *
Cache_RealAlloc (cache_user_t *c, int size, const char *name)
{
	cache_system_t *cs;

	if (c->data)
		Sys_Error ("Cache_Alloc: already allocated");

	if (size <= 0)
		Sys_Error ("Cache_Alloc: size %i", size);

	size = (size + sizeof (cache_system_t) + 15) & ~15;

	// find memory for it   
	while (1) {
		cs = Cache_TryAlloc (size, false);
		if (cs) {
			strncpy (cs->name, name, sizeof (cs->name) - 1);
			c->data = (void *) (cs + 1);
			cs->user = c;
			break;
		}
		// free the least recently used cachedat
		if (!Cache_FreeLRU())
			Sys_Error ("Cache_Alloc: out of memory");
	}

	return Cache_RealCheck (c);
}

static void
Cache_Profile (void)
{
	cache_system_t *cs;
	unsigned int i;
	unsigned int items[31] = {0}, sizes[31] = {0};
	int count = 0, total = 0;

	CACHE_WRITE_LOCK;

	cs = cache_head.next;
	while (cs != &cache_head) {
		for (i = 0; (cs->size >> (i + 1)) && i < 30; i++)
			;
		items[i]++;
		sizes[i] += cs->size;
		total += cs->size;
		count++;
		cs = cs->next;
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
				" %d per allocation\n", total, count, total / count);

	CACHE_WRITE_UNLOCK;
}

VISIBLE void
Cache_Add (cache_user_t *c, void *object, cache_loader_t loader)
{
	CACHE_WRITE_LOCK;

	if (c->data || c->object || c->loader)
		Sys_Error ("Cache_Add: cache item already exists!");

	c->object = object;
	c->loader = loader;

//	c->loader (c, Cache_RealAlloc); // for debugging

	CACHE_WRITE_UNLOCK;
}

void
Cache_Remove (cache_user_t *c)
{
	CACHE_WRITE_LOCK;

	if (!c->object || !c->loader)
		Sys_Error ("Cache_Remove: already removed!");

	if (Cache_RealCheck (c))
		Cache_RealFree (c);

	c->object = 0;
	c->loader = 0;

	CACHE_WRITE_UNLOCK;
}

VISIBLE void *
Cache_TryGet (cache_user_t *c)
{
	void *mem;

	CACHE_WRITE_LOCK;

	mem = Cache_RealCheck (c);
	if (!mem) {
		c->loader (c->object, Cache_RealAlloc);
		mem = Cache_RealCheck (c);
	}
	if (mem)
		(((cache_system_t *)c->data) - 1)->readlock++;

	CACHE_WRITE_UNLOCK;
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

	CACHE_WRITE_LOCK;
	readlock = &(((cache_system_t *)c->data) - 1)->readlock;

	if (!*readlock)
		Sys_Error ("Cache_Release: already released!");

	(*readlock)--;

//	if (!*readlock)
//		Cache_RealFree (c); // for debugging
	CACHE_WRITE_UNLOCK;
}

// QA_alloc and friends =======================================================

size_t (*QA_alloc_callback) (size_t size);

void *
QA_alloc (unsigned flags, ...)
{
	int failure = QA_NOFAIL;
	size_t size = 0;
	qboolean zeroed = false;
	va_list ap;
	void *mem;
	void *ptr = 0;

	if (flags & ~(QA_FAILURE | QA_PREVIOUS | QA_SIZE | QA_ZEROED))
		Sys_Error ("QA_alloc: bad flags: %u", flags);

	va_start (ap, flags);
	if (flags & QA_PREVIOUS)
		ptr = va_arg (ap, void *);
	if (flags & QA_SIZE)
		size = va_arg (ap, size_t);
	if (flags & QA_ZEROED)
		zeroed = true;
	if (flags & QA_FAILURE)
		failure = va_arg (ap, int);
	va_end (ap);

	if (failure != QA_NOFAIL && failure != QA_LATEFAIL && failure
		!= QA_EARLYFAIL)
		Sys_Error ("QA_alloc: invalid failure type: %u", failure);

	if (size) {
		do {
			if (ptr) {
				if (zeroed)
					Sys_Error ("QA_alloc: Zeroing reallocated memory not yet "
							   "supported");
				else
					mem = realloc (ptr, size);
			} else {
				if (zeroed)
					mem = calloc (size, 1);
				else
					mem = malloc (size);
			}
		} while (failure != QA_EARLYFAIL && !mem
				 && QA_alloc_callback && QA_alloc_callback (size));

		if (!mem && failure == QA_NOFAIL)
			Sys_Error ("QA_alloc: could not allocate %d bytes!", (int)size);

		return mem;
	} else {
		if (!ptr)
			Sys_Error ("QA_alloc: can't free a NULL pointers!");
		free (ptr);
		return 0;
	}
}

void *
QA_malloc (size_t size)
{
	return QA_alloc (QA_SIZE, size);
}

void *
QA_calloc (size_t nmemb, size_t size)
{
	return QA_alloc (QA_ZEROED | QA_SIZE, nmemb * size);
}

void *
QA_realloc (void *ptr, size_t size)
{
	return QA_alloc (QA_PREVIOUS | QA_SIZE, ptr, size);
}

void 
QA_free (void *ptr)
{
	QA_alloc (QA_PREVIOUS, ptr);
}

char *
QA_strdup (const char *s)
{
	char *mem;

	mem = QA_malloc (strlen (s) + 1);
	strcpy (mem, s);
	return mem;
}

//============================================================================

VISIBLE void
Memory_Init (void *buf, int size)
{
	hunk_base = buf;
	hunk_size = size;
	hunk_low_used = 0;
	hunk_high_used = 0;

	Cache_Init ();
}
