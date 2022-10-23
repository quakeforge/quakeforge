/*
	d_surf.c

	rasterization driver surface heap manager

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

#include <stdlib.h>

#include "QF/qargs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/scene/component.h"
#include "QF/scene/entity.h"
#include "QF/scene/scene.h"

#include "compat.h"
#include "d_local.h"
#include "r_internal.h"

float        surfscale;

static int          sc_size;
surfcache_t *sc_rover;
static surfcache_t *sc_base;

#define GUARDSIZE       4


void *
D_SurfaceCacheAddress (void)
{
	return sc_base;
}

int
D_SurfaceCacheForRes (int width, int height)
{
	int         size, pix;

	if (COM_CheckParm ("-surfcachesize")) {
		size = atoi (com_argv[COM_CheckParm ("-surfcachesize") + 1]) * 1024;
		return size;
	}

	size = SURFCACHE_SIZE_AT_320X200;

	pix = width * height;
	if (pix > 64000)
		size += (pix - 64000) * 3;

	return size;
}

static void
D_CheckCacheGuard (void)
{
	byte       *s;
	int         i;

	s = (byte *) sc_base + sc_size;
	for (i = 0; i < GUARDSIZE; i++)
		if (s[i] != (byte) i)
			Sys_Error ("D_CheckCacheGuard: failed");
}

static void
D_ClearCacheGuard (void)
{
	byte       *s;
	int         i;

	s = (byte *) sc_base + sc_size;
	for (i = 0; i < GUARDSIZE; i++)
		s[i] = (byte) i;
}

void
D_InitCaches (void *buffer, int size)
{
	Sys_MaskPrintf (SYS_vid, "D_InitCaches: %ik surface cache\n", size/1024);

	sc_size = size - GUARDSIZE;
	sc_base = (surfcache_t *) buffer;
	sc_rover = sc_base;

	sc_base->next = NULL;
	sc_base->owner = NULL;
	sc_base->size = sc_size;

	D_ClearCacheGuard ();
}

void
D_FlushCaches (void *data)
{
	surfcache_t *c;

	if (!sc_base)
		return;

	for (c = sc_base; c; c = c->next) {
		if (c->owner)
			*c->owner = NULL;
	}

	sc_rover = sc_base;
	sc_base->next = NULL;
	sc_base->owner = NULL;
	sc_base->size = sc_size;
}

static surfcache_t *
D_SCAlloc (int width, int size)
{
	surfcache_t *new;
	qboolean    wrapped_this_time;

	if ((width < 0) || (width > 512))	// FIXME shouldn't really have a max
		Sys_Error ("D_SCAlloc: bad cache width %d", width);

	if ((size <= 0) || (size > 0x40000))	// FIXME ditto
		Sys_Error ("D_SCAlloc: bad cache size %d", size);

	// This adds the offset of data[0] in the surfcache_t struct.
	size += field_offset (surfcache_t, data);

#define SIZE_ALIGN	(sizeof (surfcache_t *) - 1)
	size = (size + SIZE_ALIGN) & ~SIZE_ALIGN;
#undef SIZE_ALIGN
	size = (size + 3) & ~3;
	if (size > sc_size)
		Sys_Error ("D_SCAlloc: %i > cache size", size);

	// if there is not size bytes after the rover, reset to the start
	wrapped_this_time = false;

	if (!sc_rover || (byte *) sc_rover - (byte *) sc_base > sc_size - size) {
		if (sc_rover) {
			wrapped_this_time = true;
		}
		sc_rover = sc_base;
	}
	// colect and free surfcache_t blocks until the rover block is large enough
	new = sc_rover;
	if (sc_rover->owner)
		*sc_rover->owner = NULL;

	while (new->size < size) {
		// free another
		sc_rover = sc_rover->next;
		if (!sc_rover)
			Sys_Error ("D_SCAlloc: hit the end of memory");
		if (sc_rover->owner)
			*sc_rover->owner = NULL;

		new->size += sc_rover->size;
		new->next = sc_rover->next;
	}

	// create a fragment out of any leftovers
	if (new->size - size > 256) {
		sc_rover = (surfcache_t *) ((byte *) new + size);
		sc_rover->size = new->size - size;
		sc_rover->next = new->next;
		sc_rover->width = 0;
		sc_rover->owner = NULL;
		new->next = sc_rover;
		new->size = size;
	} else
		sc_rover = new->next;

	new->width = width;
// DEBUG
	if (width > 0)
		new->height = (size - sizeof (*new) + sizeof (new->data)) / width;

	new->owner = NULL;					// should be set properly after return

	if (d_roverwrapped) {
		if (wrapped_this_time || (sc_rover >= d_initial_rover))
			r_cache_thrash = true;
	} else if (wrapped_this_time) {
		d_roverwrapped = true;
	}

	D_CheckCacheGuard ();				// DEBUG
	return new;
}

#if 0
static void
D_SCDump (void)
{
	surfcache_t *test;

	for (test = sc_base; test; test = test->next) {
		if (test == sc_rover)
			Sys_Printf ("ROVER:\n");
		Sys_Printf ("%p : %i bytes     %i width\n", test, test->size,
					test->width);
	}
}
#endif

surfcache_t *
D_CacheSurface (entity_t ent, msurface_t *surface, int miplevel)
{
	surfcache_t *cache;
	animation_t *animation = Ent_GetComponent (ent.id, scene_animation, r_refdef.scene->reg);
	transform_t transform = Entity_Transform (ent);

	// if the surface is animating or flashing, flush the cache
	r_drawsurf.texture = R_TextureAnimation (animation, surface);
	r_drawsurf.lightadj[0] = d_lightstylevalue[surface->styles[0]];
	r_drawsurf.lightadj[1] = d_lightstylevalue[surface->styles[1]];
	r_drawsurf.lightadj[2] = d_lightstylevalue[surface->styles[2]];
	r_drawsurf.lightadj[3] = d_lightstylevalue[surface->styles[3]];

	// see if the cache holds apropriate data
	cache = surface->cachespots[miplevel];

	if (cache && !cache->dlight && surface->dlightframe != r_framecount
		&& cache->texture == r_drawsurf.texture
		&& cache->lightadj[0] == r_drawsurf.lightadj[0]
		&& cache->lightadj[1] == r_drawsurf.lightadj[1]
		&& cache->lightadj[2] == r_drawsurf.lightadj[2]
		&& cache->lightadj[3] == r_drawsurf.lightadj[3])
		return cache;

	// determine shape of surface
	surfscale = 1.0 / (1 << miplevel);
	r_drawsurf.surfmip = miplevel;
	r_drawsurf.surfwidth = surface->extents[0] >> miplevel;
	r_drawsurf.rowbytes = r_drawsurf.surfwidth;
	r_drawsurf.surfheight = surface->extents[1] >> miplevel;

	// allocate memory if needed
	if (!cache) {
		// if a texture just animated, don't reallocate it
		cache = D_SCAlloc (r_drawsurf.surfwidth,
						   r_drawsurf.surfwidth * r_drawsurf.surfheight);
		surface->cachespots[miplevel] = cache;
		cache->owner = &surface->cachespots[miplevel];
		cache->mipscale = surfscale;
	}

	if (surface->dlightframe == r_framecount)
		cache->dlight = 1;
	else
		cache->dlight = 0;

	r_drawsurf.surfdat = (byte *) cache->data;

	cache->texture = r_drawsurf.texture;
	cache->lightadj[0] = r_drawsurf.lightadj[0];
	cache->lightadj[1] = r_drawsurf.lightadj[1];
	cache->lightadj[2] = r_drawsurf.lightadj[2];
	cache->lightadj[3] = r_drawsurf.lightadj[3];

	// draw and light the surface texture
	r_drawsurf.surf = surface;

	c_surf++;
	R_DrawSurface (transform);

	return surface->cachespots[miplevel];
}
