/*
	d_init.c

	rasterization driver initialization

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

#include "QF/cvar.h"
#include "QF/render.h"

#include "compat.h"
#include "d_local.h"
#include "r_internal.h"
#include "vid_internal.h"

#define NUM_MIPS	4

surfcache_t *d_initial_rover;
qboolean     d_roverwrapped;
int          d_minmip;
float        d_scalemip[NUM_MIPS - 1];

static float basemip[NUM_MIPS - 1] = { 1.0, 0.5 * 0.8, 0.25 * 0.8 };


void        (*d_drawspans) (espan_t *pspan);


void
D_Init (void)
{
	r_drawpolys = false;
	r_worldpolysbacktofront = false;
	r_recursiveaffinetriangles = true;

	viddef_t   *vid = vr_data.vid;

	vid->vid_internal->flush_caches = D_FlushCaches;

	int         buffersize = vid->rowbytes * vid->height;
	int         zbuffersize = vid->width * vid->height * sizeof (*vid->zbuffer);
	int         cachesize = D_SurfaceCacheForRes (vid->width, vid->height);

	if (vid->zbuffer) {
		free (vid->zbuffer);
		vid->zbuffer = 0;
	}
	if (vid->surfcache) {
		D_FlushCaches (0);
		free (vid->surfcache);
		vid->surfcache = 0;
	}
	if (vid->vid_internal->init_buffers) {
		vid->vid_internal->init_buffers (vid->vid_internal->data);
	} else {
		free (vid->buffer);
		vid->buffer = calloc (buffersize, 1);
	}
	vid->zbuffer = calloc (zbuffersize, 1);
	vid->surfcache = calloc (cachesize, 1);

	D_InitCaches (vid->surfcache, cachesize);
}

void
D_TurnZOn (void)
{
	// not needed for software version
}

void
D_SetupFrame (void)
{
	int         i;

	if (r_dowarp)
		d_viewbuffer = r_warpbuffer;
	else
		d_viewbuffer = vid.buffer;

	if (r_dowarp)
		screenwidth = WARP_WIDTH;
	else
		screenwidth = vid.rowbytes;

	d_roverwrapped = false;
	d_initial_rover = sc_rover;

	d_minmip = bound (0, d_mipcap->value, 3);

	for (i = 0; i < (NUM_MIPS - 1); i++)
		d_scalemip[i] = basemip[i] * d_mipscale->value;

	d_drawspans = D_DrawSpans8;

	d_skyoffs = r_skytime * r_skyspeed;
}

void
D_UpdateRects (vrect_t *prect)
{
	// the software driver draws these directly to the vid buffer
}
