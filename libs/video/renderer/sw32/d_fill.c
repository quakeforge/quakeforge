/*
	d_fill.c

	clears a specified rectangle to the specified color

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

#include "d_iface.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

void
sw32_D_FillRect (vrect_t *rect, int color)
{
	switch (sw32_ctx->pixbytes)
	{
	case 1:
		{
			int rx, ry, rwidth, rheight;
			byte *dest, pix;

			pix = color;

			rx = rect->x;
			ry = rect->y;
			rwidth = rect->width;
			rheight = rect->height;

			if (rx < 0) {
				rwidth += rx;
				rx = 0;
			}
			if (ry < 0) {
				rheight += ry;
				ry = 0;
			}
			if ((unsigned) (rx + rwidth) > vid.width)
				rwidth = vid.width - rx;
			if ((unsigned) (ry + rheight) > vid.height)
				rheight = vid.height - rx;

			if (rwidth < 1 || rheight < 1)
				return;

			dest = (byte *) vid.buffer + ry * vid.rowbytes + rx;

			for (ry = 0; ry < rheight; ry++)
			{
				for (rx = 0; rx < rwidth; rx++)
					dest[rx] = pix;
				dest += vid.rowbytes;
			}
		}
		break;
	case 2:
		{
			int rx, ry, rwidth, rheight;
			unsigned short *dest, pix;

			pix = sw32_8to16table[color];

			rx = rect->x;
			ry = rect->y;
			rwidth = rect->width;
			rheight = rect->height;

			if (rx < 0) {
				rwidth += rx;
				rx = 0;
			}
			if (ry < 0) {
				rheight += ry;
				ry = 0;
			}
			if ((unsigned) (rx + rwidth) > vid.width)
				rwidth = vid.width - rx;
			if ((unsigned) (ry + rheight) > vid.height)
				rheight = vid.height - rx;

			if (rwidth < 1 || rheight < 1)
				return;

			dest = (unsigned short *) vid.buffer + ry * (vid.rowbytes >> 1) +
				rx;

			for (ry = 0; ry < rheight; ry++)
			{
				for (rx = 0; rx < rwidth; rx++)
					dest[rx] = pix;
				dest += (vid.rowbytes >> 1);
			}
		}
		break;
	case 4:
		{
			int rx, ry, rwidth, rheight;
			unsigned int *dest, pix;

			pix = d_8to24table[color];

			rx = rect->x;
			ry = rect->y;
			rwidth = rect->width;
			rheight = rect->height;

			if (rx < 0) {
				rwidth += rx;
				rx = 0;
			}
			if (ry < 0) {
				rheight += ry;
				ry = 0;
			}
			if ((unsigned) (rx + rwidth) > vid.width)
				rwidth = vid.width - rx;
			if ((unsigned) (ry + rheight) > vid.height)
				rheight = vid.height - rx;

			if (rwidth < 1 || rheight < 1)
				return;

			dest = (unsigned int *) vid.buffer + ry * (vid.rowbytes >> 2) + rx;

			for (ry = 0; ry < rheight; ry++)
			{
				for (rx = 0; rx < rwidth; rx++)
					dest[rx] = pix;
				dest += (vid.rowbytes >> 2);
			}
		}
		break;
	default:
		Sys_Error("D_FillRect: unsupported r_pixbytes %i", sw32_ctx->pixbytes);
	}
}
