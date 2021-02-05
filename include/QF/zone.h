/*
	zone.h

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
#ifndef __QF_zone_h
#define __QF_zone_h

/** \defgroup zone Memory Management
	\ingroup utils

	H_??? The hunk manages the entire memory block given to quake.  It must be
	contiguous.  Memory can be allocated from either the low or high end in a
	stack fashion.  The only way memory is released is by resetting one of the
	pointers.

	Hunk allocations should be given a name, so the Hunk_Print () function
	can display usage.

	Hunk allocations are guaranteed to be 16 byte aligned.

	The video buffers are allocated high to avoid leaving a hole underneath
	server allocations when changing to a higher video mode.


	Z_??? Zone memory functions used for small, dynamic allocations like text
	strings from command input.  There is only about 48K for it, allocated at
	the very bottom of the hunk.

	Cache_??? Cache memory is for objects that can be dynamically loaded and
	can usefully stay persistant between levels.  The size of the cache
	fluctuates from level to level.

	To allocate a cachable object


	Temp_??? Temp memory is used for file loading and surface caching.  The
	size of the cache memory is adjusted so that there is a minimum of 512k
	remaining for temp memory.


	------ Top of Memory -------

	high hunk allocations

	<--- high hunk reset point held by vid

	video buffer

	z buffer

	surface cache

	<--- high hunk used

	cachable memory

	<--- low hunk used

	client and server low hunk allocations

	<-- low hunk reset point held by host

	startup hunk allocations

	Zone block

	----- Bottom of Memory -----
*/
///@{

typedef struct memzone_s memzone_t;

void Memory_Init (void *buf, int size);

void Z_ClearZone (memzone_t *zone, int size, int zone_offset, int ele_size);
void Z_Free (memzone_t *zone, void *ptr);
void *Z_Malloc (memzone_t *zone, int size);			// returns 0 filled memory
void *Z_TagMalloc (memzone_t *zone, int size, int tag);
void *Z_Realloc (memzone_t *zone, void *ptr, int size);
void Z_Print (memzone_t *zone);
void Z_CheckHeap (memzone_t *zone);
void Z_SetError (memzone_t *zone, void (*err) (void *data, const char *msg),
				 void *data);
void Z_CheckPointer (const memzone_t *zone, const void *ptr, int size);

void *Hunk_Alloc (int size);		// returns 0 filled memory
void *Hunk_AllocName (int size, const char *name);
int	Hunk_LowMark (void) __attribute__((pure));
void Hunk_FreeToLowMark (int mark);
void *Hunk_TempAlloc (int size);
void Hunk_Check (void);



struct cache_user_s;
typedef void *(*cache_allocator_t) (struct cache_user_s *c, int size, const char *name);
typedef void (*cache_loader_t) (void *object, cache_allocator_t allocator);
typedef struct cache_user_s {
	void	*data;
	void	*object;
	cache_loader_t loader;
} cache_user_t;

void Cache_Flush (void);
void *Cache_Check (cache_user_t *c);
// returns the cached data, and moves to the head of the LRU list
// if present, otherwise returns NULL

void Cache_Free (cache_user_t *c);
void *Cache_Alloc (cache_user_t *c, int size, const char *name);
// Returns NULL if all purgable data was tossed and there still
// wasn't enough room.
void Cache_Report (void);
void Cache_Add (cache_user_t *c, void *object, cache_loader_t loader);
void Cache_Remove (cache_user_t *c);
void *Cache_TryGet (cache_user_t *c);
void *Cache_Get (cache_user_t *c);
void Cache_Release (cache_user_t *c);
int Cache_ReadLock (cache_user_t *c) __attribute__((pure));

///@}

#endif//__QF_zone_h
