/*
	d_modech.c

	called when mode has just changed

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

#include "QF/render.h"

#include "d_local.h"
#include "r_internal.h"
#include "vid_sw.h"

int         sw32_d_vrectx, sw32_d_vrecty, sw32_d_vrectright_particle, sw32_d_vrectbottom_particle;

int         sw32_d_y_aspect_shift, sw32_d_pix_min, sw32_d_pix_max, sw32_d_pix_shift;

int         sw32_d_scantable[MAXHEIGHT];
short      *sw32_zspantable[MAXHEIGHT];


static void
D_Patch (void)
{
}

void
sw32_D_ViewChanged (void)
{
	int         rowpixels;

	if (sw32_r_dowarp)
		rowpixels = WARP_WIDTH;
	else
		rowpixels = vid.rowbytes / sw32_ctx->pixbytes;

	sw32_scale_for_mip = sw32_xscale;
	if (sw32_yscale > sw32_xscale)
		sw32_scale_for_mip = sw32_yscale;

	sw32_d_zrowbytes = vid.width * 2;
	sw32_d_zwidth = vid.width;

	sw32_d_pix_min = r_refdef.vrect.width / 320;
	if (sw32_d_pix_min < 1)
		sw32_d_pix_min = 1;

	sw32_d_pix_max = (int) ((float) r_refdef.vrect.width / (320.0 / 4.0) + 0.5);
	sw32_d_pix_shift = 8 - (int) ((float) r_refdef.vrect.width / 320.0 + 0.5);
	if (sw32_d_pix_max < 1)
		sw32_d_pix_max = 1;

	if (sw32_pixelAspect > 1.4)
		sw32_d_y_aspect_shift = 1;
	else
		sw32_d_y_aspect_shift = 0;

	sw32_d_vrectx = r_refdef.vrect.x;
	sw32_d_vrecty = r_refdef.vrect.y;
	sw32_d_vrectright_particle = r_refdef.vrectright - sw32_d_pix_max;
	sw32_d_vrectbottom_particle =
		r_refdef.vrectbottom - (sw32_d_pix_max << sw32_d_y_aspect_shift);

	{
		unsigned    i;

		for (i = 0; i < vid.height; i++) {
			sw32_d_scantable[i] = i * rowpixels;
			sw32_zspantable[i] = sw32_d_pzbuffer + i * sw32_d_zwidth;
		}
	}

	D_Patch ();
}
