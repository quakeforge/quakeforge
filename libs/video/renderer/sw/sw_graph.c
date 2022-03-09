/*
	sw_graph.c

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

#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/render.h"

#include "r_internal.h"

/*
	R_LineGraph

	Called by only R_DisplayTime
*/
void
R_LineGraph (int x, int y, int *h_vals, int count, int height)
{
	int         h, i, s, color;
	byte       *dest;

	// FIXME: disable on no-buffer adapters, or put in the driver
	s = height;

	while (count--) {
		dest = ((byte*)vid.buffer) + vid.rowbytes * y + x++;

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

		for (i = 0; i < h; i++, dest -= vid.rowbytes) {
			dest[0] = color;
		}
	}
}
