/*
	d_zpoint.c

	software driver module for drawing z-buffered points

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

#include "QF/sys.h"

#include "d_local.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"


void
sw32_D_DrawZPoint (void)
{
	short      *pz;
	int         izi;

	pz = sw32_d_pzbuffer + (sw32_d_zwidth * sw32_r_zpointdesc.v) + sw32_r_zpointdesc.u;
	izi = (int) (sw32_r_zpointdesc.zi * 0x8000);

	if (*pz <= izi) {
		*pz = izi;
		switch(sw32_ctx->pixbytes)
		{
		case 1:
			((byte *) sw32_d_viewbuffer) [sw32_d_scantable[sw32_r_zpointdesc.v] +
									 sw32_r_zpointdesc.u] = sw32_r_zpointdesc.color;
			break;
		case 2:
			((short *) sw32_d_viewbuffer) [sw32_d_scantable[sw32_r_zpointdesc.v] +
									  sw32_r_zpointdesc.u] =
				sw32_8to16table[sw32_r_zpointdesc.color];
			break;
		case 4:
			((int *) sw32_d_viewbuffer) [sw32_d_scantable[sw32_r_zpointdesc.v] +
									sw32_r_zpointdesc.u] =
				d_8to24table[sw32_r_zpointdesc.color];
			break;
		default:
			Sys_Error("D_DrawZPoint: unsupported r_pixbytes %i", sw32_ctx->pixbytes);
		}
	}
}
