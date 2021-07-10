/*
    sw32_graph.c

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

#define NH_DEFINE
#include "namehack.h"

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

/*
	R_LineGraph

	Called by only R_DisplayTime
*/
void
sw32_R_LineGraph (int x, int y, int *h_vals, int count)
{
	int         h, i, s, color;

	// FIXME: disable on no-buffer adapters, or put in the driver
	s = r_graphheight->int_val;

	while (count--) {
		h = *h_vals++;

		if (h == 10000)
			color = 0x6f;					// yellow
		else if (h == 9999)
			color = 0x4f;					// red
		else if (h == 9998)
			color = 0xd0;					// blue
		else
			color = 0xff;					// pink

		if (h > s)
			h = s;

		switch(sw32_ctx->pixbytes) {
			case 1:
				{
					byte *dest = (byte *) vid.buffer + vid.rowbytes * y + x;
					for (i = 0; i < h; i++, dest -= vid.rowbytes)
						*dest = color;
				}
				break;
			case 2:
				{
					short *dest = (short *) vid.buffer +
								  (vid.rowbytes >> 1) * y + x;
					color = sw32_8to16table[color];
					for (i = 0; i < h; i++, dest -= (vid.rowbytes >> 1))
						*dest = color;
				}
				break;
			case 4:
				{
					int *dest = (int *) vid.buffer +
								(vid.rowbytes >> 2) * y + x;
					color = d_8to24table[color];
					for (i = 0; i < h; i++, dest -= (vid.rowbytes >> 2))
						*dest = color;
				}
			break;
			default:
				Sys_Error("R_LineGraph: unsupported r_pixbytes %i",
						  sw32_ctx->pixbytes);
		}
	}
}
