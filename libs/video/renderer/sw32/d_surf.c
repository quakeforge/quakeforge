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

#define NH_DEFINE
#include "namehack.h"

#include <stdlib.h>

#include "QF/qargs.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "compat.h"
#include "d_local.h"
#include "r_internal.h"

static float        surfscale;

static int          sc_size;
surfcache_t *sw32_sc_rover;
static surfcache_t *sc_base;

#define GUARDSIZE       4


void *
sw32_D_SurfaceCacheAddress (void)
{
	return sc_base;
}


int
sw32_D_SurfaceCacheForRes (int width, int height)
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

	size *= sw32_r_pixbytes;

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
sw32_D_InitCaches (void *buffer, int size)
{
	Sys_MaskPrintf (SYS_dev, "D_InitCaches: %ik surface cache\n", size/1024);

	sc_size = size - GUARDSIZE;
	sc_base = (surfcache_t *) buffer;
	sw32_sc_rover = sc_base;

	sc_base->next = NULL;
	sc_base->owner = NULL;
	sc_base->size = sc_size;

	sw32_d_pzbuffer = vid.zbuffer;

	D_ClearCacheGuard ();
}


void
sw32_D_FlushCaches (void)
{
	surfcache_t *c;

	if (!sc_base)
		return;

	for (c = sc_base; c; c = c->next) {
		if (c->owner)
			*c->owner = NULL;
	}

	sw32_sc_rover = sc_base;
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

	if ((size <= 0) || (size > (0x40000 * sw32_r_pixbytes))) //FIXME ditto
		Sys_Error ("D_SCAlloc: bad cache size %d", size);

	/* This adds the offset of data[0] in the surfcache_t struct. */
	size += field_offset (surfcache_t, data);

#define SIZE_ALIGN	(sizeof(surfcache_t*)-1)
	size = (size + SIZE_ALIGN) & ~SIZE_ALIGN;
#undef SIZE_ALIGN
	size = (size + 3) & ~3;
	if (size > sc_size)
		Sys_Error ("D_SCAlloc: %i > cache size", size);

	// if there is not size bytes after the rover, reset to the start
	wrapped_this_time = false;

	if (!sw32_sc_rover || (byte *) sw32_sc_rover - (byte *) sc_base > sc_size - size) {
		if (sw32_sc_rover) {
			wrapped_this_time = true;
		}
		sw32_sc_rover = sc_base;
	}
	// colect and free surfcache_t blocks until the rover block is large enough
	new = sw32_sc_rover;
	if (sw32_sc_rover->owner)
		*sw32_sc_rover->owner = NULL;

	while (new->size < size) {
		// free another
		sw32_sc_rover = sw32_sc_rover->next;
		if (!sw32_sc_rover)
			Sys_Error ("D_SCAlloc: hit the end of memory");
		if (sw32_sc_rover->owner)
			*sw32_sc_rover->owner = NULL;

		new->size += sw32_sc_rover->size;
		new->next = sw32_sc_rover->next;
	}

	// create a fragment out of any leftovers
	if (new->size - size > 256) {
		sw32_sc_rover = (surfcache_t *) ((byte *) new + size);
		sw32_sc_rover->size = new->size - size;
		sw32_sc_rover->next = new->next;
		sw32_sc_rover->width = 0;
		sw32_sc_rover->owner = NULL;
		new->next = sw32_sc_rover;
		new->size = size;
	} else
		sw32_sc_rover = new->next;

	new->width = width;
// DEBUG
	if (width > 0)
		new->height = (size - sizeof (*new) + sizeof (new->data)) /
			(width * sw32_r_pixbytes);

	new->owner = NULL;					// should be set properly after return

	if (sw32_d_roverwrapped) {
		if (wrapped_this_time || (sw32_sc_rover >= sw32_d_initial_rover))
			r_cache_thrash = true;
	} else if (wrapped_this_time) {
		sw32_d_roverwrapped = true;
	}

	D_CheckCacheGuard ();				// DEBUG
	return new;
}


surfcache_t *
sw32_D_CacheSurface (msurface_t *surface, int miplevel)
{
	surfcache_t *cache;

	// if the surface is animating or flashing, flush the cache
	sw32_r_drawsurf.texture = R_TextureAnimation (surface);
	sw32_r_drawsurf.lightadj[0] = d_lightstylevalue[surface->styles[0]];
	sw32_r_drawsurf.lightadj[1] = d_lightstylevalue[surface->styles[1]];
	sw32_r_drawsurf.lightadj[2] = d_lightstylevalue[surface->styles[2]];
	sw32_r_drawsurf.lightadj[3] = d_lightstylevalue[surface->styles[3]];

	// see if the cache holds apropriate data
	cache = surface->cachespots[miplevel];

	if (cache && !cache->dlight && surface->dlightframe != r_framecount
		&& cache->texture == sw32_r_drawsurf.texture
		&& cache->lightadj[0] == sw32_r_drawsurf.lightadj[0]
		&& cache->lightadj[1] == sw32_r_drawsurf.lightadj[1]
		&& cache->lightadj[2] == sw32_r_drawsurf.lightadj[2]
		&& cache->lightadj[3] == sw32_r_drawsurf.lightadj[3])
		return cache;

	// determine shape of surface
	surfscale = 1.0 / (1 << miplevel);
	sw32_r_drawsurf.surfmip = miplevel;
	sw32_r_drawsurf.surfwidth = surface->extents[0] >> miplevel;
	sw32_r_drawsurf.rowbytes = sw32_r_drawsurf.surfwidth * sw32_r_pixbytes;
	sw32_r_drawsurf.surfheight = surface->extents[1] >> miplevel;

	// allocate memory if needed
	if (!cache) {
		// if a texture just animated, don't reallocate it
		cache = D_SCAlloc (sw32_r_drawsurf.surfwidth,
						   sw32_r_drawsurf.rowbytes * sw32_r_drawsurf.surfheight);
		surface->cachespots[miplevel] = cache;
		cache->owner = &surface->cachespots[miplevel];
		cache->mipscale = surfscale;
	}

	if (surface->dlightframe == r_framecount)
		cache->dlight = 1;
	else
		cache->dlight = 0;

	sw32_r_drawsurf.surfdat = (byte *) cache->data;

	cache->texture = sw32_r_drawsurf.texture;
	cache->lightadj[0] = sw32_r_drawsurf.lightadj[0];
	cache->lightadj[1] = sw32_r_drawsurf.lightadj[1];
	cache->lightadj[2] = sw32_r_drawsurf.lightadj[2];
	cache->lightadj[3] = sw32_r_drawsurf.lightadj[3];

	// draw and light the surface texture
	sw32_r_drawsurf.surf = surface;

	sw32_c_surf++;
	sw32_R_DrawSurface ();

	return surface->cachespots[miplevel];
}
