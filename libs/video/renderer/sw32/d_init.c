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

#define NH_DEFINE
#include "namehack.h"

#include "QF/cvar.h"
#include "QF/render.h"

#include "compat.h"
#include "d_local.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

#define NUM_MIPS	4

surfcache_t *sw32_d_initial_rover;
qboolean     sw32_d_roverwrapped;
int          sw32_d_minmip;
float        sw32_d_scalemip[NUM_MIPS - 1];

static float basemip[NUM_MIPS - 1] = { 1.0, 0.5 * 0.8, 0.25 * 0.8 };


float        sw32_d_zitable[65536];


void
sw32_D_Init (void)
{
	sw32_r_drawpolys = false;
	sw32_r_worldpolysbacktofront = false;

	// LordHavoc: compute 1/zi table for use in rendering code everywhere
	if (!sw32_d_zitable[1]) {
		int i;
		sw32_d_zitable[0] = 0;
		for (i = 1;i < 65536;i++)
			sw32_d_zitable[i] = (65536.0 * 65536.0 / (double) i);
	}

	vr_data.vid->vid_internal->surf_cache_size = sw32_D_SurfaceCacheForRes;
	vr_data.vid->vid_internal->flush_caches = sw32_D_FlushCaches;
	vr_data.vid->vid_internal->init_caches = sw32_D_InitCaches;

	VID_InitBuffers ();
	VID_MakeColormaps();
}

void
sw32_D_TurnZOn (void)
{
	// not needed for software version
}

void
sw32_D_SetupFrame (void)
{
	int         i;

	if (sw32_r_dowarp)
		sw32_d_viewbuffer = sw32_r_warpbuffer;
	else
		sw32_d_viewbuffer = vid.buffer;

	if (sw32_r_dowarp)
		sw32_screenwidth = WARP_WIDTH;
	else
		sw32_screenwidth = vid.rowbytes / sw32_ctx->pixbytes;

	sw32_d_roverwrapped = false;
	sw32_d_initial_rover = sw32_sc_rover;

	sw32_d_minmip = bound (0, d_mipcap->value, 3);

	for (i = 0; i < (NUM_MIPS - 1); i++)
		sw32_d_scalemip[i] = basemip[i] * d_mipscale->value;
}

void
sw32_D_UpdateRects (vrect_t *prect)
{
	// the software driver draws these directly to the vid buffer
}
